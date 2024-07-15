#ifndef __LIGHTOS_PARAMS_HEADER__
#define __LIGHTOS_PARAMS_HEADER__

#include "sys/types.h"

enum CMD_PARAM_TYPE {
    CMD_PARAM_TYPE_BOOL,
    CMD_PARAM_TYPE_NUM,
    CMD_PARAM_TYPE_STRING,
};

typedef struct cmd_param {
    union {
        void* ptr;
        BOOL* b;
        uint64_t* n;
        char* str;
    } dest;
    uint32_t dest_len;
    enum CMD_PARAM_TYPE type;
    char flag;
    const char* desc;
    /* Single parameter can have a max of 5 aliases */
    const char* aliases[5];
} cmd_param_t;

#define CMD_PARAM_5_ALIAS(dest, dest_len, type, flag, desc, alias_1, alias_2, alias_3, alias_4, alias_5) \
    {                                                                                                    \
        { (void*)&dest }, dest_len, type, flag, desc, {                                                  \
            alias_1,                                                                                     \
            alias_2,                                                                                     \
            alias_3,                                                                                     \
            alias_4,                                                                                     \
            alias_5,                                                                                     \
        },                                                                                               \
    }

#define CMD_PARAM_4_ALIAS(dest, dest_len, type, flag, desc, alias_1, alias_2, alias_3, alias_4) CMD_PARAM_5_ALIAS(dest, dest_len, type, flag, desc, alias_1, alias_2, alias_3, alias_4, NULL)
#define CMD_PARAM_3_ALIAS(dest, dest_len, type, flag, desc, alias_1, alias_2, alias_3) CMD_PARAM_5_ALIAS(dest, dest_len, type, flag, desc, alias_1, alias_2, alias_3, NULL, NULL)
#define CMD_PARAM_2_ALIAS(dest, dest_len, type, flag, desc, alias_1, alias_2) CMD_PARAM_5_ALIAS(dest, dest_len, type, flag, desc, alias_1, alias_2, NULL, NULL, NULL)
#define CMD_PARAM_1_ALIAS(dest, dest_len, type, flag, desc, alias_1) CMD_PARAM_5_ALIAS(dest, dest_len, type, flag, desc, alias_1, NULL, NULL, NULL, NULL)

#define CMD_PARAM_AUTO_5_ALIAS(dest, type, flag, desc, alias_1, alias_2, alias_3, alias_4, alias_5) CMD_PARAM_5_ALIAS(dest, sizeof(dest), type, flag, desc, alias_1, alias_2, alias_3, alias_4, alias_5)
#define CMD_PARAM_AUTO_4_ALIAS(dest, type, flag, desc, alias_1, alias_2, alias_3, alias_4) CMD_PARAM_4_ALIAS(dest, sizeof(dest), type, flag, desc, alias_1, alias_2, alias_3, alias_4)
#define CMD_PARAM_AUTO_3_ALIAS(dest, type, flag, desc, alias_1, alias_2, alias_3) CMD_PARAM_3_ALIAS(dest, sizeof(dest), type, flag, desc, alias_1, alias_2, alias_3)
#define CMD_PARAM_AUTO_2_ALIAS(dest, type, flag, desc, alias_1, alias_2) CMD_PARAM_2_ALIAS(dest, sizeof(dest), type, flag, desc, alias_1, alias_2)
#define CMD_PARAM_AUTO_1_ALIAS(dest, type, flag, desc, alias_1) CMD_PARAM_1_ALIAS(dest, sizeof(dest), type, flag, desc, alias_1)

#define CMD_PARAM_AUTO(dest, type, flag, desc) CMD_PARAM_1_ALIAS(dest, sizeof(dest), type, flag, desc, NULL)

extern int params_parse(const char** argv, uint32_t argc, cmd_param_t* params, uint32_t param_count);

#endif // !__LIGHTOS_PARAMS_HEADER__
