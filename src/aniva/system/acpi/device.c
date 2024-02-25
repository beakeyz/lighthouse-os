#include "acpi.h"
#include "dev/device.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "libk/flow/error.h"
#include <mem/heap.h>
#include "libk/string.h"
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/device.h"
#include "system/acpi/tables.h"

/* Raw ACPI devgroup to store the devices we find on early enumerate */
static dgroup_t* _acpi_group;

static uint64_t _cpy_ascii_str(char* dst, const char* src, size_t len)
{
  uint64_t c_dst_idx = 0;
  for (size_t i = 0; i < len; i++) {
    if ((src[i] < 'A' || src[i] > 'Z') && 
        (src[i] < 'a' || src[i] > 'z') &&
        (src[i] < '0' || src[i] > '9') &&
        (src[i] != ' '))
      continue;

    dst[c_dst_idx++] = src[i];
  }

  return c_dst_idx;
}

/*!
 * @brief: Do some magic to generate a unique device name per acpi device
 */
static const char* _generate_acpi_device_name(acpi_device_t* device, ACPI_DEVICE_INFO* info)
{
  size_t ret_len;
  size_t dhid_len;
  size_t uuid_len;
  uint32_t c_name_idx;
  char* ret;

  if (!info->UniqueId.Length)
    return device->hid;

  c_name_idx = 0;
  dhid_len = strlen(device->hid);
  uuid_len = strlen(info->UniqueId.String);

  /*         <hid>    '_'   <uuid>   '\0' */
  ret_len = dhid_len + 1 + uuid_len + 1;

  ret = kmalloc(ret_len);

  if (!ret)
    return nullptr;

  memset(ret, 0, ret_len);

  c_name_idx += _cpy_ascii_str(ret, device->hid, dhid_len);

  ret[c_name_idx++] = '_';

  _cpy_ascii_str(&ret[c_name_idx], info->UniqueId.String, uuid_len);
  return ret;
}

/*!
 * @brief: Allocate memory for an acpi device and its generic device parent
 */
static acpi_device_t* _create_acpi_device(acpi_handle_t handle, ACPI_DEVICE_INFO* info, device_ep_t* eps, uint32_t ep_count)
{
  device_t* device;
  acpi_device_t* ret;
  const char* device_name;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->handle = handle;
  ret->hid = strdup(info->HardwareId.String);

  device_name = _generate_acpi_device_name(ret, info);

  if (!device_name)
    goto dealloc_and_exit;

  /* Create the generic device */
  device = create_device_ex(NULL, (char*)device_name, ret, NULL, eps, ep_count);

  /* Make this shit fuck off */
  kfree((void*)device_name);

  if (!device)
    goto dealloc_and_exit;

  device_power_on(device);

  device_register(device, _acpi_group);
  return ret;
dealloc_and_exit:
  kfree((void*)ret->hid);
  kfree(ret);
  return nullptr;
}

kerror_t acpi_add_device(acpi_handle_t handle, int type, device_ep_t* eps, uint32_t ep_count)
{
  acpi_device_t* device;
  ACPI_DEVICE_INFO* info = nullptr;

  AcpiGetObjectInfo(handle, &info);
  if (!info) {
    printf("Failed to get object info\n");
    return AE_OK;
  }

  device = _create_acpi_device(handle, info, eps, ep_count);
  if (!device)
    return -KERR_NOMEM;

  printf (" HID: %s, ADR: %llX, Name: %s\n", device->hid, info->Address, info->UniqueId.String);

  return KERR_NONE;
}

void init_acpi_devices()
{
  _acpi_group = register_dev_group(DGROUP_TYPE_ACPI, "acpi", NULL, NULL);
}
