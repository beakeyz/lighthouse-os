#ifndef __ANIVA_PROC_PENV__
#define __ANIVA_PROC_PENV__

#include "sync/mutex.h"
#include <libk/stddef.h>

struct proc;
struct sysvar;
struct oss_node;
struct user_profile;

/*
 * Process environment
 *
 * Groups processes together, which share privileges, ctlvars, ect.
 */
typedef struct penv {
  const char* label;
  uint16_t flags;
  /* This goes from 0x00 to 0xff. Anything beyond that is considered invalid */
  uint16_t priv_level;

  mutex_t* lock;

  struct oss_node* node;
  struct user_profile* profile;
} penv_t;

penv_t* create_penv(const char* label, uint32_t priv_level, uint32_t flags);
void destroy_penv(penv_t* env);

int penv_add_proc(penv_t* env, struct proc* p);
int penv_remove_proc(penv_t* env, struct proc* p);

#endif // !__ANIVA_PROC_PENV__
