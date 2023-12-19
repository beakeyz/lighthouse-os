#include "log.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include <sync/mutex.h>

static mutex_t* __default_mutex;
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

    __loggers[i] = __loggers[i+1];

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

  for (uint16_t i = 0; i < __loggers_count; i++) {
    target = __loggers[i];

    if (!target)
      break;

    if (strcmp(title, target->title) == 0)
      break;
  }

  if (!target)
    return -1;

  try_lock();

  memcpy(buffer, target, sizeof(logger_t));

  try_unlock();
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


void log(const char* msg)
{
  log_ex(0, msg, nullptr, LOG_TYPE_DEFAULT);
}

void logf(const char* fmt, ...)
{
  kernel_panic("TODO: implement logf");
}

void logln(const char* msg)
{
  log_ex(0, msg, nullptr, LOG_TYPE_LINE);
}

/*!
 * @brief Extended generic logging function
 *
 * Nothing to add here...
 */
void log_ex(logger_id_t id, const char* msg, va_list args, uint8_t type)
{
  static const char* waring_str = "[Warning]: ";
  static const char* error_str = "[ERROR]: ";
  size_t msg_len;
  size_t msg_offset;
  logger_t* target;

  if (!valid_logger_id(id))
    return;

  target = __loggers[id];

  if (!target)
    return;

  msg_len = NULL;
  msg_offset = 0;

  if (msg)
    msg_len = strlen(msg);

  /*
   * Add a lil offset if we are specifying warning or error prefixes
   */
  if (type == LOG_TYPE_WARNING) {
    msg_len += strlen(waring_str);
  } else if (type == LOG_TYPE_ERROR) {
    msg_len += strlen(error_str);
  }

  /* FIXME: heap-alloc? */
  char msg_buffer[msg_len + 1];
  memset(msg_buffer, 0, msg_len + 1);

  /*
   * Very ugly bit of type intercepting
   */
  if (type == LOG_TYPE_WARNING) {
    memcpy(&msg_buffer[0], waring_str, strlen(waring_str));
    msg_offset += strlen(waring_str);

    type = LOG_TYPE_LINE;
  } else if (type == LOG_TYPE_ERROR) {
    memcpy(&msg_buffer[0], error_str, strlen(error_str));
    msg_offset += strlen(error_str);

    type = LOG_TYPE_LINE;
  }

  if (msg)
    memcpy(&msg_buffer[msg_offset], msg, strlen(msg));

  switch (type) {
    case LOG_TYPE_DEFAULT:
      {
        if (target->f_log)
          target->f_log(msg_buffer);
        break;
      }
    case LOG_TYPE_CHAR:
      {
        if (target->f_logc)
          target->f_logc(msg_buffer[0]);

        break;
      }
    case LOG_TYPE_LINE:
      {
        if (target->f_logln)
          target->f_logln(msg_buffer);
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
static int print_ex(const char* msg, uint8_t type)
{
  logger_t* current;

  for (uint16_t i = 0; i < __loggers_count; i++) {

    current = __loggers[i];

    /* Skip these loggers */
    if ((current->flags & LOGGER_FLAG_NO_CHAIN))
      continue;

    log_ex(i, msg, nullptr, type);
  }

  return 0;
}

int putch(char c) 
{
  char buff[2] = { 0 };
  buff[0] = c;

  return print_ex(buff, LOG_TYPE_CHAR);
}

int print(const char* msg)
{
  return print_ex(msg, LOG_TYPE_DEFAULT);
}

int println(const char* msg)
{
  return print_ex(msg, LOG_TYPE_LINE);
}

int printf(const char* fmt, ...)
{
  kernel_panic("TODO: implement kernel printf");
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
}
