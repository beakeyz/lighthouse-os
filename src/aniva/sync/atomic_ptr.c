#include "atomic_ptr.h"
#include "libk/atomic.h"
#include "mem/kmalloc.h"

union atomic_ptr {
  uintptr_t __lock;
};

atomic_ptr_t* create_atomic_ptr() {
  return kmalloc(sizeof(atomic_ptr_t));
}

uintptr_t atomic_ptr_load(atomic_ptr_t* ptr) {
  uintptr_t ret = atomicLoad_alt(&ptr->__lock);
  return ret;
}

void atomic_ptr_write(atomic_ptr_t* ptr, uintptr_t value) {
  atomicStore_alt(&ptr->__lock, value);
}
