#include "ast.h"
#include "drivers/env/prism/error.h"
#include "drivers/env/prism/prism.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include <libk/string.h>

static prism_token_t* create_prism_token(const char* label, uint32_t label_len, enum PRISM_TOKEN_TYPE type)
{
  prism_encapsulator_t enc;
  prism_token_t* ret;

  /* Input validation */
  switch (type) {
    case PRISM_TOKEN_TYPE_KEYWORD:
      if (!prism_is_valid_keyword(label, label_len))
        return nullptr;
      break;
    case PRISM_TOKEN_TYPE_ENCAPSULATOR:
      if (!prism_is_valid_encapsulator(*label, &enc))
        return nullptr;
    default:
      break;
  }

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->type = type;
  ret->label = kmalloc(label_len + 1);

  memset((void*)ret->label, 0, label_len + 1);
  memcpy((void*)ret->label, label, label_len);

  /* Private field population */
  switch (type) {
    case PRISM_TOKEN_TYPE_ENCAPSULATOR:
      ret->priv.encapsulator = enc;
      break;
    default:
      break;
  }
  
  return ret;
}

static inline bool _prism_is_word_char(char c)
{
  return (
    (c >= 'a' && c <= 'z') ||
    (c >= 'A' && c <= 'Z') || 
    (c >= '0' && c <= '9')
  );
}

static inline bool _prism_is_opperator_char(char c)
{
  return (prism_is_valid_operator(&c, 1));
}

static inline bool _prism_is_encapsulator_char(char c)
{
  return (
    (c == '(') || (c == ')') ||
    (c == '{') || (c == '}')
  );
}

/*
static inline bool _prism_is_valid_char(char c)
{
  return (
    _prism_is_word_char(c) || 
    _prism_is_opperator_char(c) ||
    _prism_is_encapsulator_char(c)
  );
}
*/

enum AST_CHAR_TYPE {
  CHAR_TYPE_WORD,
  CHAR_TYPE_ENCAPSULATOR,
  CHAR_TYPE_OPPERATOR,
  CHAR_TYPE_OTHER,
};

static inline void _prism_ast_set_chartype(char c, enum AST_CHAR_TYPE* type)
{
  if (_prism_is_word_char(c))
    *type = CHAR_TYPE_WORD;
  else if (_prism_is_encapsulator_char(c))
    *type = CHAR_TYPE_ENCAPSULATOR;
  else if (_prism_is_opperator_char(c))
    *type = CHAR_TYPE_OPPERATOR;
  else
    *type = CHAR_TYPE_OTHER;
}

static inline void _prism_ast_determine_token_type(const char* token, uint32_t token_len, enum PRISM_TOKEN_TYPE* type)
{
  if (prism_is_valid_keyword(token, token_len))
    *type = PRISM_TOKEN_TYPE_KEYWORD;
  else if (prism_is_valid_operator(token, token_len))
    *type = PRISM_TOKEN_TYPE_OPERATOR;
  else if (token_len == 1 && _prism_is_encapsulator_char(*token))
    *type = PRISM_TOKEN_TYPE_ENCAPSULATOR;
  else
    *type = PRISM_TOKEN_TYPE_IDENTIFIER;
}

static inline bool _prism_ast_does_token_target(prism_token_t* current, prism_token_t* next)
{
  return (
      (current->type == PRISM_TOKEN_TYPE_KEYWORD && next->type == PRISM_TOKEN_TYPE_IDENTIFIER) ||
      (current->type == PRISM_TOKEN_TYPE_IDENTIFIER && 
        (next->type == PRISM_TOKEN_TYPE_ENCAPSULATOR || next->type == PRISM_TOKEN_TYPE_OPERATOR)) ||
      (current->type == PRISM_TOKEN_TYPE_ENCAPSULATOR &&
        (next->type == PRISM_TOKEN_TYPE_ENCAPSULATOR || next->type == PRISM_TOKEN_TYPE_IDENTIFIER))
  );
}

static int _prism_ast_process_token(prism_ast_t* ast, prism_token_t** target_token, const char* c_token, uint32_t token_len, prism_error_t* error)
{
  prism_token_t* new_token;
  prism_token_t* this_token;
  enum PRISM_TOKEN_TYPE type;

  _prism_ast_determine_token_type(c_token, token_len, &type);

  new_token = create_prism_token(c_token, token_len, type);

  if (!new_token)
    return init_prism_error(error, PRISM_ERR_INTERNAL, NULL, "Could not create token!");

  this_token = *target_token;

  if (!_prism_ast_does_token_target(this_token, new_token))
    ast->syntax_errors++;

  this_token->next = new_token;

  *target_token = new_token;
  return 0;
}

/*!
 * @brief: Constructs a Prism AST based on the provided chunk of code
 *
 * First thing we expect to find in a block of code is a mode declaration. This tells us the executors intent with the code.
 * If the first token found is not a mode declarator, usermode is implied
 *
 * @code: The ASCII string which contains the raw Prism code
 * @code_len: Length of the code buffer
 */
prism_ast_t* create_prism_ast(const char* code, size_t code_len, prism_error_t* error)
{
  uint64_t i;
  uint32_t token_len;
  const char* c_code_token;
  const char* c_char;
  enum AST_CHAR_TYPE prev_ctype, c_ctype;
  prism_ast_t* ret;
  prism_token_t* c_token;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->root = create_prism_token("mode", 4, PRISM_TOKEN_TYPE_KEYWORD);
  c_token = ret->root->next = create_prism_token("user", 4, PRISM_TOKEN_TYPE_IDENTIFIER);

  i = 0;
  c_code_token = nullptr;
  c_ctype = CHAR_TYPE_OTHER;

  /*
   * Scan the entire code :clown: 
   * 
   * TODO: Make this more efficient
   */
  for (; i < code_len; i++) {
    c_char = &code[i];

    prev_ctype = c_ctype;

    /*
     * Certain characters spell different kinds of tokens. If we go from a letter to a space for
     * example, we know that we have hit the end of a token 
     */
    _prism_ast_set_chartype(*c_char, &c_ctype);

    /* If we see a change in character type, we can start processing a new token (prolly) */
    if (c_ctype == prev_ctype)
      continue;

    /* Yay we did find a new token! process it */
    if (c_code_token && prev_ctype != CHAR_TYPE_OTHER) {
      /* Compute length */
      token_len = c_char - c_code_token;

      /* Process the bitch */
      _prism_ast_process_token(
          ret,
          &c_token,
          c_code_token,
          token_len,
          error
      );

      if (prism_is_error(error))
        goto exit_and_error;
    }

    /* Set the token */
    c_code_token = c_char;
  }

  return ret;

exit_and_error:
  kernel_panic("FUCK Prism ast error!");
}

/*!
 * @brief: Destroys a complete AST
 *
 * This is a recursive opperation
 */
int destroy_prism_ast(prism_ast_t* ast)
{
  kernel_panic("TODO: destroy_prism_ast");
  return 0;
}

/*!
 * @brief: Print debug information about a Prism AST
 */
int prism_ast_debug(prism_ast_t* ast)
{
  uint32_t indent_count;
  prism_token_t* walker;

  kwarnf("Trying to debug...\n");

  if (!ast->root)
    return -1;

  indent_count = 0;
  walker = ast->root;

  do {
    if (walker->type == PRISM_TOKEN_TYPE_ENCAPSULATOR && !walker->priv.encapsulator.opening && indent_count)
      indent_count--;

    kwarnf("Token: %s%s (type=%d)\n", (indent_count ? "  " : ""), walker->label, walker->type);

    if (walker->type == PRISM_TOKEN_TYPE_ENCAPSULATOR && walker->priv.encapsulator.opening)
      indent_count++;

    walker = walker->next;
  } while (walker);

  return 0;
}
