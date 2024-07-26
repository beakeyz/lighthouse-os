#include "dev/core.h"
#include "dev/endpoint.h"
#include "dev/io/hid/event.h"
#include "dev/manifest.h"
#include "drivers/input/i8042/i8042.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "lightos/event/key.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include <dev/device.h>
#include <dev/driver.h>
#include <dev/io/hid/hid.h>

/*
 * This is okay right now, but once we create a HID device
 * for PS2, these variables need to be moved inside a device obj
 * so this is a FIXME
 */
static hid_device_t* s_i8042_device;
static uint16_t s_mod_flags;
static uint16_t s_current_scancode;
static uint16_t s_current_scancodes[7];

registers_t* i8042_kbd_irq_handler(registers_t* regs);
registers_t* i8042_mouse_irq_handler(registers_t* regs);

/**
 * @brief: i8042 poll routine
 *
 * There is no such thing as polling on a i8042 device, since the IRQ handler dumps the events directly into the ring buffer
 */
static int i8042_poll(device_t* device)
{
    return 0;
}

struct device_hid_endpoint _i8042_hid_ep = {
    .f_poll = i8042_poll,
};

static inline uint32_t i8042_read_status()
{
    return in8(I8042_STATUS_PORT);
}

/*!
 * @brief: Wait until certain bits in the status register are set
 */
static inline bool i8042_poll_status(u8 mask)
{
    for (u64 i = 0; i < 100000UL; i++) {
        if (i8042_read_status() & mask)
            return true;
    }

    return false;
}

/*!
 * @brief: Wait until certain bits in the status register are clear
 */
static bool i8042_poll_status_clear(u8 mask)
{
    for (u64 i = 0; i < 100000UL; i++) {
        if ((i8042_read_status() & mask) == 0)
            return true;
    }

    return false;
}
static inline uint32_t i8042_read_data()
{
    i8042_poll_status(I8042_STATUS_OUTBUF_FULL);
    return in8(I8042_DATA_PORT);
}

static inline void i8042_write_data(uint8_t byte)
{
    out8(I8042_DATA_PORT, byte);
}

static inline void i8042_write_cmd(uint8_t byte)
{
    out8(I8042_CMD_PORT, byte);
}

static u8 i8042_exec_cmd(u8 byte)
{
    /* Write the command when the input buffer is empty */
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_cmd(byte);

    /* Wait until we can read  */
    return i8042_read_data();
}

static void i8042_exec_cmd_arg(u8 cmd, u8 arg)
{
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_cmd(cmd);
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_data(arg);
}

static inline u8 i8042_mouse_write(u8 byte)
{
    i8042_exec_cmd_arg(I8042_CMD_WRITE_MOUSE, byte);

    return i8042_read_data();
}

static inline kerror_t i8042_try_flush_input()
{
    u8 stat;
    u32 ticks = 0x400;

    do {
        stat = i8042_read_status() & 0x01;

        if (!stat)
            break;

        /* Flush data */
        in8(I8042_DATA_PORT);
        ticks--;
    } while (ticks);

    if (ticks)
        return 0;

    /* Ticks went all the way to zero, there might just not be
     * a i8042 controller at the end of this xD */
    return -KERR_NOT_FOUND;
}

static int i8042_enable(device_t* device)
{
    /* Disable both ports */
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_cmd(I8042_CMD_DISABLE_PORT1);
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_cmd(I8042_CMD_DISABLE_PORT2);

    if (i8042_try_flush_input())
        return -KERR_NOT_FOUND;

    u8 cfg = i8042_exec_cmd(I8042_CMD_RD_CFG);
    cfg |= (I8042_CFG_PORT1_IRQ | I8042_CFG_PORT2_IRQ);

    /* Set the new config byte */
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_cmd(I8042_CMD_WR_CFG);
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_data(cfg);

    /* Enable both ports */
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_cmd(I8042_CMD_ENABLE_PORT1);
    i8042_poll_status_clear(I8042_STATUS_INBUF_FULL);
    i8042_write_cmd(I8042_CMD_ENABLE_PORT2);

    /* Enable mouse data */
    i8042_mouse_write(MOUSE_SET_DEFAULTS);
    i8042_mouse_write(MOUSE_DATA_ON);

    return 0;
}

static int i8042_disable(device_t* device)
{
    /* Disable both ports */
    i8042_write_cmd(I8042_CMD_DISABLE_PORT1);
    i8042_write_cmd(I8042_CMD_DISABLE_PORT2);

    return 0;
}

struct device_generic_endpoint _i8042_generic_ep = {
    .f_enable = i8042_enable,
    .f_disable = i8042_disable,
};

device_ep_t i8042_eps[] = {
    DEVICE_ENDPOINT(ENDPOINT_TYPE_HID, _i8042_hid_ep),
    DEVICE_ENDPOINT(ENDPOINT_TYPE_GENERIC, _i8042_generic_ep),
    { NULL },
};

static int _init_i8042(drv_manifest_t* driver)
{
    acpi_parser_t* parser = NULL;

    get_root_acpi_parser(&parser);

    if (!parser)
        return -KERR_NODEV;

    if (!acpi_parser_is_fadt_bootflag(parser, ACPI_FADT_8042))
        return -KERR_NODEV;

    // kernel_panic("Fuck, there seems to be no i8042 present on the system =/");

    int error;
    s_mod_flags = NULL;
    s_current_scancode = NULL;

    /* Create a HID device for this bitch */
    s_i8042_device = create_hid_device(driver, "i8042", HID_BUS_TYPE_PS2, i8042_eps);

    /* Register it */
    if (!KERR_OK(register_hid_device(s_i8042_device)))
        return -1;

    /* Try to enable the device */
    if (device_enable(s_i8042_device->dev))
        return -1;

    /* Zero scancode buffer */
    memset(&s_current_scancodes, 0, sizeof(s_current_scancodes));

    /* Try to allocate an IRQ */
    error = irq_allocate(PS2_KB_IRQ_VEC, NULL, NULL, i8042_kbd_irq_handler, NULL, "PS/2 keyboard");

    if (error)
        return -1;

    /* Try to allocate an IRQ */
    error = irq_allocate(PS2_MOUSE_IRQ_VEC, NULL, NULL, i8042_mouse_irq_handler, NULL, "PS/2 mouse");

    if (error)
        return -1;

    /* Quick flush */
    //(void)i8042_read_status();
    return 0;
}

static int _exit_i8042()
{
    int error;

    if (s_i8042_device) {
        /*
         * Remove the device
         * FIXME: What happens to any handles that are held to a device? What do we do when the device is
         * being accessed while we want to destroy it?
         */
        unregister_hid_device(s_i8042_device);

        /* Disable the device */
        device_disable(s_i8042_device->dev);

        /* Destroy the fucker */
        destroy_hid_device(s_i8042_device);
    }

    error = irq_deallocate(PS2_KB_IRQ_VEC, i8042_kbd_irq_handler);

    if (error)
        return error;

    return irq_deallocate(PS2_MOUSE_IRQ_VEC, i8042_mouse_irq_handler);
}

/*!
 * @brief: Called when we try to match this driver to a device
 *
 * @returns: 0 when the probe is successfull, otherwise a kerr code
 */
static kerror_t _probe_i8042(aniva_driver_t* driver, device_t* dev)
{
    return -KERR_INVAL;
}

static void ps2_set_keycode_buffer(uint16_t keycode, bool pressed)
{
    uint32_t key_idx;

    if (pressed) {
        for (uint32_t i = 0; i < arrlen(s_current_scancodes); i++) {
            /* No duplicates */
            if (s_current_scancodes[i] == keycode)
                break;

            /* Skip taken scancodes */
            if (s_current_scancodes[i])
                continue;

            s_current_scancodes[i] = keycode;
            break;
        }

        return;
    }

    key_idx = 0;

    /* Search for our keycode */
    for (; key_idx < arrlen(s_current_scancodes); key_idx++)
        if (s_current_scancodes[key_idx] == keycode)
            break;

    /* Shift the remaining keys forwards */
    while (key_idx < arrlen(s_current_scancodes)) {

        /* If we can't, simply put a NULL */
        if ((key_idx + 1) >= arrlen(s_current_scancodes))
            s_current_scancodes[key_idx] = NULL;
        else
            s_current_scancodes[key_idx] = s_current_scancodes[key_idx + 1];

        key_idx++;
    }
}

static inline void set_flags(uint16_t* flags, uint8_t bit, bool val)
{
    if (val) {
        *flags |= bit;
    } else {
        *flags &= ~bit;
    }
}

registers_t* i8042_kbd_irq_handler(registers_t* regs)
{
    char character;
    uint16_t scan_code = ((uint16_t)in8(I8042_DATA_PORT)) | s_current_scancode;
    bool pressed = (!(scan_code & 0x80));

    /* Don't do anything if the device is not enabled */
    if (!device_is_enabled(s_i8042_device->dev))
        return nullptr;

    if (scan_code == 0x00e0) {
        /* Extended keycode */
        s_current_scancode = (scan_code << 8);
        return regs;
    }

    s_current_scancode = NULL;

    uint16_t key_code = scan_code & 0x7f;

    switch (key_code) {
    case KBD_SCANCODE_ALT:
        set_flags(&s_mod_flags, KBD_MOD_ALT, pressed);
        break;
    case KBD_SCANCODE_LSHIFT:
    case KBD_SCANCODE_RSHIFT:
        set_flags(&s_mod_flags, KBD_MOD_SHIFT, pressed);
        break;
    case KBD_SCANCODE_CTRL:
        set_flags(&s_mod_flags, KBD_MOD_CTRL, pressed);
        break;
    case KBD_SCANCODE_SUPER:
        set_flags(&s_mod_flags, KBD_MOD_SUPER, pressed);
        break;
    default:
        break;
    }

    character = NULL;

    if (key_code < 256)
        character = (s_mod_flags & KBD_MOD_SHIFT)
            ? kbd_us_shift_map[key_code]
            : kbd_us_map[key_code];

    hid_event_t event = { 0 };

    event.type = HID_EVENT_KEYPRESS;
    event.device = s_i8042_device;
    event.key.scancode = aniva_scancode_table[key_code];
    event.key.pressed_char = character;
    /* Set mod flags */
    event.key.flags = ((s_mod_flags << HID_EVENT_KEY_FLAG_MOD_BITSHIFT) & HID_EVENT_KEY_FLAG_MOD_MASK);

    if (pressed)
        event.key.flags |= HID_EVENT_KEY_FLAG_PRESSED;

    /* Buffer the keycodes */
    ps2_set_keycode_buffer(event.key.scancode, pressed);

    /* Copy the scancode buffer to the event */
    memcpy(&event.key.pressed_buffer, &s_current_scancodes, sizeof(s_current_scancodes));

    // kevent_fire("keyboard", &kb, sizeof(kb));
    hid_event_buffer_write(&s_i8042_device->device_events, &event);

    return regs;
}

static void _i8042_hanle_mouse_packet(u8 packet[4])
{
    u8 mouse_status;
    hid_event_t event = { 0 };

    mouse_status = packet[0];

    event.type = HID_EVENT_MOUSE;
    event.device = s_i8042_device;
    event.mouse.deltax = packet[1];
    event.mouse.deltay = packet[2];

    /* Check for a negative x-direction */
    if (event.mouse.deltax && (mouse_status & MOUSE_STAT_X_NEGATIVE) == MOUSE_STAT_X_NEGATIVE)
        event.mouse.deltax -= 0x100;

    /* Check for a negative y-direction */
    if (event.mouse.deltay && (mouse_status & MOUSE_STAT_Y_NEGATIVE) == MOUSE_STAT_Y_NEGATIVE)
        event.mouse.deltay -= 0x100;

    /* Check for an overflow condidion */
    if (mouse_status & (MOUSE_STAT_X_OVERFLOW | MOUSE_STAT_Y_OVERFLOW))
        event.mouse.deltax = event.mouse.deltay = 0;

    if (mouse_status & MOUSE_STAT_LBTN)
        event.mouse.flags |= HID_MOUSE_FLAG_LBTN_PRESSED;

    if (mouse_status & MOUSE_STAT_RBTN)
        event.mouse.flags |= HID_MOUSE_FLAG_RBTN_PRESSED;

    if (mouse_status & MOUSE_STAT_MBTN)
        event.mouse.flags |= HID_MOUSE_FLAG_MBTN_PRESSED;

    /* Write to the i8042 device */
    hid_event_buffer_write(&s_i8042_device->device_events, &event);
}

registers_t* i8042_mouse_irq_handler(registers_t* regs)
{
    static u8 packet[4] = { 0 };
    static u8 packet_stage = 0;

    u8 mouse_data = in8(I8042_DATA_PORT);

    switch (packet_stage) {
    case 0:
        packet[0] = mouse_data;
        break;
    case 1:
        packet[1] = mouse_data;
        break;
    case 2:
        packet[2] = mouse_data;
        break;
    case 3:
        packet[3] = mouse_data;
        break;
    }

    if (packet_stage++ < 3)
        return regs;

    /* All bytes recieved, handle the packet */
    _i8042_hanle_mouse_packet(packet);
    packet_stage = 0;

    return regs;
}

EXPORT_DRIVER(i8042) = {
    .m_name = "i8042",
    .m_descriptor = "i8042 input driver",
    .m_type = DT_IO,
    .m_version = DRIVER_VERSION(0, 0, 2),
    .f_init = _init_i8042,
    .f_exit = _exit_i8042,
    .f_probe = _probe_i8042,
};

EXPORT_DEPENDENCIES(deps) = {
    // DRV_DEP(DRV_DEPTYPE_PATH, NULL, "Root/System/"),
    DRV_DEP_END,
};
