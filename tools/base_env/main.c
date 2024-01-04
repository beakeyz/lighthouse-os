#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defaults.h"
#include "proc/var_types.h"

/* Buffers that get placed into the file */
pvr_file_header_t pvr_hdr;
static pvr_file_var_t* fvar_buffer;
static pvr_file_valtab_t* valtab_buffer;
static pvr_file_strtab_t* strtab_buffer;

/* Sizes for this buffer */
static uint32_t fvar_buffersize;
static uint32_t valtab_buffersize;
static uint32_t strtab_buffersize;

struct profile_var_template base_defaults[] = {
  VAR_ENTRY("test", PROFILE_VAR_TYPE_STRING, "Yay", 0),
  VAR_ENTRY("test_2", PROFILE_VAR_TYPE_BYTE, VAR_NUM(4), 0),
};

/*
 * Default values for the global profile
 * These include mostly just paths to drivers, kobjects, ect.
 */
struct profile_var_template global_defaults[] = {
  VAR_ENTRY("DFLT_LWND_PATH", PROFILE_VAR_TYPE_STRING, "service/lwnd", PVAR_FLAG_CONFIG),
  VAR_ENTRY("DFLT_KB_EVENT", PROFILE_VAR_TYPE_STRING, "keyboard", PVAR_FLAG_CONFIG),
  VAR_ENTRY("DFLT_ERR_EVENT", PROFILE_VAR_TYPE_STRING, "error", PVAR_FLAG_CONFIG),
  VAR_ENTRY("BOOTDISK_PATH", PROFILE_VAR_TYPE_STRING, "unknown", PVAR_FLAG_CONFIG),
};

static int pvr_file_find_free_strtab_offset(uint32_t* offset)
{
  uint32_t i;

  if (strtab_buffer->entries[0] == '\0') {
    *offset = 0;
    return 0;
  }

  for (i = 1; i < strtab_buffer->bytecount; i++) {
    if (strtab_buffer->entries[i] == '\0' && strtab_buffer->entries[i-1] == '\0')
      break;
  }

  if (i >= strtab_buffer->bytecount)
    return -1;

  *offset = i;
  return 0;
}

static int pvr_file_find_free_valtab_offset(uint32_t* offset)
{
  uint32_t i;

  for (i = 0; i < valtab_buffer->capacity; i++) {
    if (!VALTAB_ENTRY_ISUSED(&valtab_buffer->entries[i]))
      break;
  }

  if (i >= valtab_buffer->capacity)
    return -1;

  *offset = i;
  return 0;
}

static inline void pvr_file_set_valtab_entry_used(pvr_file_valtab_entry_t* entry, uint64_t value)
{
  entry->flags |= VALTAB_ENTRY_FLAG_USED;
  entry->value = value;
}

static int pvr_file_add_variable(struct profile_var_template* var)
{
  uint8_t buffer_bounds_respected;
  uint32_t valtab_offset;
  uint32_t strtab_offset;
  size_t buffersize;
  pvr_file_var_t* c_var;

  c_var = fvar_buffer;
  buffersize = 0;
  valtab_offset = strtab_offset = 0;

  /* Find an unused var */
  while ((buffer_bounds_respected = (buffersize + sizeof(*c_var)) < fvar_buffersize)) {

    if (c_var->var_type == PROFILE_VAR_TYPE_UNSET)
      break;

    c_var++;
    buffersize += sizeof(*c_var);
  }

  /* Could not find a free var */
  if (!buffer_bounds_respected)
    return -1;

  pvr_file_find_free_valtab_offset(&valtab_offset);

  if (var->type == PROFILE_VAR_TYPE_STRING) {
    pvr_file_find_free_strtab_offset(&strtab_offset);

    /* Let's trust that there is a string in ->value =) */
    strcpy(&strtab_buffer->entries[strtab_offset], var->value);

    /* Set the value in the valtab to point to our value string */
    pvr_file_set_valtab_entry_used(&valtab_buffer->entries[valtab_offset], strtab_offset);
  } else {
    pvr_file_set_valtab_entry_used(&valtab_buffer->entries[valtab_offset], (uint64_t)var->value);
  }

  pvr_file_find_free_strtab_offset(&strtab_offset);

  strcpy(&strtab_buffer->entries[strtab_offset], var->key);

  /* Set the c_var */
  c_var->var_type = var->type;
  c_var->str_off = strtab_offset;
  c_var->value_off = valtab_offset;
  c_var->var_flags = var->flags;

  pvr_hdr.var_count++;
  return 0;
}

static void init_fvar_buffer(uint32_t buffersize)
{
  pvr_file_var_t* c_var;
  size_t current_size;

  fvar_buffersize = buffersize;
  fvar_buffer = malloc(buffersize);

  /* Clear the buffer */
  memset((void*)fvar_buffer, 0, buffersize);

  /* Set our capacity */
  pvr_hdr.var_capacity = (buffersize) / sizeof(pvr_file_var_t);

  for (uint32_t i = 0; i < pvr_hdr.var_capacity; i++) {
    c_var = &fvar_buffer[i];

    c_var->var_type = PROFILE_VAR_TYPE_UNSET;
  }
}

static void init_valtab_buffer(uint32_t buffersize)
{
  valtab_buffersize = buffersize;
  valtab_buffer = malloc(buffersize);

  memset((void*)valtab_buffer, 0, buffersize);

  valtab_buffer->capacity = (buffersize - sizeof(*valtab_buffer)) / sizeof(pvr_file_valtab_entry_t);
}

static void init_strtab_buffer(uint32_t buffersize)
{
  strtab_buffersize = buffersize;
  strtab_buffer = malloc(buffersize);

  memset((void*)strtab_buffer, 0, buffersize);

  strtab_buffer->bytecount = buffersize - sizeof(*strtab_buffer);
}

/*!
 * @brief: Tool to create .pvr files for lightos 
 *
 * .pvr (dot profile variable) files are files that contain profile variables for a specific profile
 * They are used by the aniva kernel to read in saved profile variables for any profiles during system boot.
 * The kernel is able to create its own variable files, but we need some sort of bootstrap, which this tool
 * is for.
 *
 * NOTE: the name of the .pvr file should be the same as the profile name it targets. This means there can't be two 
 * files in the same directory, that target the same profile.
 *
 * Usage:
 * This tool can be used to create, modify, test, and install .pvr files. This is how it's used:
 *  base_env [target path] [flags] <flag args>
 * The target path is relative to the /system directory in the project root, which serves as an image for the ramdisk 
 * and the systems initial fs state.
 * Accepted flags are:
 *  -B : Create the default .pvr file for the BASE profile
 *  -G : Create the default .pvr file for the Global profile
 *  -e <VAR NAME> <new value> : Edits a variable with a value of the same type
 *  -r <VAR NAME> : Removes a variable
 *  -t <VAR NAME> <NEW TYPE> : Change the type of a variable
 *
 * File layout:
 * The layout of a .pvr file is closely related to how an ELF file is laid out. There is a header which defines some 
 * initial offsets for the value and string tables. The var table starts directly after the header, so there is no offset needed there
 *
 * TODO: parse arguments
 */
int main(int argc, char** argv)
{
  size_t cur_offset;
  printf("Welcome to the base_env manager\n");

  memset(&pvr_hdr, 0, sizeof(pvr_hdr));
  memcpy((void*)&pvr_hdr.sign, PVR_SIG, 4);

  init_fvar_buffer(sizeof(pvr_file_var_t) * DEFAULT_VAR_CAPACITY);
  init_valtab_buffer(sizeof(pvr_file_valtab_entry_t) * DEFAULT_VALTAB_CAPACITY);
  init_strtab_buffer(sizeof(pvr_file_strtab_t) + DEFAULT_STRTAB_BYTECOUNT);

  FILE* f = fopen("./test.pvr", "w");

  if (!f)
    return -1;

  for (uint32_t i = 0; i < (sizeof global_defaults / sizeof global_defaults[0]); i++)
    pvr_file_add_variable(&global_defaults[i]);

  /*
   * Write the entire thing to disk
   */
  {

    fseek(f, sizeof(pvr_hdr), SEEK_SET);
    cur_offset = sizeof(pvr_hdr);

    cur_offset += fvar_buffersize;
    fwrite(fvar_buffer, fvar_buffersize, 1, f);

    pvr_hdr.valtab_offset = cur_offset;
    cur_offset += valtab_buffersize;
    fwrite(valtab_buffer, valtab_buffersize, 1, f);

    pvr_hdr.strtab_offset = cur_offset;
    cur_offset += strtab_buffersize;
    fwrite(strtab_buffer, strtab_buffersize, 1, f);

    /* Write the header last */
    fseek(f, 0, SEEK_SET);
    fwrite(&pvr_hdr, sizeof(pvr_hdr), 1, f);

    fclose(f);
  }
}
