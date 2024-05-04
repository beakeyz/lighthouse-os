#ifndef __ANIVA_ENVDRV_PRISM__
#define __ANIVA_ENVDRV_PRISM__

#include "libk/stddef.h"

enum PRISM_KEYWORD {
  PRISM_KEYWORD_MODE,
  PRISM_KEYWORD_FUNC,
};

enum PRISM_OPERATOR {
  PRISM_OPERATOR_ASSIGNMENT,
  PRISM_OPERATOR_ADD,
  PRISM_OPERATOR_SUBTRACT,
  PRISM_OPERATOR_TYPE,
};

enum PRISM_ENCAPSULATOR {
  PRISM_ENCAPSULATOR_SCOPE,
  PRISM_ENCAPSULATOR_FUNC_PARAMS,
  PRISM_ENCAPSULATOR_CAST,
};

typedef struct prism_encapsulator {
  enum PRISM_ENCAPSULATOR id;
  bool opening;
} prism_encapsulator_t;

bool prism_is_valid_keyword(const char* label, uint32_t label_len);
bool prism_is_valid_operator(const char* label, uint32_t label_len);
bool prism_is_valid_encapsulator(char c, prism_encapsulator_t* b_out);

#endif // !__ANIVA_ENVDRV_PRISM__
