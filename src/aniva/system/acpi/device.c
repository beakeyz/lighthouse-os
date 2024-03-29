#include "acpi.h"
#include "dev/device.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "libk/flow/error.h"
#include <mem/heap.h>
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/aclocal.h"
#include "system/acpi/acpica/acnamesp.h"
#include "system/acpi/acpica/acpiosxf.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/acpica/acutils.h"
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
        (src[i] != ' ')) {
      dst[c_dst_idx++] = NULL;
      break;
    }

    dst[c_dst_idx++] = src[i];

    if (src[i] == '\0')
      break;
  }

  return c_dst_idx;
}

/*!
 * @brief: Do some magic to generate a unique device name per acpi device
 */
static inline const char* _generate_acpi_device_name(acpi_device_t* device, ACPI_DEVICE_INFO* info)
{
  size_t ret_len;
  size_t dhid_len;
  size_t uuid_len;
  uint32_t c_name_idx;
  char* ret;

  if (!info->UniqueId.Length)
    return strdup(device->hid);

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
static acpi_device_t* _create_acpi_device(acpi_handle_t handle, ACPI_DEVICE_INFO* info, device_ep_t* eps, uint32_t ep_count, const char* acpi_path)
{
  device_t* device;
  acpi_device_t* ret;
  const char* device_name;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->handle = handle;
  ret->hid = info->HardwareId.Length ? kmalloc(info->HardwareId.Length) : strdup(acpi_path);

  device_name = acpi_path;

  if (info->HardwareId.Length) {
    _cpy_ascii_str((char*)ret->hid, info->HardwareId.String, info->HardwareId.Length);

    device_name = _generate_acpi_device_name(ret, info);

    if (!device_name)
      goto dealloc_and_exit;
  }

  /* Create the generic device */
  device = create_device_ex(NULL, (char*)device_name, ret, NULL, eps, ep_count);

  /* Make this shit fuck off if we needed to process the hardware id field */
  if (info->HardwareId.Length)
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

static inline kerror_t _get_acpidev_info(acpi_handle_t handle, ACPI_DEVICE_INFO* infobuf)
{
  ACPI_PNP_DEVICE_ID* hid;
  ACPI_PNP_DEVICE_ID* uid;
  ACPI_NAMESPACE_NODE *node;

  if (ACPI_FAILURE(AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE)))
    return -KERR_INVAL;

  node = AcpiNsValidateHandle(handle);

  (void) AcpiUtReleaseMutex(ACPI_MTX_NAMESPACE);

  if (!node)
    return -KERR_INVAL;

  if (node->Type != ACPI_TYPE_DEVICE && node->Type != ACPI_TYPE_PROCESSOR)
    return -KERR_INVAL;

  if (ACPI_FAILURE(AcpiUtExecute_HID(node, &hid)))
    hid = nullptr;

  if (ACPI_FAILURE(AcpiUtExecute_UID(node, &uid)))
    uid = nullptr;

  if (!hid)
    return -KERR_INVAL;

  memcpy(&infobuf->HardwareId, hid, sizeof(*hid));
  ACPI_FREE(hid);

  /* Uid is optional */
  if (uid) {
    memcpy(&infobuf->UniqueId, uid, sizeof(*uid));
    ACPI_FREE(uid);
  }

  return 0;
}

kerror_t acpi_add_device(acpi_handle_t handle, int type, device_ep_t* eps, uint32_t ep_count, const char* acpi_path)
{
  acpi_device_t* device;
  ACPI_DEVICE_INFO info = { 0 };

  /*
   * (NOTE/FIXME): We manually parse methods on the device, since AcpiGetObjectInfo gives us trouble
   * on real metal =/
   *
   * Also FIXME: We're leaking memory through _get_acpidev_info allocating hid and (sometimes) uid strings
   * which we never free
   */

  _get_acpidev_info(handle, &info);

  device = _create_acpi_device(handle, &info, eps, ep_count, acpi_path);

  if (!device)
    return -KERR_NOMEM;

  printf (" HID: %s, Name: %s\n", device->hid, info.UniqueId.String);

  return KERR_NONE;
}

void init_acpi_devices()
{
  _acpi_group = register_dev_group(DGROUP_TYPE_ACPI, "acpi", NULL, NULL);
}
