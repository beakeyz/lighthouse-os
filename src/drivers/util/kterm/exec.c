#include "exec.h"
#include "dev/manifest.h"
#include "fs/vfs.h"
#include "fs/vobj.h"
#include "libk/bin/elf.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

void kterm_try_exec(char* buffer, uint32_t buffer_size)
{
  proc_t* p;

  if (buffer[0] == NULL)
    return;

  vobj_t* obj = vfs_resolve(buffer);

  if (!obj)
    return;

  file_t* file = vobj_get_file(obj);

  if (!file) {
    logln("Could not execute object!");
    return;
  }

  ErrorOrPtr result = elf_exec_static_64_ex(file, false, true);

  if (IsError(result)) {
    logln("Coult not execute object!");
    vobj_close(obj);
    return;
  }

  p = (proc_t*)Release(result);

  vobj_close(obj);

  khandle_type_t driver_type = KHNDL_TYPE_DRIVER;
  dev_manifest_t* kterm_manifest = get_driver("other/kterm");

  khandle_t _stdin;
  khandle_t _stdout;
  khandle_t _stderr;

  create_khandle(&_stdin, &driver_type, kterm_manifest);
  create_khandle(&_stdout, &driver_type, kterm_manifest);
  create_khandle(&_stderr, &driver_type, kterm_manifest);

  _stdin.flags |= HNDL_FLAG_READACCESS;
  _stdout.flags |= HNDL_FLAG_WRITEACCESS;
  _stderr.flags |= HNDL_FLAG_RW;

  bind_khandle(&p->m_handle_map, &_stdin);
  bind_khandle(&p->m_handle_map, &_stdout);
  bind_khandle(&p->m_handle_map, &_stderr);

  sched_add_priority_proc(p, true);

  await_proc_termination(p);
}
