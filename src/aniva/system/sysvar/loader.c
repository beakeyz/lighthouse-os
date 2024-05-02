#include "loader.h"
#include "fs/file.h"
#include "mem/heap.h"
#include <libk/string.h>
#include "oss/node.h"
#include "sync/mutex.h"
#include "system/sysvar/map.h"
#include "system/sysvar/var.h"

static inline bool is_header_valid(pvr_file_header_t* hdr)
{
  return (
      hdr->sign[0] == SYSVAR_SIG_0 && 
      hdr->sign[1] == SYSVAR_SIG_1 &&
      hdr->sign[2] == SYSVAR_SIG_2 &&
      hdr->sign[3] == SYSVAR_SIG_3
  );
}

static inline uintptr_t get_abs_strtab_entry_offset(pvr_file_header_t* hdr, uint32_t strtab_entry_off)
{
  return (hdr->strtab_offset + sizeof(pvr_file_strtab_t) + strtab_entry_off);
}

static inline uintptr_t get_abs_valtab_entry_offset(pvr_file_header_t* hdr, uint32_t valtab_entry_off)
{
  return (hdr->valtab_offset + sizeof(pvr_file_valtab_t) + valtab_entry_off);
}


int sysvarldr_load_variables(struct oss_node* node, uint16_t priv_lvl, struct file* file)
{
  const char* c_key;
  uintptr_t c_val;
  uintptr_t c_var_offset;
  sysvar_t* loaded_var;
  pvr_file_var_t c_var = { 0 };
  pvr_file_header_t hdr = { 0 };

  file_read(file, &hdr, sizeof(hdr), 0);

  if (!is_header_valid(&hdr))
    return -1;

  c_var_offset = sizeof(hdr);

  /* Fully lock the map during this opperation */
  mutex_lock(node->lock);

  for (uint32_t i = 0; i < hdr.var_count; i++) {
    file_read(file, &c_var, sizeof(c_var), c_var_offset);

    pvr_file_valtab_entry_t c_val_entry;
    uint8_t* key_buffer = kmalloc(c_var.key_len);
    uint8_t* val_buffer = kmalloc(c_var.val_len);

    loaded_var = nullptr;

    printf("Var of type %d with value offset (%d)\n", c_var.var_type, c_var.value_off);

    /* Get the key string */
    file_read(file, key_buffer, c_var.key_len, get_abs_strtab_entry_offset(&hdr, c_var.key_off));

    /* Get the value entry */
    file_read(file, &c_val_entry, sizeof(c_val_entry), get_abs_valtab_entry_offset(&hdr, c_var.value_off));

    /* Copy the correct value */
    if (c_var.var_type == SYSVAR_TYPE_STRING)
      file_read(file, val_buffer, c_var.val_len, get_abs_strtab_entry_offset(&hdr, c_val_entry.value));
    else
      memcpy(val_buffer, &c_val_entry.value, sizeof(c_val_entry.value));

    /* Key gets strduped by create_sysvar */
    c_key = (const char*)key_buffer;

    if (c_var.var_type == SYSVAR_TYPE_STRING)
      /* Value also lololol */
      c_val = (uintptr_t)val_buffer;
    else
      memcpy(&c_val, val_buffer, c_var.val_len);

    loaded_var = sysvar_get(node, c_key);

    /* Do we already have this variable? */
    if (loaded_var != nullptr) {
      /* Overrride */
      sysvar_write(loaded_var, c_val);

      /* Release the reference made by sysvar_map_get */
      release_sysvar(loaded_var);
      goto cycle;
    }

    /* Put the variable */
    (void)sysvar_attach(node,
        c_key, 
        priv_lvl,
        c_var.var_type, 
        c_var.var_flags, 
        c_val);

cycle:
    kfree(val_buffer);
    kfree(key_buffer);
    c_var_offset += sizeof(c_var);
  }

  mutex_unlock(node->lock);
  return 0;
}
