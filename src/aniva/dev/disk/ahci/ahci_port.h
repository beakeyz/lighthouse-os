#ifndef __ANIVA_AHCIPORT__
#define __ANIVA_AHCIPORT__

#include "libk/flow/error.h"
#include <dev/disk/device.h>
#include <sync/spinlock.h>

struct ahci_device;

typedef struct ahci_port {
    struct ahci_device* m_device;

    char m_device_model[40];

    bool m_awaiting_dma_transfer_complete;
    bool m_is_waiting;
    bool m_transfer_failed;

    uintptr_t m_port_offset;

    uintptr_t m_ib_page;
    uintptr_t m_cmd_list_page;
    uintptr_t m_fis_recieve_page;

    uintptr_t m_dma_buffer;
    uintptr_t m_cmd_table_buffer;

    uint32_t m_port_index;

    volume_device_t* m_generic;
} ahci_port_t;

ahci_port_t* create_ahci_port(struct ahci_device* device, uintptr_t port_offset, uint32_t index);

void destroy_ahci_port(ahci_port_t* port);

ANIVA_STATUS initialize_port(ahci_port_t* port);
ANIVA_STATUS ahci_port_gather_info(ahci_port_t* port);

ANIVA_STATUS ahci_port_handle_int(ahci_port_t* port);

kerror_t ahci_port_find_unused_command_header(ahci_port_t* port);

#endif // !__ANIVA_AHCIPORT__
