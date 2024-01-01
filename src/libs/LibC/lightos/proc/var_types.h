#ifndef __LIGHTENV_SYS_VAR_TYPES__
#define __LIGHTENV_SYS_VAR_TYPES__

/*
 * The different types that a profile variable can hold
 *
 * FIXME: we should make this naming sceme consistent with PVAR_FLAG___ naming
 */
enum PROFILE_VAR_TYPE {
  PROFILE_VAR_TYPE_STRING = 0,
  PROFILE_VAR_TYPE_BYTE,
  PROFILE_VAR_TYPE_WORD,
  PROFILE_VAR_TYPE_DWORD,
  PROFILE_VAR_TYPE_QWORD,
};

/* Open for anyone to read */
#define PVAR_FLAG_GLOBAL (0x00000001)
/* Open for anyone to modify? */
#define PVAR_FLAG_CONSTANT (0x00000002)
/* Holds data that the system probably needs to function correctly */
#define PVAR_FLAG_VOLATILE (0x00000004)
/* Holds configuration data */
#define PVAR_FLAG_CONFIG (0x00000008)
/* Hidden to any profiles with lesser permissions */
#define PVAR_FLAG_HIDDEN (0x00000010)

#endif // !__LIGHTENV_SYS_VAR_TYPES__
