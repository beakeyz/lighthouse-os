#ifndef __LIGHTENV_SYS_VAR_TYPES__
#define __LIGHTENV_SYS_VAR_TYPES__

/*
 * The different types that a profile variable can hold
 */
enum PROFILE_VAR_TYPE {
  PROFILE_VAR_TYPE_STRING = 0,
  PROFILE_VAR_TYPE_BYTE,
  PROFILE_VAR_TYPE_WORD,
  PROFILE_VAR_TYPE_DWORD,
  PROFILE_VAR_TYPE_QWORD,
};

#endif // !__LIGHTENV_SYS_VAR_TYPES__