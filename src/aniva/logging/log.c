#include "log.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "sync/spinlock.h"
#include <libk/ctype.h>
#include <libk/string.h>
#include <sync/mutex.h>

static mutex_t* __default_mutex = NULL;
static spinlock_t* __log_lock = NULL;
static logger_t* __loggers[LOGGER_MAX_COUNT];
static size_t __loggers_count;

static bool valid_logger_id(logger_id_t id)
{
    return (id < LOGGER_MAX_COUNT);
}

static inline void try_lock()
{
    if (!__default_mutex)
        return;

    mutex_lock(__default_mutex);
}

static inline void try_unlock()
{
    if (!__default_mutex)
        return;

    mutex_unlock(__default_mutex);
}

static inline void try_lock_logging()
{
    if (!__log_lock)
        return;

    spinlock_lock(__log_lock);
}

static inline void try_unlock_logging()
{
    if (!__log_lock)
        return;

    spinlock_unlock(__log_lock);
}

/*!
 * @brief Register a logger to the logger list
 *
 * @returns: the index to the registered logger on success, otherwise
 * a negative errorcode
 */
int register_logger(logger_t* logger)
{
    /* A logger MUST at least support f_log */
    if (!logger || !logger->f_log)
        return -1;

    try_lock();

    logger->id = __loggers_count;
    __loggers[__loggers_count++] = logger;

    try_unlock();

    return logger->id;
}

/*!
 * @brief Remove a logger from the chain
 *
 * Shift every logger after the one we are removing back by one,
 * to fill the hole we made.
 * @returns: zero on success, negative on fault.
 */
int unregister_logger(logger_t* logger)
{
    logger_t* target;

    if (!valid_logger_id(logger->id))
        return -1;

    target = __loggers[logger->id];

    if (!target)
        return -2;

    try_lock();

    /* Clear the logger from the list */
    __loggers[logger->id] = nullptr;

    /* Shift over all the loggers after this one back by one */
    for (uint16_t i = logger->id; i < LOGGER_MAX_COUNT; i++) {
        if (!valid_logger_id(i))
            break;

        __loggers[i] = __loggers[i + 1];

        /* Check if we are shifting nullptrs, in which case we can just stop */
        if (!__loggers[i])
            break;
    }

    __loggers_count--;

    try_unlock();
    return 0;
}

/*!
 * @brief Return the logger with a certain ID
 *
 * Nothing to add here...
 */
int logger_get(logger_id_t id, logger_t* buffer)
{
    logger_t* target;

    if (!valid_logger_id(id))
        return -1;

    target = __loggers[id];

    if (!target)
        return -2;

    /*
     * Lock the mutex to prevent any funnynes
     */
    try_lock();

    memcpy(buffer, target, sizeof(logger_t));

    try_unlock();
    return 0;
}

/*!
 * @brief Scan the logger vector for a matching title
 *
 * Nothing to add here...
 */
int logger_scan(char* title, logger_t* buffer)
{
    logger_t* target = nullptr;

    try_lock();

    for (uint16_t i = 0; i < __loggers_count; i++) {
        target = __loggers[i];

        if (!target)
            break;

        if (strcmp(title, target->title) == 0)
            break;
    }

    try_unlock();

    if (!target)
        return -1;

    memcpy(buffer, target, sizeof(logger_t));

    return 0;
}

/*!
 * @brief Swap the priorities of two valid loggers
 *
 *
 */
int logger_swap_priority(logger_id_t old, logger_id_t new)
{
    logger_t* old_target;
    logger_t* new_target;
    logger_t buffer = { 0 };

    if (!valid_logger_id(old) || !valid_logger_id(new))
        return -1;

    old_target = __loggers[old];

    if (!old_target)
        return -2;

    new_target = __loggers[new];

    /* We enforce priority swaps with loggers that exist */
    if (!new_target)
        return -3;

    try_lock();

    /* Copy the new target into the buffer */
    memcpy(&buffer, new_target, sizeof(logger_t));

    /* Swap the ids */
    new_target->id = old_target->id;
    old_target->id = buffer.id;

    memcpy(new_target, old_target, sizeof(logger_t));
    memcpy(old_target, &buffer, sizeof(logger_t));

    try_unlock();

    return 0;
}

/*!
 * @brief Extended generic logging function
 *
 * Nothing to add here...
 */
void log_ex(logger_id_t id, const char* msg, va_list args, uint8_t type)
{
    logger_t* target;

    if (!valid_logger_id(id))
        return;

    target = __loggers[id];

    if (!target)
        return;

    switch (type) {
    case LOG_TYPE_DEFAULT: {
        if (target->f_log)
            target->f_log(msg);
        break;
    }
    case LOG_TYPE_CHAR: {
        if (target->f_logc)
            target->f_logc(msg[0]);

        break;
    }
    case LOG_TYPE_LINE: {
        if (target->f_logln)
            target->f_logln(msg);
        break;
    }
    default:
        kernel_panic("TODO: implement logging type!");
        break;
    }
}

/*!
 * @brief Preform a print of a specific type
 *
 * Nothing to add here...
 */
static int print_ex(uint8_t flags, const char* msg, uint8_t type)
{
    logger_t* current;

    /* Mask everything but the type flags */
    flags &= (LOGGER_FLAG_INFO | LOGGER_FLAG_DEBUG | LOGGER_FLAG_WARNINGS);

    for (uintptr_t i = 0; i < __loggers_count; i++) {

        current = __loggers[i];

        /* Only print to loggers that have matching flags */
        if ((current->flags & flags))
            log_ex(i, msg, nullptr, type);
    }

    return 0;
}

static int USED _kputch(uint8_t typeflags, char c, char** out, size_t* p_cur_size)
{
    /* We don't use this variable */
    (void)p_cur_size;

    return print_ex(typeflags, &c, LOG_TYPE_CHAR);
}

int kputch(char c)
{
    return _kputch(LOGGER_FLAG_INFO, c, NULL, NULL);
}

int print(const char* msg)
{
    return print_ex(LOGGER_FLAG_INFO, msg, LOG_TYPE_DEFAULT);
}

int println(const char* msg)
{
    return print_ex(LOGGER_FLAG_INFO, msg, LOG_TYPE_LINE);
}

static inline void _print_hex(uint8_t typeflags, uint64_t value, int prec, int (*output_cb)(uint8_t typeflags, char c, char** out, size_t* p_cur_size), char** out, size_t* max_size)
{
    uint32_t idx;
    char tmp[128];
    const char hex_chars[17] = "0123456789ABCDEF";
    uint8_t len = 1;

    memset(tmp, '0', sizeof(tmp));

    uint64_t size_test = value;
    while (size_test / 16 > 0) {
        size_test /= 16;
        len++;
    }

    if (len < prec && prec && prec != -1)
        len = prec;

    idx = 0;

    while (value / 16 > 0) {
        uint8_t remainder = value % 16;
        value -= remainder;
        tmp[len - 1 - idx] = hex_chars[remainder];
        idx++;
        value /= 16;
    }
    uint8_t last = value % 16;
    tmp[len - 1 - idx] = hex_chars[last];

    for (uint32_t i = 0; i < len; i++)
        output_cb(typeflags, tmp[i], out, max_size);
}

static inline int _print_decimal(uint8_t typeflags, int64_t value, int prec, int (*output_cb)(uint8_t typeflags, char c, char** out, size_t* p_cur_size), char** out, size_t* max_size)
{
    char tmp[128];
    uint8_t size = 1;

    memset(tmp, '0', sizeof(tmp));

    /* Test the size of this value */
    uint64_t size_test = value;
    while (size_test / 10 > 0) {
        size_test /= 10;
        size++;
    }

    if (size < prec && prec && prec != -1)
        size = prec;

    uint8_t index = 0;

    while (value / 10 > 0) {
        uint8_t remain = value % 10;
        value /= 10;
        tmp[size - 1 - index] = remain + '0';
        index++;
    }
    uint8_t remain = value % 10;
    tmp[size - 1 - index] = remain + '0';

    for (uint32_t i = 0; i < size; i++)
        output_cb(typeflags, tmp[i], out, max_size);

    return size;
}

static inline int _vprintf(uint8_t typeflags, const char* fmt, va_list args, int (*output_cb)(uint8_t typeflags, char c, char** out, size_t* p_cur_size), char** out, size_t max_size)
{
    int prec;
    const char* prefix;
    uint32_t integer_width;
    uint32_t max_length;

    prefix = nullptr;

    try_lock_logging();

    /* Check for the no prefix flag */
    if ((typeflags & LOGGER_FLAG_NO_PREFIX) != LOGGER_FLAG_NO_PREFIX) {
        if ((typeflags & LOGGER_FLAG_WARNINGS) == LOGGER_FLAG_WARNINGS)
            prefix = "[ ERROR ] ";
        else if ((typeflags & LOGGER_FLAG_DEBUG) == LOGGER_FLAG_DEBUG)
            prefix = "[ DEBUG ] ";
        else if ((typeflags & LOGGER_FLAG_INFO) == LOGGER_FLAG_INFO)
            prefix = "[ INFO ] ";

        if (prefix)
            for (const char* i = prefix; *i; i++)
                output_cb(typeflags, *i, out, &max_size);
    }

    for (const char* c = fmt; *c; c++) {

        if (*c != '%')
            goto kputch_cycle;

        integer_width = 0;
        max_length = 0;
        prec = -1;
        c++;

        if (*c == '-')
            c++;

        /* FIXME: Register digits at the start */
        while (isdigit(*c)) {
            max_length = (max_length * 10) + ((*c) - '0');
            c++;
        }

        if (*c == '.') {
            prec = 0;
            c++;

            while (*c >= '0' && *c <= '9') {
                prec *= 10;
                prec += *c - '0';
                c++;
            }
        }

        /* Account for size */
        while (*c == 'l' && integer_width < 2) {
            integer_width++;
            c++;
        }

        switch (*c) {
        case 'i':
        case 'd': {
            int64_t value;
            switch (integer_width) {
            case 0:
                value = va_arg(args, int);
                break;
            case 1:
                value = va_arg(args, long);
                break;
            case 2:
                value = va_arg(args, long long);
                break;
            default:
                value = 0;
            }

            if (value < 0) {
                value = -value;
                output_cb(typeflags, '-', out, &max_size);
            }

            _print_decimal(typeflags, value, prec, output_cb, out, &max_size);
            break;
        }
        case 'p':
            integer_width = 2;
        case 'x':
        case 'X': {
            uint64_t value;
            switch (integer_width) {
            case 0:
                value = va_arg(args, int);
                break;
            case 1:
                value = va_arg(args, long);
                break;
            case 2:
                value = va_arg(args, long long);
                break;
            default:
                value = 0;
            }

            _print_hex(typeflags, value, prec, output_cb, out, &max_size);
            break;
        }
        case 's': {
            const char* str = va_arg(args, char*);

            if (!max_length)
                while (*str)
                    output_cb(typeflags, *str++, out, &max_size);
            else
                while (max_length--)
                    if (*str)
                        output_cb(typeflags, *str++, out, &max_size);
                    else
                        output_cb(typeflags, ' ', out, &max_size);

            break;
        }
        default: {
            output_cb(typeflags, *c, out, &max_size);
            break;
        }
        }

        continue;

    kputch_cycle:
        output_cb(typeflags, *c, out, &max_size);
    }

    try_unlock_logging();
    return 0;
}

int vprintf(const char* fmt, va_list args)
{
    int error;

    /*
     * These kinds of prints should go to all loggers
     */
    error = _vprintf(LOGGER_FLAG_NO_PREFIX | LOGGER_FLAG_DEBUG | LOGGER_FLAG_INFO | LOGGER_FLAG_WARNINGS, fmt, args, _kputch, NULL, NULL);

    return error;
}

void log(const char* msg)
{
    log_ex(0, msg, nullptr, LOG_TYPE_DEFAULT);
}

void vlogf(const char* fmt, va_list args)
{
    (void)_vprintf(LOGGER_FLAG_INFO, fmt, args, _kputch, NULL, NULL);
}

void logf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vlogf(fmt, args);

    va_end(args);
}

void logln(const char* msg)
{
    log_ex(0, msg, nullptr, LOG_TYPE_LINE);
}

/*!
 * @brief: Print formated text
 *
 * Slow, since it has to make use of our kputch thingy
 * TODO: implement full fmt scema
 */
int printf(const char* fmt, ...)
{
    int error;

    va_list args;
    va_start(args, fmt);

    error = vprintf(fmt, args);

    va_end(args);
    return error;
}

static int __bufout(uint8_t typeflags, char c, char** out, size_t* p_cur_size)
{
    /* If there is no size left, don't try to do weird shit */
    if (!(*p_cur_size))
        return 0;

    /* Reduced the current size */
    (*p_cur_size)--;

    /* Put a character in the current working place */
    **out = c;
    /* Go to the next fucker */
    (*out)++;
    /* Null terminate just in case*/
    **out = '\0';

    return 0;
}

/*!
 * @brief: Unsafe string formating function
 *
 * This just keeps going until the format string is completely processed
 */
int sfmt(char* buf, const char* fmt, ...)
{
    int error;

    va_list args;
    va_start(args, fmt);

    error = _vprintf(NULL, fmt, args, __bufout, &buf, (size_t)-1ULL);

    va_end(args);

    return error;
}

/*!
 * @brief: Safer string formating function
 *
 * Here we also pass the size of the buffer we're formatting, so we don't overshoot it
 */
int sfmt_sz(char* buf, size_t bsize, const char* fmt, ...)
{
    int error;

    va_list args;
    va_start(args, fmt);

    error = _vprintf(NULL, fmt, args, __bufout, &buf, bsize);

    va_end(args);

    return error;
}

int vkdbgf(bool prefix, const char* fmt, va_list args)
{
    uint8_t flags = LOGGER_FLAG_DEBUG;

    if (!prefix)
        flags |= LOGGER_FLAG_NO_PREFIX;

    return _vprintf(flags, fmt, args, _kputch, NULL, NULL);
}

int kdbgf(const char* fmt, ...)
{
    int error;

    va_list args;
    va_start(args, fmt);

    error = vkdbgf(true, fmt, args);

    va_end(args);

    return error;
}

int kdbgln(const char* msg)
{
    int error;

    error = print_ex(LOGGER_FLAG_DEBUG, msg, LOG_TYPE_LINE);

    return error;
}

int kdbg(const char* msg)
{
    int error;

    error = print_ex(LOGGER_FLAG_DEBUG, msg, LOG_TYPE_DEFAULT);

    return error;
}

int kdbgc(char c)
{
    int error;

    error = _kputch(LOGGER_FLAG_DEBUG, c, NULL, NULL);

    return error;
}

int kwarnf(const char* fmt, ...)
{
    int error;

    va_list args;
    va_start(args, fmt);

    error = _vprintf(LOGGER_FLAG_WARNINGS, fmt, args, _kputch, NULL, NULL);

    va_end(args);

    return error;
}

int kwarnln(const char* msg)
{
    int error;

    error = print_ex(LOGGER_FLAG_WARNINGS, msg, LOG_TYPE_LINE);

    return error;
}

int kwarn(const char* msg)
{
    int error;

    error = print_ex(LOGGER_FLAG_WARNINGS, msg, LOG_TYPE_DEFAULT);

    return error;
}

int kwarnc(char c)
{
    int error;

    error = _kputch(LOGGER_FLAG_WARNINGS, c, NULL, NULL);

    return error;
}

/*!
 * @brief Init the logging system in its early form
 *
 * For when we don't yet have a heap and stuff
 */
void init_early_logging()
{
    __loggers_count = 0;
    __default_mutex = nullptr;
    memset(__loggers, 0, sizeof(__loggers));
}

/*!
 * @brief Initialize the locked logging system
 *
 * This should be called as soon as we have memory up to speed since
 * the logging system will be in an 'unsafe' state before that point
 */
void init_logging()
{
    __default_mutex = create_mutex(NULL);
    __log_lock = create_spinlock(SPINLOCK_FLAG_SOFT);
}
