#include "phys.h"
#include "entry/entry.h"
#include "libk/data/bitmap.h"
#include "libk/data/linkedlist.h"
#include "libk/multiboot.h"
#include "lightos/types.h"
#include "mem/kmem.h"
#include "mem/tracker/mtracker.h"
#include "sync/spinlock.h"

static struct {
    uint32_t m_mmap_entry_num;

    /* Raw mmap entries */
    multiboot_memory_map_t* m_mmap_entries;

    uintptr_t m_highest_phys_page_base;
    size_t m_phys_pages_count;
    size_t m_total_avail_memory_bytes;
    size_t m_total_unavail_memory_bytes;

    /* TODO: Replace this with the physical tracker */
    bitmap_t* m_phys_bitmap;

    /* Tracker 0: The physical page tracker =D */
    page_tracker_t m_physical_tracker;

    /* Lock around the physical allocation */
    spinlock_t m_allocator_lock;

    /* List of physical ranges retrieved by parsing the memory map */
    list_t* m_phys_ranges;
} g_phys_data;

/*!
 * @brief: Prepare kmem for using the memory map
 */
error_t prep_mmap(struct multiboot_tag_mmap* mmap)
{
    if (!mmap)
        return -ENULL;

    g_phys_data.m_mmap_entry_num = (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size;
    g_phys_data.m_mmap_entries = (struct multiboot_mmap_entry*)mmap->entries;

    return 0;
}

static kmem_range_t* _create_kmem_range(multiboot_memory_map_t* mb_mmap_entry)
{
    paddr_t range_end;
    kmem_range_t* range;

    range = kmalloc(sizeof(kmem_range_t));

    /* Unlikely */
    if (!range)
        return nullptr;

    range->start = mb_mmap_entry->addr;
    range->length = mb_mmap_entry->len;

    switch (mb_mmap_entry->type) {
    case (MULTIBOOT_MEMORY_AVAILABLE): {
        range->type = MEMTYPE_USABLE;
        break;
    }
    case (MULTIBOOT_MEMORY_ACPI_RECLAIMABLE): {
        range->type = MEMTYPE_ACPI_RECLAIM;
        break;
    }
    case (MULTIBOOT_MEMORY_NVS): {
        range->type = MEMTYPE_ACPI_NVS;
        break;
    }
    case (MULTIBOOT_MEMORY_RESERVED): {
        range->type = MEMTYPE_RESERVED;
        break;
    }
    case (MULTIBOOT_MEMORY_BADRAM): {
        range->type = MEMTYPE_BAD_MEM;
        break;
    }
    default: {
        range->type = MEMTYPE_UNKNOWN;
        break;
    }
    }

    list_append(g_phys_data.m_phys_ranges, range);

    /* DEBUG */
    KLOG_DBG("mmap range (type=%d) addr=0x%llx len=0x%llx\n", range->type, range->start, range->length);

    range_end = ALIGN_DOWN(range->start + range->length, SMALL_PAGE_SIZE);

    if (range_end > g_phys_data.m_highest_phys_page_base)
        g_phys_data.m_highest_phys_page_base = range_end;

    if (range->type != MEMTYPE_USABLE)
        goto exit_and_return;

    range->start = ALIGN_DOWN(range->start, SMALL_PAGE_SIZE);
    range->length = ALIGN_UP(range->length, SMALL_PAGE_SIZE);

    g_phys_data.m_total_avail_memory_bytes += range->length;

exit_and_return:
    return range;
}

static kmem_range_t* _create_kmem_range_raw(enum KMEM_MEMORY_TYPE type, paddr_t start, size_t length)
{
    kmem_range_t* range;

    range = kmalloc(sizeof(*range));

    if (!range)
        return nullptr;

    range->type = type;
    range->start = start;
    range->length = length;

    return range;
}

static bool _kmem_ranges_overlap(kmem_range_t* range1, kmem_range_t* range2)
{
    if (!range1 || !range2)
        return false;

    return (
        /* @range2 ends inside @range1 */
        ((range2->start + range2->length) > range1->start && (range2->start + range2->length) <= (range1->start + range1->length)) ||
        /* @range2 starts inside @range2 */
        (range2->start >= range1->start && range2->start < (range1->start + range1->length) && (range2->start + range2->length) >= (range1->start + range1->length)));
}

static inline void _kmem_create_and_add_range(list_t* list, enum KMEM_MEMORY_TYPE type, paddr_t start, size_t length)
{
    kmem_range_t* range = _create_kmem_range_raw(
        type,
        start,
        length);

    list_append(list, range);
}

static void _kmem_merge_ranges(kmem_range_t* reserved)
{
    list_t* new_list;
    kmem_range_t* c_range;
    paddr_t new_start;
    size_t new_length;
    enum KMEM_MEMORY_TYPE new_type;

    new_list = init_list();

    FOREACH(i, g_phys_data.m_phys_ranges)
    {
        c_range = i->data;

        /*
         * Now we only have to figure out what part of the current range we need to add to the new list
         */

        new_type = c_range->type;
        new_start = c_range->start;
        new_length = c_range->length;

        if (!_kmem_ranges_overlap(c_range, reserved)) {
            _kmem_create_and_add_range(
                new_list,
                new_type,
                new_start,
                new_length);
            continue;
        }

        /* We have a conflicting reserved region, fuck */

        /* Reserved range starts inside this range */
        if (reserved->start > new_start) {
            new_length = (reserved->start - new_start);

            _kmem_create_and_add_range(
                new_list,
                new_type,
                new_start,
                new_length);
        }

        new_start = reserved->start;
        new_length = (reserved->start + reserved->length) < (c_range->start + c_range->length) ? reserved->length : (c_range->start + c_range->length) - reserved->start;
        new_type = reserved->type;

        if (new_length)
            _kmem_create_and_add_range(
                new_list,
                new_type,
                new_start,
                new_length);

        new_start = reserved->start + reserved->length;
        new_length = (reserved->start + reserved->length) < (c_range->start + c_range->length) ? (c_range->start + c_range->length) - new_start : 0;
        new_type = c_range->type;

        if (!new_length)
            continue;

        _kmem_create_and_add_range(
            new_list,
            new_type,
            new_start,
            new_length);
    }

    FOREACH(i, g_phys_data.m_phys_ranges)
    {
        kfree(i->data);
    }

    destroy_list(g_phys_data.m_phys_ranges);

    g_phys_data.m_phys_ranges = new_list;
}

static void kmem_parse_mmap(void)
{
    paddr_t _p_kernel_start;
    paddr_t _p_kernel_end;
    kmem_range_t* range;

    _p_kernel_start = (uintptr_t)&_kernel_start & ~HIGH_MAP_BASE;
    _p_kernel_end = ((uintptr_t)&_kernel_end & ~HIGH_MAP_BASE) + g_system_info.post_kernel_reserved_size;

    /*
     * These are the static memory regions that we need to reserve in order for the system to boot
     */
    kmem_range_t reserved_ranges[] = {
        { MEMTYPE_KERNEL_RESERVED, 0, 1 * Mib },
        { MEMTYPE_KERNEL, _p_kernel_start, _p_kernel_end },
    };

    g_phys_data.m_phys_pages_count = 0;
    g_phys_data.m_highest_phys_page_base = 0;

    for (uintptr_t i = 0; i < g_phys_data.m_mmap_entry_num; i++) {
        multiboot_memory_map_t* map = &g_phys_data.m_mmap_entries[i];

        range = _create_kmem_range(map);

        ASSERT_MSG(range, "Failed to create kmem range!");

        /* At this point skip non-usable ranges */
        if (range->type != MEMTYPE_USABLE)
            continue;
    }

    for (uint32_t i = 0; i < arrlen(reserved_ranges); i++)
        _kmem_merge_ranges(&reserved_ranges[i]);

    g_phys_data.m_phys_pages_count = GET_PAGECOUNT(0, g_phys_data.m_total_avail_memory_bytes);

    KLOG_DBG("Total contiguous pages: %lld\n", g_phys_data.m_phys_pages_count);
}

/*!
 * @brief: Finds a free physical range that fits @size bytes
 *
 * When a usable range is found, we truncate the range in the physical range list and return a
 * dummy range. This range won't be included in the total physical range list, but that won't be
 * needed, as we will only look at ranges marked as usable. This means we can get away with only
 * resizing the range we steal bytes from
 */
static int _allocate_free_physical_range(kmem_range_t* range, size_t size)
{
    size_t mapped_size;
    kmem_range_t* c_range;
    kmem_range_t* selected_range;

    mapped_size = NULL;
    selected_range = NULL;

    /* Make sure we're allocating on page boundries */
    size = ALIGN_UP(size, SMALL_PAGE_SIZE);

    KLOG_DBG("Looking for %lld bytes (kmem_get_early_map_size()=%lld)\n", size, kmem_get_early_map_size());

    FOREACH(i, g_phys_data.m_phys_ranges)
    {
        c_range = i->data;

        /* Only look for usable ranges */
        if (c_range->type != MEMTYPE_USABLE)
            continue;

        /* Can't use any ranges that aren't mapped */
        if (c_range->start >= kmem_get_early_map_size())
            continue;

        if (c_range->length < size)
            continue;

        /* Assume we're mapped */
        mapped_size = c_range->length;

        /* If the range is only partially mapped, we need to figure out if we can fit into the mapped part */
        if ((c_range->start + c_range->length) > kmem_get_early_map_size())
            mapped_size = kmem_get_early_map_size() - c_range->start;

        /* Get the stuff we need */
        selected_range = c_range;

        /* Check if we fit */
        if (size < mapped_size)
            break;

        /* Fuck, reset and go next */
        selected_range = NULL;
    }

    if (!selected_range)
        return -1;

    /* Create a new dummy range */
    range->start = selected_range->start;
    range->length = size;
    range->type = MEMTYPE_KERNEL_RESERVED;

    selected_range->start += size;
    selected_range->length -= size;

    return 0;
}

static int __init_physical_tracker()
{
    kmem_range_t bm_range;
    bitmap_t* physical_bitmap;
    vaddr_t bitmap_start_addr;
    size_t physical_bitmap_size;
    size_t physical_pages;
    size_t physical_pages_bytes;

    physical_pages = kmem_get_page_idx(g_phys_data.m_highest_phys_page_base) + 1;
    physical_pages_bytes = (ALIGN_UP(physical_pages, 8) >> 3);
    physical_bitmap_size = physical_pages_bytes + sizeof(bitmap_t);

    /* Find a bitmap. This is crucial */
    ASSERT_MSG(_allocate_free_physical_range(&bm_range, physical_bitmap_size) == NULL, "Failed to find a physical range for our physmem bitmap!");

    /* Compute where our bitmap MUST go */
    bitmap_start_addr = kmem_ensure_high_mapping(bm_range.start);

    /*
     * new FIXME: we allocate a bitmap for the entire memory page space on the heap,
     * which means that on systems with a lot of RAM, we need a biiig heap to contain
     * that. That's an issue, because we need to reserve that space right now in the form of
     * a static memory space that gets embedded into the kernel binary, because in order
     * for us to have a dynamic heap like this, we'll need a working physical allocator,
     * but for that we also need a dynamic heap...
     * The solution would be to map enough memory in the boot stub to try to find a suitable
     * location for the bitmap to fit manualy and then initialize the heap after that.
     */
    physical_bitmap = (bitmap_t*)(bitmap_start_addr);

    physical_bitmap->m_size = physical_pages_bytes;
    physical_bitmap->m_entries = physical_pages;
    physical_bitmap->m_default = 0xff;

    KLOG_DBG("Trying to initialize our bitmap at 0x%llx with %lld entries\n", bitmap_start_addr, physical_bitmap->m_entries);

    memset(physical_bitmap->m_map, physical_bitmap->m_default, physical_pages_bytes);

    g_phys_data.m_phys_bitmap = physical_bitmap;

    paddr_t base;
    size_t size;

    // Mark the contiguous 'free' ranges as free in our bitmap
    FOREACH(i, g_phys_data.m_phys_ranges)
    {
        const kmem_range_t* range = i->data;

        if (range->type != MEMTYPE_USABLE)
            continue;

        base = range->start;
        size = range->length;

        KLOG_DBG("Marking free range: start=0x%llx, size=0x%llx\n", base, size);

        /* Base and size should already be page-aligned */
        kmem_phys_dealloc_range(
            kmem_get_page_idx(base),
            GET_PAGECOUNT(base, size));
    }
    return 0;
}

size_t kmem_phys_get_total_bytecount()
{
    return g_phys_data.m_total_avail_memory_bytes;
}

kmem_range_t* kmem_phys_get_range(u32 idx)
{
    return list_get(g_phys_data.m_phys_ranges, idx);
}

u32 kmem_phys_get_nr_ranges()
{
    return g_phys_data.m_phys_ranges->m_length;
}

bool kmem_phys_is_page_used(uintptr_t idx)
{
    return bitmap_isset(g_phys_data.m_phys_bitmap, idx);
}

/*!
 * @brief: Allocates a single physical page
 */
error_t kmem_phys_alloc_page(u64* p_page_idx)
{
    return kmem_phys_alloc_range(1, p_page_idx);
}

/*!
 * @brief: Deallocates a single page
 */
error_t kmem_phys_dealloc_page(u64 page_idx)
{
    if (!g_phys_data.m_phys_bitmap)
        return -ENULL;

    spinlock_lock(&g_phys_data.m_allocator_lock);

    /* Mark the entry used */
    bitmap_unmark(g_phys_data.m_phys_bitmap, page_idx);

    spinlock_unlock(&g_phys_data.m_allocator_lock);

    return 0;
}

/*!
 * @brief: Reserves a specific page range
 *
 * Used by drivers to prevent certain parts of mmio memory to be allocated
 * for generic use
 */
error_t kmem_phys_reserve_range(u64 page_idx, u32 nr_pages)
{
    if (!g_phys_data.m_phys_bitmap)
        return -ENULL;

    if (!nr_pages)
        return -EINVAL;

    spinlock_lock(&g_phys_data.m_allocator_lock);

    /* Mark the entry used */
    bitmap_mark_range(g_phys_data.m_phys_bitmap, page_idx, nr_pages);

    spinlock_unlock(&g_phys_data.m_allocator_lock);

    return 0;
}

/*!
 * @brief: Allocates a contiguous range of physical memory
 */
error_t kmem_phys_alloc_range(u32 nr_pages, u64* p_page_idx)
{
    error_t error;
    u64 result = NULL;

    if (!p_page_idx || !nr_pages)
        return -EINVAL;

    spinlock_lock(&g_phys_data.m_allocator_lock);

    /* Find a free range */
    error = bitmap_find_free_range(g_phys_data.m_phys_bitmap, nr_pages, &result);

    if (error)
        goto unlock_and_exit;

    /* Mark the range used */
    bitmap_mark_range(g_phys_data.m_phys_bitmap, result, nr_pages);

unlock_and_exit:
    spinlock_unlock(&g_phys_data.m_allocator_lock);

    *p_page_idx = result;

    return error;
}

/*!
 * @brief: Deallocates a contiguous range of physical memory
 */
error_t kmem_phys_dealloc_range(u64 page_idx, u32 nr_pages)
{
    if (!nr_pages)
        return -EINVAL;

    spinlock_lock(&g_phys_data.m_allocator_lock);

    /* Mark the range used */
    bitmap_unmark_range(g_phys_data.m_phys_bitmap, page_idx, nr_pages);

    spinlock_unlock(&g_phys_data.m_allocator_lock);

    return 0;
}

int init_kmem_phys(u64* mb_addr)
{
    /* Clear this buffer */
    memset(&g_phys_data, 0, sizeof(g_phys_data));

    /* Initialize the physical ranges list */
    g_phys_data.m_phys_ranges = init_list();

    /* Initialize the allocator spinlock */
    init_spinlock(&g_phys_data.m_allocator_lock, NULL);

    /* Prepare the memory map (just calculate values lol) */
    if (prep_mmap(get_mb2_tag((void*)mb_addr, 6)))
        return -ENULL;

    /*
     * Try to parse the memory map given to us by the bootloader
     * This creates the final list of page ranges that need to be
     * reserved by the physical allocator
     */
    kmem_parse_mmap();

    if (__init_physical_tracker())
        return -ENOMEM;

    return 0;
}

/*!
 * @brief: Initialize the physical allocator further, after the
 *         page structures have been loaded.
 *
 */
int init_kmem_phys_late()
{
    return 0;
}
