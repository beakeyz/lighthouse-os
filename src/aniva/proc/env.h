#ifndef __ANIVA_PROC_PENV__
#define __ANIVA_PROC_PENV__

#include "libk/data/linkedlist.h"
#include "lightos/proc/var_types.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct proc;
struct sysvar;
struct sysvar_map;
struct user_profile;

/*
 * Process environment
 *
 * Groups processes together, which share privileges, ctlvars, ect.
 */
typedef struct penv {
  const char* label;
  uint32_t id;
  uint16_t flags;
  /* This goes from 0x00 to 0xff. Anything beyond that is considered invalid */
  uint16_t priv_level;

  mutex_t* lock;

  /* Map for the variables */
  struct sysvar_map* vars;
  struct user_profile* profile;
  list_t* processes;
} penv_t;

penv_t* create_penv(const char* label, uint32_t priv_level, uint32_t flags, struct user_profile* profile);
void destroy_penv(penv_t* env);

int penv_add_proc(penv_t* env, struct proc* p);
int penv_remove_proc(penv_t* env, struct proc* p);

struct sysvar* penv_get_sysvar(penv_t* env, const char* key);
kerror_t penv_add_sysvar(penv_t* env, const char* key, enum SYSVAR_TYPE type, uint8_t flags, uintptr_t value);

#endif // !__ANIVA_PROC_PENV__
