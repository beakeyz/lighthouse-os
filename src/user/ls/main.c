#include "lightos/fs/dir.h"
#include "lightos/proc/cmdline.h"
#include <stdio.h>

CMDLINE line;

static const char* _ls_get_target_path()
{
  return "Runtime/User";
}

static void _ls_print_direntry(DirEntry* entry)
{
  printf(" %s\n", entry->name);
}

/*!
 * The ls binary for lightos
 *
 * pretty much just copied the manpage of the linux version of this utility.
 * There are a few different things though. Since lightos does not give an argv
 * though the program execution pipeline, we need to ask lightos for our commandline.
 */
int main() 
{
  int error;
  uint64_t idx;
  const char* path;
  Directory* dir;
  DirEntry* entry;

  error = cmdline_get(&line);

  if (error)
    return error;

  path = _ls_get_target_path();

  idx = 0;
  dir = open_dir(path, NULL, NULL);

  if (!dir) {
    printf("Invalid path (Not a directory?)\n");
    goto close_and_exit;
  }

  do {
    entry = dir_read_entry(dir, idx);

    if (!entry)
      break;

    _ls_print_direntry(entry);

    idx++;
  } while (true);

close_and_exit:
  close_dir(dir);
  return 0;
}
