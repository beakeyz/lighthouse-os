#include "test.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/queue.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/ipc/tspckt.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"

void test_dbg_init(queue_t* buffer);
int test_dbg_exit();

uintptr_t test_dbg_msg(void* buffer, size_t buffer_size);

const aniva_driver_t g_test_dbg_driver = {
  .m_name = "debug",
  .m_type = DT_DIAGNOSTICS,
  .m_ident = DRIVER_IDENT(0, 1),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = (FuncPtr)test_dbg_init,
  .f_exit = test_dbg_exit,
  .f_drv_msg = test_dbg_msg,
  .m_port = 1,
};

void test_dbg_init(queue_t* buffer) {

  println("Initialized the test debug driver!");

}

int test_dbg_exit() {

  draw_char(169 + 8, 100, 'i');
  return 0;
}

uintptr_t test_dbg_msg(void* buffer, size_t buffer_size) {

  uintptr_t data = *(uintptr_t*)buffer;

  if (data == TEST_DBG_PRINT) {
    println("Hi bitch");
    draw_char(169, 100, 'h');
  }

  return 0;
}
