#ifndef __ANIVA_LOGGING__
#define __ANIVA_LOGGING__

#include <libk/stddef.h>

#define LOGGER_MAX_COUNT 255

/* Does this logger display debug info? */
#define LOGGER_FLAG_DEBUG 0x01
/* Does this logger display regular info? */
#define LOGGER_FLAG_INFO 0x02
/* Does this logger display warnings? */
#define LOGGER_FLAG_WARNINGS 0x04
#define LOGGER_FLAG_NO_PREFIX 0x08

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
#define LOG_TYPE_LINE 1
#define LOG_TYPE_CHAR 2

/*
 * NOTE: log calls only log to the logger with id 0 !!
 */

void log(const char* msg);
void logf(const char* fmt, ...);
void vlogf(const char* fmt, va_list args);
void logln(const char* msg);
void log_ex(logger_id_t id, const char* msg, va_list args, uint8_t type);

/* Format strings */
int sfmt(char* buf, const char* fmt, ...);

#if DBG_VERBOSE
#define KLOG(fmt, ...) printf(fmt, __VA_ARGS__ - 0)
#define KLOG_DBG(fmt, ...) kdbgf(fmt, __VA_ARGS__ - 0)
#define KLOG_INFO(fmt, ...) logf(fmt, __VA_ARGS__ - 0)

#define V_KLOG(fmt, args) vprintf(fmt, args)
#define V_KLOG_DBG(fmt, args) vkdbgf(false, fmt, args)
#define V_KLOG_INFO(fmt, args) vlogf(fmt, args)
#else
#define KLOG(fmt, ...)
#define V_KLOG(fmt, args)
#endif

#if DBG_DEBUG
#define KLOG_DBG(fmt, ...) kdbgf(fmt, __VA_ARGS__ - 0)
#define KLOG_INFO(fmt, ...) logf(fmt, __VA_ARGS__ - 0)

#define V_KLOG_DBG(fmt, args) vkdbgf(fmt, args)
#define V_KLOG_INFO(fmt, args) vlogf(fmt, args)
#else
#ifndef KLOG_DBG
#define KLOG_DBG(fmt, ...)
#define V_KLOG_DBG(fmt, args)
#endif
#endif

#if DBG_INFO
#define KLOG_INFO(fmt, ...) logf(fmt, __VA_ARGS__ - 0)
#define V_KLOG_INFO(fmt, args) vlogf(fmt, args)
#else
#ifndef KLOG_INFO
#define KLOG_INFO(fmt, ...)
#define V_KLOG_INFO(fmt, args)
#endif
#endif // DEBUG

#define KLOG_ERR(fmt, ...) kwarnf(fmt, __VA_ARGS__ - 0)

/*
 * Functions that perform a print to the kernels 'stdio'
 *
 * TODO: rename to kprintf, kprintln, ect.
 *
 * These prints only send to info loggers
 */
int kputch(char c);
int print(const char* msg);
int println(const char* msg);
int printf(const char* fmt, ...);
int vprintf(const char* fmt, va_list args);

/*
 * Debug print routines
 *
 * These prints only send to debug loggers
 */
int kdbgf(const char* fmt, ...);
int vkdbgf(bool prefix, const char* fmt, va_list args);
int kdbgln(const char* msg);
int kdbg(const char* msg);
int kdbgc(char c);

/*
 * Warning print routines
 *
 * These prints only send to warning loggers
 */
int kwarnf(const char* fmt, ...);
int kwarnln(const char* msg);
int kwarn(const char* msg);
int kwarnc(char c);

#endif // !__ANIVA_LOGGING__
