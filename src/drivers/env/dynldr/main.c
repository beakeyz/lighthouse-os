#include "priv.h"
#include "fs/file.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/driver/loader.h"
#include "sync/mutex.h"
#include <lightos/handle_def.h>
#include <dev/core.h>
#include <dev/driver.h>

/*
 * Driver to dynamically load programs that were linked against shared binaries
 *
 */

/* Where we store all the loaded_app structs */
static list_t* _loaded_apps;
static mutex_t* _dynld_lock;

/*!
 * @brief: Register an app that has been dynamically loaded
 *
 * Simply appends to the applist
 */
kerror_t register_app(loaded_app_t* app)
{
  if (!app)
    return -KERR_NULL;

  mutex_lock(_dynld_lock);

  list_append(_loaded_apps, app);

  mutex_unlock(_dynld_lock);
  return KERR_NONE;
}

/*!
 * @brief: Unregister an app that has been dynamically loaded
 *
 * Simply removes from the applist
 */
kerror_t unregister_app(loaded_app_t* app)
{
  bool result;

  if (!app)
    return -KERR_NULL;

  mutex_lock(_dynld_lock);

  result = list_remove_ex(_loaded_apps, app);

  mutex_unlock(_dynld_lock);

  return result ? KERR_NONE : -KERR_INVAL;
}

/*!
 * @brief: Hook for any process events
 */
static int _dyn_loader_proc_hook(kevent_ctx_t* _ctx)
{
  kevent_proc_ctx_t* ctx;

  if (_ctx->buffer_size != sizeof(*ctx))
    return -1;

  ctx = (kevent_proc_ctx_t*)_ctx->buffer;

  /* We're not interrested in these events */
  if (ctx->type != PROC_EVENTTYPE_DESTROY)
    return 0;

  /*
   * We've hit an interresting event 
   * Now there is an issue, since we're inside an event context: we need to aquire
   * the loaders lock, but we can't. To solve this here is a lil todo,
   * TODO: Create workqueues that we can defer this kind of work to. This means we need to 'pause'
   * the actual event, because in the case of a proc destroy for example, we can't just defer the 
   * event handler, continue to destory the process and THEN call the deferred handler, right after
   * we've already gotter rid of the process...
   */
  printf("Process: %s\n", ctx->process->m_name);
  kernel_panic("Hit dynamic process destroy hook");

  return 0;
}

/*!
 * @brief: Initialize the drivers structures
 */
static int _loader_init()
{
  _loaded_apps = init_list();
  _dynld_lock = create_mutex(NULL);

  ASSERT(KERR_OK(kevent_add_hook("proc", DYN_LDR_NAME, _dyn_loader_proc_hook)));
  return 0;
}

/*!
 * @brief: Destruct the drivers structures
 */
static int _loader_exit()
{
  ASSERT(KERR_OK(kevent_remove_hook("proc", DYN_LDR_NAME)));

  destroy_list(_loaded_apps);
  destroy_mutex(_dynld_lock);
  return 0;
}

/*!
 * @brief: Load a dynamically linked program directly from @file
 */
static kerror_t _loader_ld_appfile(file_t* file)
{
  kerror_t error;
  loaded_app_t* app;

  /* We've found the file, let's try to load this fucker */
  error = load_app(file, &app);

  if (error || !app)
    return -DRV_STAT_INVAL;

  printf("Ayay\n");
  /* Everything went good, register it to ourself */
  register_app(app);

  return KERR_NONE;
}

/*!
 * @brief: Open @path and load the app that in the resulting file
 */
static kerror_t _loader_ld_app(const char* path, size_t pathlen)
{
  kerror_t error;
  file_t* file;

  /* Try to find the file (TODO: make this not just gobble up the input buffer) */
  file = file_open(path);

  if (!file)
    return -DRV_STAT_INVAL;

  error = _loader_ld_appfile(file);

  file_close(file);

  return error;
}

static uint64_t _loader_msg(aniva_driver_t* driver, dcc_t code, void* in_buf, size_t in_bsize, void* out_buf, size_t out_bsize)
{
  file_t* in_file;
  const char* in_path;

  switch (code) {
    /* 
     * Load an app with dynamic libraries 
     * TODO: safety
     */
    case DYN_LDR_LOAD_APP:
      in_path = in_buf;

      if (!in_path)
        return DRV_STAT_INVAL;

      /* FIXME: Safety here lmao */
      if (_loader_ld_app(in_path, in_bsize))
        return DRV_STAT_INVAL;

      break;
    case DYN_LDR_LOAD_APPFILE:
      in_file = in_buf;

      /* This would be kinda cringe */
      if (!in_file || in_bsize != sizeof(*in_file))
        return DRV_STAT_INVAL;

      /* 
       * Try to load this mofo directly.
       * NOTE: File ownership is in the hands of the caller, 
       *       so we can't touch file lifetime
       */
      if (_loader_ld_appfile(in_file))
        return DRV_STAT_INVAL;

      break;
    /* Look through the loaded libraries of the current process to find the specified library */
    case DYN_LDR_GET_LIB:

      /* Can't give out a handle */
      if (out_bsize != sizeof(HANDLE))
        return DRV_STAT_INVAL;

      kernel_panic("TODO: DYN_LDR_GET_LIB");
      break;
    default:
      return DRV_STAT_INVAL;
  }

  return DRV_STAT_OK;
}

EXPORT_DRIVER(_dynamic_loader) = {
  .m_name = DYN_LDR_NAME,
  .m_descriptor = "Used to load dynamically linked apps",
  .m_type = DT_OTHER,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = _loader_init,
  .f_exit = _loader_exit,
  .f_msg = _loader_msg,
};

EXPORT_DEPENDENCIES(deps) = {
  DRV_DEP_END,
};
