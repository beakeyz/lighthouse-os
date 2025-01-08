#ifndef __ANIVA_USB_EHCI_BUFFER_H__
#define __ANIVA_USB_EHCI_BUFFER_H__

#include "libk/data/bitmap.h"

struct ehci_qtd;

/*
 * Scratchpad buffer implementation for the aniva EHCI driver
 *
 * The purpose of this is to be able to quickly access buffer pages without having to go through the
 * big and clunky physical allocator (kmem).
 */

struct ehci_scratch_entry {
    /* Virtual kernel mapping of this entry */
    vaddr_t v;
    /* Physical address of this entry */
    paddr_t p;
};

#define EHCI_SCRATCH_BUFFER_MIN_CAPACITY 64
#define EHCI_SCRATCH_BUFFER_INVAL_IDX ((u32) - 1)

typedef struct ehci_scratch_buffer {
    /* Capacity of this buffer (default = 64) */
    u32 dw_capacity;
    /* Some flags (currently unused 4-bit field) */
    u32 dw_flags;
    /* The bitmap that tells us which pages are free */
    bitmap_t* bm_freemap;
    /* Buffer that contains virtual addresses of the scratch pages */
    struct ehci_scratch_entry* scratch_pages;
    /* Link to an optional backup scratch buffer in case this guy gets full */
    struct ehci_scratch_buffer* next;
} ehci_scratch_buffer_t;

error_t init_ehci_scratch_buffer(ehci_scratch_buffer_t* buffer, u32 capacity, u32 flags);
ehci_scratch_buffer_t* create_ehci_scratch_buffer(u32 capacity, u32 flags);
void cleanup_ehci_scratch_buffer(ehci_scratch_buffer_t* buffer);
void destroy_ehci_scratch_buffer(ehci_scratch_buffer_t* buffer);

error_t ehci_scratch_buffer_alloc(ehci_scratch_buffer_t* buffer, struct ehci_scratch_entry* pentry, u32* p_ndx);
error_t ehci_scratch_buffer_dealloc(ehci_scratch_buffer_t* buffer, u32 ndx);
error_t ehci_scratch_buffer_getentry(ehci_scratch_buffer_t* buffer, u32 ndx, struct ehci_scratch_entry* pentry);

error_t ehci_scratch_buffer_write_qtd(ehci_scratch_buffer_t* buffer, struct ehci_qtd* qtd, void* buf, size_t bsize);
error_t ehci_scratch_buffer_read_qtd(ehci_scratch_buffer_t* buffer, struct ehci_qtd* qtd, void* buf, size_t bsize);

#endif // !__ANIVA_USB_EHCI_BUFFER_H__
