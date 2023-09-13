#ifndef __ANIVA_XHCI_EXTENDED__
#define __ANIVA_XHCI_EXTENDED__

#define XHCI_HCC_EXT_CAPS(p) (((p) >> 16) & 0xFFFF)
#define XHCI_MAX_EXT_CAPS 50

#define XHCI_EXT_CAP_GETID(p) ((p) & 0xff)
#define XHCI_EXT_CAP_NEXT(p) (((p) >> 8) & 0xff)
#define XHCI_EXT_CAP_VALUE(p) ((p) >> 16)

/* Extended capability ids */
#define XHCI_EXT_CAPS_LEGACY 1
#define XHCI_EXT_CAPS_PROTOCOL 2
#define XHCI_EXT_CAPS_PM 3
#define XHCI_EXT_CAPS_VIRT 4
#define XHCI_EXT_CAPS_ROUTE 5

#define XHCI_HC_BIOS_OWNED (1<<16)
#define XHCI_HC_OS_OWNED (1<<24)

#define XHCI_LEGACY_SUPP_OFF (0x00)
#define XHCI_LEGACY_CTL_OFF (0x04)

#define XHCI_LEGACY_DISABLE_SMI ((0x7 << 1) + (0xff << 5) + (0x7 << 17))
#define XHCI_LEGACY_SMI_EVENTS (0x07 << 29)

#endif // !__ANIVA_XHCI_EXTENDED__
