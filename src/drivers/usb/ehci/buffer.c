#include "buffer.h"
#include "drivers/usb/ehci/ehci_spec.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "libk/math/math.h"
#include "mem/heap.h"
#include "mem/kmem.h"
#include "sys/types.h"

error_t init_ehci_scratch_buffer(ehci_scratch_buffer_t* buffer, u32 capacity, u32 flags)
{
    /* Make sure we're at the minimum  */
    if (capacity < EHCI_SCRATCH_BUFFER_MIN_CAPACITY)
        capacity = EHCI_SCRATCH_BUFFER_MIN_CAPACITY;

    buffer->dw_capacity = capacity;
    buffer->dw_flags = flags;

    /* Create the bitmap for this buffer */
    buffer->bm_freemap = create_bitmap_ex(capacity, 0x00);

    /* Fuck */
    if (!buffer->bm_freemap)
        return -ENOMEM;

    /* Try to allocate an array of scratch pages */
    buffer->scratch_pages = kmalloc(capacity * sizeof(struct ehci_scratch_entry));

    /* Fuck */
    if (!buffer->scratch_pages) {
        destroy_bitmap(buffer->bm_freemap);
        return -ENOMEM;
    }

    /* Fill the scratch array */
    for (u32 i = 0; i < capacity; i++) {
        ASSERT(IS_OK(kmem_kernel_alloc_range((void**)&buffer->scratch_pages[i].v, SMALL_PAGE_SIZE, NULL, KMEM_FLAG_DMA)));

        /* Retrieve the physical address as well */
        buffer->scratch_pages[i].p = kmem_to_phys(nullptr, buffer->scratch_pages[i].v);
    }

    return 0;
}

ehci_scratch_buffer_t* create_ehci_scratch_buffer(u32 capacity, u32 flags)
{
    ehci_scratch_buffer_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    /* Try to init */
    if (IS_OK(init_ehci_scratch_buffer(ret, capacity, flags)))
        return ret;

    /* Init failed. Let's cry very hard */
    kfree(ret);
    return nullptr;
}

/*!
 * @brief: Destroys the entire buffer obj and its child objects
 */
void destroy_ehci_scratch_buffer(ehci_scratch_buffer_t* buffer)
{
    cleanup_ehci_scratch_buffer(buffer);

    kfree(buffer);
}

/*!
 * @brief: Cleans up the memory objects owned by the buffer object
 */
void cleanup_ehci_scratch_buffer(ehci_scratch_buffer_t* buffer)
{
    /* Deallocate the scratch pages */
    for (u32 i = 0; i < buffer->dw_capacity; i++)
        ASSERT(IS_OK(kmem_kernel_dealloc(buffer->scratch_pages[i].v, SMALL_PAGE_SIZE)));

    kfree(buffer->scratch_pages);
    destroy_bitmap(buffer->bm_freemap);
}

error_t ehci_scratch_buffer_alloc(ehci_scratch_buffer_t* buffer, struct ehci_scratch_entry* pentry, u32* p_ndx)
{
    u64 ndx = 0;

    if (!buffer || !pentry || !p_ndx)
        return -EINVAL;

    /* Try to find a free entry */
    if (bitmap_find_free(buffer->bm_freemap, &ndx))
        return -ENOENT;

    /* Mark the entry */
    bitmap_mark(buffer->bm_freemap, ndx);

    /* Copy out the buffer entry */
    *pentry = buffer->scratch_pages[ndx];
    *p_ndx = ndx;

    return 0;
}

error_t ehci_scratch_buffer_dealloc(ehci_scratch_buffer_t* buffer, u32 ndx)
{
    if (ndx >= buffer->dw_capacity)
        return -EINVAL;

    bitmap_unmark(buffer->bm_freemap, ndx);

    return 0;
}

error_t ehci_scratch_buffer_getentry(ehci_scratch_buffer_t* buffer, u32 ndx, struct ehci_scratch_entry* pentry)
{
    if (ndx >= buffer->dw_capacity)
        return -ERANGE;

    /* Check if this is an allocated entry */
    if (!bitmap_isset(buffer->bm_freemap, ndx))
        return -ENOENT;

    /* Expor the entry */
    *pentry = buffer->scratch_pages[ndx];

    return 0;
}

error_t ehci_scratch_buffer_write_qtd(ehci_scratch_buffer_t* buffer, ehci_qtd_t* qtd, void* buf, size_t bsize)
{
    uintptr_t cur_boffset = 0;
    size_t c_wr_sz;
    struct ehci_scratch_entry entry;

    for (u32 i = 0; i < arrlen(qtd->ndcs); i++) {
        if (!bsize)
            break;

        if (ehci_scratch_buffer_getentry(buffer, qtd->ndcs[i], &entry))
            return -ENOENT;

        /* Get the minima of how big a single scratch buffer can be and how much we want to write */
        c_wr_sz = MIN(SMALL_PAGE_SIZE, bsize);

        memcpy((void*)entry.v, buf + cur_boffset, c_wr_sz);

        /* Adjust the offsets */
        bsize -= c_wr_sz;
        cur_boffset += c_wr_sz;
    }

    return 0;
}

error_t ehci_scratch_buffer_read_qtd(ehci_scratch_buffer_t* buffer, struct ehci_qtd* qtd, void* buf, size_t bsize)
{
    uintptr_t cur_boffset = 0;
    size_t c_wr_sz;
    struct ehci_scratch_entry entry;

    for (u32 i = 0; i < arrlen(qtd->ndcs); i++) {
        if (!bsize)
            break;

        if (ehci_scratch_buffer_getentry(buffer, qtd->ndcs[i], &entry))
            return -ENOENT;

        /* Get the minima of how big a single scratch buffer can be and how much we want to write */
        c_wr_sz = MIN(SMALL_PAGE_SIZE, bsize);

        /* Copy out the entries contents into the buffer */
        memcpy(buf + cur_boffset, (void*)entry.v, c_wr_sz);

        /* Adjust the offsets */
        bsize -= c_wr_sz;
        cur_boffset += c_wr_sz;
    }

    return 0;
}
