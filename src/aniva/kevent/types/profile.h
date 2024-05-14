#ifndef __ANIVA_KEVENT_TYPE_PROFILE__
#define __ANIVA_KEVENT_TYPE_PROFILE__

#include "system/profile/profile.h"

enum KEVENT_PROFILE_TYPE {
  KEVENT_PROFILE_CHANGE,
  /* Rare */
  KEVENT_PROFILE_PRIV_CHANGE,
};

typedef struct kevent_profile_ctx {
  user_profile_t* old, *new;

  enum KEVENT_PROFILE_TYPE type;
} kevent_profile_ctx_t;

#endif // !__ANIVA_KEVENT_TYPE_PROFILE__
