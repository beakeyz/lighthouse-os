#include "test.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "libk/data/queue.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/ipc/tspckt.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"

int test_dbg_init();
int test_dbg_exit();

void kb_callback(ps2_key_event_t event);

uintptr_t test_dbg_msg(packet_payload_t payload, packet_response_t** response);

const aniva_driver_t g_test_dbg_driver = {
  .m_name = "debug",
  .m_type = DT_DIAGNOSTICS,
  .m_ident = DRIVER_IDENT(0, 1),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = test_dbg_init,
  .f_exit = test_dbg_exit,
  .f_drv_msg = test_dbg_msg,
  .m_port = 1,
};

int test_dbg_init() {

  println("Initialized the test debug driver!");

  return 0;
}

int test_dbg_exit() {
  return 0;
}

uintptr_t test_dbg_msg(packet_payload_t payload, packet_response_t** response) {

  println("debug: test_print");
  println(to_string(payload.m_code));

  return 0;
}

void kb_callback(ps2_key_event_t event) {
  println("callback got called =D");

  driver_send_packet("io.ps2_kb", KB_UNREGISTER_CALLBACK, kb_callback, sizeof(&kb_callback));
}
