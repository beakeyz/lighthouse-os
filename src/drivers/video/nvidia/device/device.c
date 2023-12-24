#include "device.h"
#include "dev/pci/pci.h"
#include "dev/video/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "drivers/video/nvidia/device/subdev/therm/core.h"
#include "drivers/video/nvidia/device/subdev/fuse/core.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"

/*
 * Entrypoints for a card of the NV94 chipset
 */
static nv_subsys_entry_t nv94_entries[NV_SUBDEV_COUNT] = {
  [NV_SUBDEV_FUSE] = nv50_fuse_create,
  [NV_SUBDEV_THERM] = g84_therm_create,
  0
};

static int nv_detect_chip(nv_device_t* nvdev)
{
  uint32_t pmc_ident;

  /* Read the nvidia identify bits */
  pmc_ident = nvd_rd32(nvdev, 0x00);

  if ((pmc_ident & 0x1f000000) > 0) {
    /* Top bits are the chipset */
    nvdev->chipset = (pmc_ident & 0x1ff00000) >> 20;
    /* Bottom bits are the revision */
    nvdev->chiprev = (pmc_ident & 0x000000ff);

    /* Determine card-type based on chipset */
    switch (nvdev->chipset & 0x1f0) {
      case 0x010:
        kernel_panic("TODO: nvdev->chipset & 0x1f0 == 0x010");
        break;
      case 0x020: nvdev->card_type = NV_20; break;
      case 0x030: nvdev->card_type = NV_30; break;
      case 0x040:
      case 0x060: nvdev->card_type = NV_40; break;
      case 0x050:
      case 0x080:
      case 0x090:
      case 0x0a0: nvdev->card_type = NV_50; break;
      case 0x0c0:
      case 0x0d0: nvdev->card_type = NV_C0; break;
      case 0x0e0:
      case 0x0f0:
      case 0x100: nvdev->card_type = NV_E0; break;
      case 0x110:
      case 0x120: nvdev->card_type = GM100; break;
      case 0x130: nvdev->card_type = GP100; break;
      case 0x140: nvdev->card_type = GV100; break;
      case 0x160: nvdev->card_type = TU100; break;
      case 0x170: nvdev->card_type = GA100; break;
      case 0x190: nvdev->card_type = AD100; break;
    }
  } else {
    kernel_panic("TODO: nvidia: handle weird chipset stuff =)");
  }

  /* TODO: With the chipset, we can also determine the functions needed for certain card subsystem management */
  switch (nvdev->chipset) {
    case 0x094: nvdev->subsys_entries = nv94_entries;
  }
  /* TODO: With the entries found, loop over them all and call them */

  return 0;
}

static int nv_pci_device_init(nv_device_t* nvdev)
{
  int error;
  uint32_t bar0;
  size_t nv_bar0_size;
  uintptr_t nv_bar0;

  /* Map BAR 0 */
  nvdev->pdevice->ops.read_dword(nvdev->pdevice, BAR0, (uint32_t*)&bar0);

  nv_bar0_size = pci_get_bar_size(nvdev->pdevice, 0);
  nv_bar0 = get_bar_address(bar0);

  nvdev->pri_size = ALIGN_UP(nv_bar0_size, SMALL_PAGE_SIZE);
  nvdev->pri = (void*)Must(__kmem_kernel_alloc(nv_bar0, nvdev->pri_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));

  /* Did we actually map? */
  if (!nvdev->pri)
    return -1;

  /* Find chipset */
  error = nv_detect_chip(nvdev);

  if (error)
    return error;

  /*
   * Execute all the subsystem entries
   */
  for (uint32_t i = 0; i < NV_SUBDEV_COUNT; i++) {
    if (!nvdev->subsys_entries[i])
      continue;

    error = nvdev->subsys_entries[i](nvdev, i, (void**)&nvdev->subdevices[i]);

    if (error)
      goto dealloc_and_exit;
  }

  /*
   * Initialize all subdevices
   */
  for (uint32_t i = 0; i < NV_SUBDEV_COUNT; i++) {
    if (!nvdev->subdevices[i])
      continue;

    error = nv_subdev_init(nvdev->subdevices[i]);

    /* FIXME: check if the error is fatal */
    if (error)
      goto dealloc_and_exit;
  }

  /* Find card type */
  /* Find bios stuff */
  /* Find other stuff... */

  return 0;

dealloc_and_exit:
  /* TODO dealloc */
  return error;
}

/*!
 * @brief: Do main nvidia device allocation
 *
 * This shit simply sets up a few allocations so we can have an interface to the aniva video core
 * and so we have a baseline device foundation to build forward from
 */
nv_device_t* create_nv_device(aniva_driver_t* driver, pci_device_t* pdev)
{
  int error;
  nv_device_t* nvd;

  nvd = kmalloc(sizeof(*nvd));

  if (!nvd)
    return nullptr;

  /* Make sure it's up and running */
  pci_device_enable(pdev);

  /* TODO: find a device name based on the PCI info */
  nvd->id.device = pdev->dev_id;
  nvd->id.vendor = pdev->vendor_id;

  /* TODO: nvidia video ops */
  nvd->vdev = create_video_device(driver, NULL);
  nvd->pdevice = pdev;

  /* Perform main device initialization */
  error = nv_pci_device_init(nvd);

  if (error)
    goto error_and_exit;

  return nvd;

error_and_exit:
  pci_device_disable(pdev);

  destroy_video_device(nvd->vdev);
  kfree(nvd);

  return nullptr;
}

/*!
 * @brief: Get a specific subdevice for a device @device of a subdev type @type
 */
nv_subdev_t* nvdev_get_subdev(nv_device_t* device, enum NV_SUBDEV_TYPE type)
{
  if (type == NV_SUBDEV_COUNT)
    return nullptr;

  return device->subdevices[type];
}

