#ifndef __ANIVA_LOGGING__
#define __ANIVA_LOGGING__

#include <libk/stddef.h>

#define LOGGER_MAX_COUNT 255

#define LOGGER_FLAG_NO_CHAIN 0x01 /* This logger should not be chained by print calls */

typedef uint8_t logger_id_t;

/*
 * A logger is a basic struct that keeps track of
 * data that is needed by logger entities
 *
 * This structure is not supposed to be allocated on the heap EVER
 */
typedef struct {
  char* title;
 
  /*
   * EVERYWHERE where there is a logger struct, this field needs 
   * to either be used, or set to NULL 
   */
  uint8_t flags;
  logger_id_t id;

  int (*f_logc)(char c);
  int (*f_log)(const char* str);
  int (*f_logln)(const char* str);
  int (*f_logf)(const char* fmt, ...);
} logger_t;

void init_early_logging();
void init_logging();

/* Function prototypes */

int register_logger(logger_t* logger);
int unregister_logger(logger_t* logger);

int logger_get(logger_id_t id, logger_t* buffer);
int logger_scan(char* title, logger_t* buffer);

int logger_swap_priority(logger_id_t old, logger_id_t new);

#define LOG_TYPE_DEFAULT 0
#define LOG_TYPE_FMT     1
#define LOG_TYPE_LINE    2
#define LOG_TYPE_CHAR    3
#define LOG_TYPE_ERROR   4 /* Defaults to line type */
#define LOG_TYPE_WARNING 5 /* Defaults to line type */

/*
 * NOTE: log calls only log to the logger with id 0 !!
 */

void log(const char* msg);
void logf(const char* fmt, ...);
void logln(const char* msg);
void log_ex(logger_id_t id, const char* msg, va_list args, uint8_t type);

/*
 * Functions that perform a print to the kernels 'stdio'
 */

int putch(char c);
int print(const char* msg);
int println(const char* msg);
int printf(const char* fmt, ...);

#endif // !__ANIVA_LOGGING__
