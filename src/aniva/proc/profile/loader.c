#include <libk/stddef.h>
#include "entry/entry.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "proc/profile/profile.h"
#include "proc/profile/variable.h"

#include <lightos/proc/var_types.h>

/*
 * profile variable loader for Aniva
 *
 * A few things TODO:
 *  - Implement hashing into these files. This speeds up search times when there are a lot of linked
 *    tables in a single .pvr file. NOTE: We can also cache the save location of all variables.
 */

static inline bool is_header_valid(pvr_file_header_t* hdr)
{
  return (
      hdr->sign[0] == PVR_SIG_0 && 
      hdr->sign[1] == PVR_SIG_1 &&
      hdr->sign[2] == PVR_SIG_2 &&
      hdr->sign[3] == PVR_SIG_3
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

/*!
 * @brief: Save @profile to a file
 *
 * Caller should ensure this file is usable and does not contain any sensitive data,
 * since we don't check the current contents of this file.
 * TODO: implement
 */
int profile_save(proc_profile_t* profile, file_t* file)
{
  kernel_panic("TODO: profile_save");
  return -1;
}

int profile_load(proc_profile_t** profile, file_t* file)
{
  kernel_panic("TODO: profile_load");
  return 0;
}

static inline bool can_save_to_file(file_t* file)
{
  return (
      file && 
      file->m_ops && 
      file->m_ops->f_write && 
      (file->m_flags & FILE_READONLY) != FILE_READONLY
  );
}

/*!
 * @brief: Save the variables of @profile
 *
 * TODO: implement
 *
 * Before we do anything, we need to check if we can actually write to this file
 * If the file already contains a valid .pvr context, we need to try to keep as much intact as we can
 * If the file is empty or invalid, just clear it and create a new context
 */
int profile_save_variables(proc_profile_t* profile, file_t* file)
{
  if (!can_save_to_file(file))
    return -1;

  kernel_panic("TODO: profile_save_variables");
  return 0;
}

/*!
 * @brief: Load the variables in @file into @profiles variable store
 */
int profile_load_variables(proc_profile_t* profile, file_t* file)
{
  const char* c_key;
  uintptr_t c_val;
  uintptr_t c_var_offset;
  profile_var_t* loaded_var;
  pvr_file_var_t c_var = { 0 };
  pvr_file_header_t hdr = { 0 };

  file_read(file, &hdr, sizeof(hdr), 0);

  if (!is_header_valid(&hdr))
    return -1;

  c_var_offset = sizeof(hdr);

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
    if (c_var.var_type == PROFILE_VAR_TYPE_STRING)
      file_read(file, val_buffer, c_var.val_len, get_abs_strtab_entry_offset(&hdr, c_val_entry.value));
    else
      memcpy(val_buffer, &c_val_entry.value, sizeof(c_val_entry.value));

    c_key = strdup((const char*)key_buffer);

    if (c_var.var_type == PROFILE_VAR_TYPE_STRING)
      c_val = (uintptr_t)strdup((const char*)val_buffer);
    else
      memcpy(&c_val, val_buffer, c_var.val_len);

    /* Do we already have this variable? */
    if (profile_get_var(profile, c_key, &loaded_var) == 0 && loaded_var != nullptr) {
      /* Overrride */
      profile_var_write(loaded_var, c_val);

      /* Clean up and go next */
      kfree((void*)c_key);
      goto cycle;
    }

    loaded_var = create_profile_var(
        c_key, 
        c_var.var_type, 
        c_var.var_flags, 
        c_val);

    profile_add_var(profile, loaded_var);

cycle:
    kfree(val_buffer);
    kfree(key_buffer);
    c_var_offset += sizeof(c_var);
  }

  return 0;
}

/*!
 * @brief: Try to find a free spot in the vartab, strtab and valtab and save this var there
 * 
 * If there is no space left in one of these tables, we create a new one and link them into the tables
 */
int profile_save_var(proc_profile_t* profile, profile_var_t* var, file_t* file)
{
  return 0;
}

int profile_load_var(proc_profile_t* profile, const char* key, file_t* file)
{
  return 0;
}
