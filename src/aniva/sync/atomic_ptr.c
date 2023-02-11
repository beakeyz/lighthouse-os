#include "atomic_ptr.h"
#include "libk/atomic.h"
#include <mem/heap.h>
#include <mem/heap.h>

union atomic_ptr {
  _Atomic uintptr_t __lock;
};

atomic_ptr_t* create_atomic_ptr() {
  return kmalloc(sizeof(atomic_ptr_t));
}

atomic_ptr_t *create_atomic_ptr_with_value(uintptr_t initial_value) {
  atomic_ptr_t *ret = create_atomic_ptr();
  atomic_ptr_write(ret, initial_value);
}

uintptr_t atomic_ptr_load(atomic_ptr_t* ptr) {
  uintptr_t ret = atomicLoad_alt((volatile uintptr_t *) &ptr->__lock);
  return ret;
}

void atomic_ptr_write(atomic_ptr_t* ptr, uintptr_t value) {
  atomicStore_alt((volatile uintptr_t *) &ptr->__lock, value);
}
