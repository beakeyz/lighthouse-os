#include "multiboot.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include <libk/stddef.h>

#define MB_MOD_PROBE_LOOPLIM 32
#define MB_TAG_PROBE_LOOPLIM 128

ErrorOrPtr init_multiboot(void* addr)
{
    size_t delta;
    vaddr_t virt_mod_end;
    struct multiboot_tag_mmap* mb_memmap;
    struct multiboot_tag_module* mb_current_module;

    mb_memmap = get_mb2_tag(addr, 6);

    /* This would be mega bad lmao */
    if (!mb_memmap) {
        KLOG_ERR("No memorymap found! aborted\n");
        return Error();
    }

    /* NOTE: addr is virtual */
    g_system_info.total_multiboot_size = ALIGN_UP(get_total_mb2_size(addr) + 8, SMALL_PAGE_SIZE);
    g_system_info.post_kernel_reserved_size = NULL;

    mb_current_module = get_mb2_tag(addr, MULTIBOOT_TAG_TYPE_MODULE);

    while (mb_current_module) {
        virt_mod_end = kmem_ensure_high_mapping(mb_current_module->mod_end);

        /* If this is above the kernel */
        if (virt_mod_end > g_system_info.kernel_end_addr) {
            /* Whats the total size between the kernel end and the module end? */
            delta = virt_mod_end - g_system_info.kernel_end_addr;

            /* If this difference is more than what we currently have registered, set this */
            if (delta > g_system_info.post_kernel_reserved_size)
                /* Plus a page as a buffer =) */
                g_system_info.post_kernel_reserved_size = ALIGN_UP(delta, SMALL_PAGE_SIZE);
        }

        /*
         * Cycle to the next 'module' (even though we will probably only ever load one, namely the ramdisk which
         * won't get particularly big)
         */
        mb_current_module = next_mb2_tag((uint8_t*)ALIGN_UP((uintptr_t)mb_current_module + mb_current_module->size, 8), MULTIBOOT_TAG_TYPE_MODULE);
    }

    return Success(0);
}

/*
 * Called after kmem_manager is set up, since we need to have access to the physical
 * allocator here. This function will preserve any essensial multiboot memory so that
 * it does not get overwritten by any allocations
 */
ErrorOrPtr finalize_multiboot()
{
    const size_t mb_pagecount = GET_PAGECOUNT(g_system_info.phys_multiboot_addr, g_system_info.total_multiboot_size);
    const paddr_t aligned_mb_start = ALIGN_DOWN(g_system_info.phys_multiboot_addr, SMALL_PAGE_SIZE);
    const uintptr_t mb_start_idx = kmem_get_page_idx(aligned_mb_start);

    KLOG_DBG("Setting multiboot range used at idx %lld with a size of %llx\n", mb_start_idx, mb_pagecount);

    /* Reserve multiboot memory */
    kmem_set_phys_range_used(mb_start_idx, mb_pagecount);

    // g_system_info.virt_multiboot_addr = Must(__kmem_alloc_ex(nullptr, nullptr, aligned_mb_start, HIGH_MAP_BASE, g_system_info.total_multiboot_size, NULL, KMEM_FLAG_KERNEL));

    struct multiboot_tag_module* mod = g_system_info.ramdisk;

    /* Mark the ramdisk 'module' memory as used */
    if (mod && mod->mod_start && mod->mod_end && mod->mod_end > mod->mod_start) {
        const paddr_t module_start_idx = kmem_get_page_idx(ALIGN_DOWN(mod->mod_start, SMALL_PAGE_SIZE));
        const paddr_t module_end_idx = kmem_get_page_idx(ALIGN_UP(mod->mod_end, SMALL_PAGE_SIZE));

        const size_t module_size_pages = module_end_idx - module_start_idx;

        kmem_set_phys_range_used(module_start_idx, module_size_pages);
    }

    struct multiboot_tag_framebuffer* fbuffer = g_system_info.firmware_fb;

    /* Mark the framebuffer memory as used */
    if (fbuffer && fbuffer->common.framebuffer_addr) {

        uintptr_t phys_fb_start_idx = kmem_get_page_idx(ALIGN_DOWN(fbuffer->common.framebuffer_addr, SMALL_PAGE_SIZE));
        uintptr_t phys_fb_page_count = kmem_get_page_idx(ALIGN_UP(fbuffer->common.framebuffer_pitch * fbuffer->common.framebuffer_height, SMALL_PAGE_SIZE));

        KLOG_DBG("Marking framebuffer as used! (start_page: 0x%llx, page_count: 0x%llx)\n", phys_fb_start_idx, phys_fb_page_count);

        kmem_set_phys_range_used(phys_fb_start_idx, phys_fb_page_count);
    }

    return Success(0);
}

size_t get_total_mb2_size(void* start_addr)
{
    size_t ret = 0;
    void* idxPtr = ((void*)start_addr) + 8;
    struct multiboot_tag* tag = idxPtr;
    // loop through the tags to find the right type
    while (true) {
        if (tag->type == 0)
            break;

        ret += tag->size;

        idxPtr += tag->size;
        idxPtr = (void*)ALIGN_UP((uintptr_t)idxPtr, 8);
        tag = idxPtr;
    }

    return ret;
}

void* next_mb2_tag(void* cur, uint32_t type)
{
    uintptr_t idxPtr = (uintptr_t)cur;
    struct multiboot_tag* tag = (void*)idxPtr;
    // loop through the tags to find the right type
    while (true) {
        if (tag->type == type)
            return tag;
        if (tag->type == 0)
            return nullptr;

        idxPtr += tag->size;

        idxPtr = ALIGN_UP(idxPtr, 8);
        tag = (void*)idxPtr;
    }
}

void* get_mb2_tag(void* addr, uint32_t type)
{
    if (!addr)
        return nullptr;

    char* header = ((void*)addr) + 8;
    return next_mb2_tag(header, type);
}
