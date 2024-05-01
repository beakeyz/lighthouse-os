#ifndef __ANIVA_SYSVAR_LOADER__
#define __ANIVA_SYSVAR_LOADER__

struct file;
struct sysvar_map;

int sysvarldr_load_variables(struct sysvar_map* profile, struct file* file);

#endif // !__ANIVA_SYSVAR_LOADER__
