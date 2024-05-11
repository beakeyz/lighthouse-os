#ifndef __ANIVA_USB_EHCI_SPEC__
#define __ANIVA_USB_EHCI_SPEC__

#include <libk/stddef.h>

#define EHCI_HCD_PORTSMAX 15

/* The EHCI USBBASE is on BAR0 */
#define EHCI_PCI_USBBASE_BARNUM 0

/* 1 byte */
#define EHCI_PCI_SBRN 0x60
/* 1 byte */
#define EHCI_PCI_FLADJ 0x61
/* 2 byte */
#define EHCI_PCI_PORTWAKECAP 0x62

/* 1 bytes */
#define EHCI_HCCR_CAPLENGTH 0x00
/* 2 bytes */
#define EHCI_HCCR_HCIVERSION 0x02
/* 4 bytes */
#define EHCI_HCCR_HCSPARAMS 0x04
/* 4 bytes */
#define EHCI_HCCR_HCCPARAMS 0x08
/* 8 bytes */
#define EHCI_HCCR_HCSP_PORTROUTE 0x0C

#define EHCI_LEGSUP_CAPID_MASK 0xff
#define EHCI_LEGSUP_CAPID 0x01
#define EHCI_LEGSUP_OSOWNED (1 << 24)
#define EHCI_LEGSUP_BIOSOWNED (1 << 16)

#define EHCI_GETPARAM(p, b) ((p) & (b))
#define EHCI_GETPARAM_VALUE(p, b, o) (EHCI_GETPARAM(p, b) >> (o))

/*
 * HCSPARAMS structural parameters
 */
#define EHCI_HCSPARAMS_DBG_PORTN_OFFSET 20
#define EHCI_HCSPARAMS_DBG_PORTN (0x0f << EHCI_HCSPARAMS_DBG_PORTN_OFFSET)
#define EHCI_HCSPARAMS_P_INDICATOR (1 << 16)
#define EHCI_HCSPARAMS_N_CC_OFFSET 12
#define EHCI_HCSPARAMS_N_CC (0x0f << EHCI_HCSPARAMS_N_CC_OFFSET)
#define EHCI_HCSPARAMS_N_PCC_OFFSET 8
#define EHCI_HCSPARAMS_N_PCC (0x0f << EHCI_HCSPARAMS_N_PCC_OFFSET)
#define EHCI_HCSPARAMS_PORT_ROUTING_RULES (1 << 7)
#define EHCI_HCSPARAMS_PPC (1 << 4)
#define EHCI_HCSPARAMS_N_PORTS_OFFSET 0
#define EHCI_HCSPARAMS_N_PORTS (0x0f)

/*
 * HCCPARAMS Capability parameters
 */
#define EHCI_HCCPARAMS_EECP_OFFSET 8
#define EHCI_HCCPARAMS_EECP ((0xff << EHCI_HCCPARAMS_EECP_OFFSET))
#define EHCI_HCCPARAMS_ISOCH_SCHED_THRESH_OFFSET 4
#define EHCI_HCCPARAMS_ISOCH_SCHED_THRESH ((0x0f << EHCI_HCCPARAMS_ISOCH_SCHED_THRESH_OFFSET))
#define EHCI_HCCPARAMS_ASYNC_SCHED_PARK (1 << 2)
#define EHCI_HCCPARAMS_PROG_FRAMELIST (1 << 1)
#define EHCI_HCCPARAMS_64BIT (1 << 0)

/*
 * EHCI Opregs
 */
#define EHCI_OPREG_USBCMD 0x00
  #define EHCI_OPREG_USBCMD_INTER_CTL_MASK 0xff
  #define EHCI_OPREG_USBCMD_INTER_CTL_SHIFT 16
  #define EHCI_OPREG_USBCMD_INTER_CTL(p) ((p) << EHCI_OPREG_USBCMD_INTER_CTL_SHIFT)
/* Asynchronous Schedule Park Mode */
  #define EHCI_OPREG_USBCMD_ASPM_ENABLE (1 << 11)
  #define EHCI_OPREG_USBCMD_ASPM_COUNT (0x3 << 8)
/* Light Host Controller reset */
  #define EHCI_OPREG_USBCMD_LHC_RESET (1 << 7)
  #define EHCI_OPREG_USBCMD_INT_ON_ASYNC_ADVANCE_DB (1 << 6)
  #define EHCI_OPREG_USBCMD_ASYNC_SCHEDULE_ENABLE (1 << 5)
  #define EHCI_OPREG_USBCMD_PERIODIC_SCHEDULE_ENABLE (1 << 4)
  #define EHCI_OPREG_USBCMD_FRAMELIST_SIZE_MASK 0x03
  #define EHCI_OPREG_USBCMD_FRAMELIST_SIZE_SHIFT 2
  #define EHCI_OPREG_USBCMD_FRAMELIST_SIZE(p) ((p) << EHCI_OPREG_USBCMD_FRAMELIST_SIZE_SHIFT
  #define EHCI_OPREG_USBCMD_PPCEE (1 << 15)
/* Host Controller reset */
  #define EHCI_OPREG_USBCMD_HC_RESET (1 << 1)
/* Run/Stop */
  #define EHCI_OPREG_USBCMD_RS (1 << 0)
#define EHCI_OPREG_USBSTS 0x04
  #define EHCI_OPREG_USBSTS_USBINT (1 << 0)
  #define EHCI_OPREG_USBSTS_USBERRINT (1 << 1)
  #define EHCI_OPREG_USBSTS_PORTCHANGE (1 << 2)
  #define EHCI_OPREG_USBSTS_FLROLLOVER (1 << 3)
  #define EHCI_OPREG_USBSTS_HOSTSYSERR (1 << 4)
/* Interrupt on Async Advance */
  #define EHCI_OPREG_USBSTS_INTONAA (1 << 5)
  #define EHCI_OPREG_USBSTS_HCHALTED (1 << 12)
  #define EHCI_OPREG_USBSTS_RECLAMATION (1 << 13)
  #define EHCI_OPREG_USBSTS_PSSTATUS (1 << 14)
  #define EHCI_OPREG_USBSTS_ASSTATUS (1 << 15)
#define EHCI_OPREG_USBINTR 0x08
  #define EHCI_USBINTR_INTONAA      (1 << 5)
/* Host system error */
  #define EHCI_USBINTR_HOSTSYSERR   (1 << 4)
/* Framelist rollover */
  #define EHCI_USBINTR_FLROLLOVER   (1 << 3)
/* Port change notifications through interrupts */
  #define EHCI_USBINTR_PORTCHANGE   (1 << 2)
/* Error interrupts enable/disable */
  #define EHCI_USBINTR_USBERRINT    (1 << 1)
/* Enable/Disable */
  #define EHCI_USBINTR_USBINT       (1 << 0)
#define EHCI_OPREG_FRINDEX 0x0C
#define EHCI_OPREG_CTRLDSSEGMENT 0x10
#define EHCI_OPREG_PERIODICLISTBASE 0x14
#define EHCI_OPREG_ASYNCLISTADDR 0x18
#define EHCI_OPREG_CONFIGFLAG 0x40
#define EHCI_OPREG_PORTSC 0x44

#define EHCI_PORTSC_CONNECT (1 << 0)
#define EHCI_PORTSC_CONNECT_CHANGE (1 << 1)
#define EHCI_PORTSC_ENABLE (1 << 2)
#define EHCI_PORTSC_ENABLE_CHANGE (1 << 3)
#define EHCI_PORTSC_OVERCURRENT (1 << 4)
#define EHCI_PORTSC_OVERCURRENT_CHANGE (1 << 5)
#define EHCI_PORTSC_FPR (1 << 6)
#define EHCI_PORTSC_SUSPEND (1 << 7)
#define EHCI_PORTSC_RESET (1 << 8)
#define EHCI_PORTSC_DMINUS_LINESTAT (1 << 10)
#define EHCI_PORTSC_DPLUS_LINESTAT (1 << 11)
#define EHCI_PORTSC_PORT_POWER (1 << 12)
#define EHCI_PORTSC_PORT_OWNER (1 << 13)
#define EHCI_PORTSC_INDIC_CTL_MASK 0x03
#define EHCI_PORTSC_INDIC_CTL_SHIFT 14
#define EHCI_PORTSC_TEST_CTL_MASK 0x07
#define EHCI_PORTSC_TEST_CTL_SHIFT 16
#define EHCI_PORTSC_WAKE_CONNECT (1 << 20)
#define EHCI_PORTSC_WAKE_DISCONN (1 << 21)
#define EHCI_PORTSC_WAKE_OVERCURR (1 << 22)

#define EHCI_PORTSC_DATAMASK	0xffffffd5

enum EHCI_USBCMD_INTERRUPT_THRESHOLD_CTL {
  PER_INTERRUPT_1_MICROFRAME = 0x01,
  PER_INTERRUPT_2_MICROFRAMES = 0x02,
  PER_INTERRUPT_4_MICROFRAMES = 0x04,
  PER_INTERRUPT_8_MICROFRAMES = 0x08,
  PER_INTERRUPT_16_MICROFRAMES = 0x10,
  PER_INTERRUPT_32_MICROFRAMES = 0x20,
  PER_INTERRUPT_64_MICROFRAMES = 0x40,
};

/*
 * ECHI Frame List link pointers
 */
#define EHCI_FLLP_TYPE_END      ((1 << 0)) /* T-bit */
#define EHCI_FLLP_TYPE_ITD      ((0 << 1))
#define EHCI_FLLP_TYPE_QH       ((1 << 1))
#define EHCI_FLLP_TYPE_SITD     ((2 << 1))
#define EHCI_FLLP_TYPE_FSTN     ((3 << 1))

#define EHCI_QTD_DATA_TOGGLE	(1U << 31)
#define EHCI_QTD_BYTES_SHIFT	16
#define EHCI_QTD_BYTES_MASK		0x7fff
#define EHCI_QTD_IOC			(1 << 15)
#define EHCI_QTD_CPAGE_SHIFT	12
#define EHCI_QTD_CPAGE_MASK		0x07
#define EHCI_QTD_ERRCOUNT_SHIFT	10
#define EHCI_QTD_ERRCOUNT_MASK	0x03
#define EHCI_QTD_PID_SHIFT		8
#define EHCI_QTD_PID_MASK		0x03
#define EHCI_QTD_PID_OUT		0x00
#define EHCI_QTD_PID_IN			0x01
#define EHCI_QTD_PID_SETUP		0x02
#define EHCI_QTD_STATUS_SHIFT	0
#define EHCI_QTD_STATUS_MASK	0x7f
#define EHCI_QTD_STATUS_ERRMASK	0x50
#define EHCI_QTD_STATUS_ACTIVE	(1 << 7)
#define EHCI_QTD_STATUS_HALTED	(1 << 6)
#define EHCI_QTD_STATUS_BUFFER	(1 << 5)
#define EHCI_QTD_STATUS_BABBLE	(1 << 4)
#define EHCI_QTD_STATUS_TERROR	(1 << 3)
#define EHCI_QTD_STATUS_MISSED	(1 << 2)
#define EHCI_QTD_STATUS_SPLIT	(1 << 1)
#define EHCI_QTD_STATUS_PING	(1 << 0)
#define EHCI_QTD_STATUS_LS_ERR	(1 << 0)
#define EHCI_QTD_PAGE_MASK		0xfffff000

/*
 * EHCI Queue transfer descriptor
 */
typedef struct ehci_qtd {
  /* Hard side */
  uint32_t hw_next;
  uint32_t hw_alt_next;
  uint32_t hw_token;
  uint32_t hw_buffer[5];
  uint32_t hw_buffer_hi[5];

  /* Soft side */
  paddr_t qtd_dma_addr;
  struct ehci_qtd* next;
  struct ehci_qtd* prev;
  struct ehci_qtd* alt_next;
  void* buffer;
  size_t len;
} __attribute__((packed, aligned(32))) ehci_qtd_t;

/* qh info 0 */
#define	EHCI_QH_CONTROL_EP	(1 << 27)	/* FS/LS control endpoint */
#define	EHCI_QH_HEAD		(1 << 15)	/* Head of async reclamation list */
#define	EHCI_QH_TOGGLE_CTL	(1 << 14)	/* Data toggle control */
#define	EHCI_QH_HIGH_SPEED	(2 << 12)	/* Endpoint speed */
#define	EHCI_QH_LOW_SPEED	(1 << 12)
#define	EHCI_QH_FULL_SPEED	(0 << 12)
#define	EHCI_QH_INACTIVATE	(1 << 7)	/* Inactivate on next transaction */

#define EHCI_QH_MPL(p) (((p) << 16))
#define EHCI_QH_RLC(p) (((p) << 28))
#define EHCI_QH_EP_NUM(p) (((p) << 8))
#define EHCI_QH_DEVADDR(p) (((p) << 0))

/* qh info 1 */
#define	EHCI_QH_SMASK	0x000000ff
#define	EHCI_QH_CMASK	0x0000ff00
#define	EHCI_QH_HUBADDR_MASK	0x007f0000
#define EHCI_QH_HUBADDR(p) ((p) << 16)
#define	EHCI_QH_HUBPORT_MASK	0x3f800000
#define EHCI_QH_HUBPORT(p) ((p) << 23)
#define	EHCI_QH_MULT    0xc0000000
#define EHCI_QH_MULT_VAL(p) (((p) << 30))

typedef struct ehci_qh {
  uint32_t hw_next;
  uint32_t hw_info_0;
  uint32_t hw_info_1;
  uint32_t hw_cur_qtd;

  /* ehci_qtd hardware side */
  uint32_t hw_qtd_next;
  uint32_t hw_qtd_alt_next;
  uint32_t hw_qtd_token;
  uint32_t hw_qtd_buffer[5];
  uint32_t hw_qtd_buffer_hi[5];

  /* ehci_qh softside */
  paddr_t qh_dma;
  struct ehci_qh* next;
  struct ehci_qh* prev;
  struct ehci_qtd* qtd_link;
  struct ehci_qtd* qtd_alt;

} __attribute__((packed, aligned(32))) ehci_qh_t;

typedef struct ehci_itd {
  uint32_t hw_next;
  uint32_t hw_token[8];
  uint32_t hw_buffer[7];
  uint32_t hw_ext_buffer[7];

  struct ehci_itd* next;
  struct ehci_itd* prev;
  paddr_t itd_dma;
  vaddr_t buffer;
  size_t bufsize;

  /* Need size to be aligned to 32 bytes */
} __attribute__((packed, aligned(32))) ehci_itd_t;

typedef struct ehci_sitd {
  uint32_t hw_next;
  uint8_t hw_port_num;
  uint8_t hw_hub_addr;
  uint8_t hw_endpoint;
  uint8_t hw_devaddr;
  uint16_t hw_reserved0;
  uint8_t hw_cmask;
  uint8_t hw_smask;
  uint16_t hw_transfer_len;
  uint8_t hw_cprogmask;
  uint8_t hw_status;
  uint32_t hw_buffer[2];
  uint32_t hw_back_buf;
  uint32_t hw_ext_buffer[2];

  struct ehci_sitd* next;
  struct ehci_sitd* prev;
  paddr_t sitd_dma;
  size_t bufsize;
  vaddr_t buf;
} __attribute__((packed, aligned(32))) ehci_sitd_t;

#endif // !__ANIVA_USB_EHCI_SPEC__
