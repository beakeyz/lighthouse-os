#ifndef __ANIVA_CMDLINE_PARSER__
#define __ANIVA_CMDLINE_PARSER__

#include "libk/flow/error.h"
#include <libk/stddef.h>

enum KOPT_TYPE {
  KOPT_TYPE_NUM,
  KOPT_TYPE_BOOL,
  KOPT_TYPE_STR,
};

/*
 * These objects are the tokens that the parser generates from a single cmdline given to us
 * by the bootloader
 */
typedef struct kopt {
  const char* key;
  enum KOPT_TYPE type;

  union {
    bool bool_value;
    uint64_t num_val;
    const char* str_value;
  };
} kopt_t;

void init_cmdline_parser();

bool opt_parser_get_bool(const char* boolkey);
kerror_t opt_parser_get_bool_ex(const char* boolkey, bool* bval);
kerror_t opt_parser_get_str(const char* strkey, const char** bstr);
kerror_t opt_parser_get_num(const char* numkey, uint64_t* bval);

#endif // !__ANIVA_CMDLINE_PARSER__
