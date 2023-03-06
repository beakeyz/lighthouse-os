#include "test.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "libk/queue.h"
#include "proc/core.h"
#include "proc/ipc/tspckt.h"

void test_dbg_init(queue_t* buffer);
int test_dbg_exit();

const aniva_driver_t g_test_dbg_driver = {
  .m_name = "debug",
  .m_type = DT_DIAGNOSTICS,
  .m_ident = DRIVER_IDENT(0, 1),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = (FuncPtr)test_dbg_init,
  .f_exit = test_dbg_exit,
  .m_port = 1,
};

void test_dbg_init(queue_t* buffer) {

  for (;;) {
    tspckt_t* packet = queue_dequeue(buffer);

    if (validate_tspckt(packet)) {
      void* data = packet->m_data;

      uintptr_t value = *(uintptr_t*)data;

      if (value == TEST_DBG_PRINT) {
        println("HI FROM THE TEST DEBUG DRIVER!");
        draw_char(150, 100, 'D');
      }
      destroy_tspckt(packet);
    }
  }
}

int test_dbg_exit() {
  return 0;
}
