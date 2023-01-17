#include "tuple.h"

tuple_t create_tuple(uintptr_t one, uintptr_t two) {
  tuple_t t = {
    .one = one,
    .two = two
  };
  return t;
}

tuple_t empty_tuple() {
  return create_tuple(NULL, NULL);
}

ptr_tuple_t create_ptr_tuple(void* one, void* two) {
  ptr_tuple_t pt = {
    .one = one,
    .two = two
  };
  return pt;
}

ptr_tuple_t empty_ptr_tuple() {
  return create_ptr_tuple(nullptr, nullptr);
}

// compare value
bool compare_tuple(tuple_t* one, tuple_t* two) {
  return (one->one == two->one && one->two == two->two);
}

// compare address
bool compare_ptr_tuple(ptr_tuple_t* one, ptr_tuple_t* two) {
  return (one->one == two->one && one->two == two->two);
}
