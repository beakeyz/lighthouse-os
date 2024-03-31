#include "kerror.h"
#include "kevent/event.h"
#include "libk/flow/error.h"

/*!
 * @brief: Called when a kerror happens
 *
 *
 */
void throw_kerror(registers_t* cpu_state, enum KERROR_TYPE type, int code)
{
  kevent_error_ctx_t ctx = {
    type,
    code,
    cpu_state
  };

  kevent_fire("error", &ctx, sizeof(ctx));
}

/*!
 * @brief: Final endpoint of a kerror
 *
 * If this is a recoverable error in a process, the process should be ended and the kernel
 * should be able to recover
 */
int __default_kerror_handler(kevent_ctx_t* ctx)
{
  kevent_error_ctx_t* error;

  error = ctx->buffer;

  switch (error->type) {
    default:
      break;
  }

  kernel_panic("Reached __default_kerror_handler");
}
