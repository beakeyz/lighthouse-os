#include "parser.h"
#include "entry/entry.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/multiboot.h"
#include "mem/heap.h"

static char* _cmdline;
static size_t _cmdline_size;
static hashmap_t* _opt_map;

#define CMDLINE_TOK_NEWOPT ' '
#define CMDLINE_TOK_EQ '='
#define CMDLINE_TOK_OPTVAL '\''

/*!
 * @brief: Try to infer the kopt type from the value it's been given
 */
static enum KOPT_TYPE _determine_opt_type(const char* val)
{
    uint64_t i;

    if (strlen(val) == 1 && (*val == '1' || *val == '0'))
        return KOPT_TYPE_BOOL;

    i = 0;

    /* Check if there are any non-digits in the opt */
    do {
        if (val[i] < '0' || val[i] > '9')
            return KOPT_TYPE_STR;

        i++;
    } while (val[i]);

    return KOPT_TYPE_NUM;
}

/*!
 * @brief: Create a single kopt
 */
static kopt_t* _create_kopt(const char* key, const char* val)
{
    kopt_t* opt;

    /* The first char should always be non-null */
    if (!key[0] || !val[0])
        return nullptr;

    opt = kmalloc(sizeof(*opt));

    if (!opt)
        return nullptr;

    opt->key = strdup(key);
    opt->type = _determine_opt_type(val);

    switch (opt->type) {
    case KOPT_TYPE_BOOL:
        opt->bool_value = (val[0] == '0' ? false : true);
        break;
    case KOPT_TYPE_STR:
        opt->str_value = strdup(val);
        break;
    case KOPT_TYPE_NUM:
        kernel_panic("TODO: parse numbers from the cmdline");
        break;
    }

    return opt;
}

/*!
 * @brief: Destroy a single kopts
 */
void _destroy_kopt(kopt_t* opt)
{
    if (opt->type == KOPT_TYPE_STR)
        kfree((void*)opt->str_value);

    kfree((void*)opt->key);
    kfree(opt);
}

/*!
 * @brief: Itterator callback to destroy all the kopts in the _opt_map
 */
static kerror_t _destroy_opts(void* val, uint64_t a, uint64_t b)
{
    kopt_t* opt;

    (void)a;
    (void)b;

    if (!val)
        return (0);

    opt = (kopt_t*)val;

    _destroy_kopt(opt);
    return (0);
}

/*!
 * @brief: Destroy the entire cmdline parser
 *
 * Called when the parse fails
 */
void destroy_cmdline_parser()
{
    hashmap_itterate(_opt_map, NULL, _destroy_opts, NULL, NULL);
    destroy_hashmap(_opt_map);

    _opt_map = nullptr;
}

#define __SCAN_TOKEN(c)         \
    do {                        \
        i++;                    \
        if (i >= _cmdline_size) \
            return -KERR_INVAL; \
        if (_cmdline[i] == (c)) \
            _cmdline[i] = NULL; \
    } while (_cmdline[i])

/*!
 * @brief: Actually parse the cmdline
 *
 */
static kerror_t _start_parse()
{
    char c_char;
    const char* opt_key;
    const char* opt_val;
    kopt_t* c_opt;

    for (uint64_t i = 0; i < _cmdline_size; i++) {
        c_char = _cmdline[i];

        /* Reached the end */
        if (!c_char)
            break;

        /* A space indicates a new char */
        if (c_char != CMDLINE_TOK_NEWOPT)
            continue;

        /* Next char should be the opt key */
        i++;

        opt_key = (const char*)&_cmdline[i];

        __SCAN_TOKEN(CMDLINE_TOK_EQ);

        /* Next char should be CMDLINE_TOK_OPTVAL */
        i++;

        if (_cmdline[i++] != CMDLINE_TOK_OPTVAL)
            return -KERR_INVAL;

        opt_val = (const char*)&_cmdline[i];

        /* Loop until we find the closing CMDLINE_TOK_OPTVAL char */
        __SCAN_TOKEN(CMDLINE_TOK_OPTVAL);

        c_opt = _create_kopt(opt_key, opt_val);

        if (!c_opt)
            return -KERR_INVAL;

        /* Register it in our hashmap */
        hashmap_put(_opt_map, (hashmap_key_t)c_opt->key, c_opt);
    }

    return KERR_NONE;
}

/*!
 * @brief: Gets a boolean opt
 *
 * @returns: A boolean with the value of the opt, if it exists. Otherwise false
 */
bool opt_parser_get_bool(const char* boolkey)
{
    bool ret;

    if (!KERR_OK(opt_parser_get_bool_ex(boolkey, &ret)))
        return false;

    return ret;
}

/*!
 * @brief: Extended version of getting a boolean opt
 *
 * @returns: A kerror code if something went wrong, otherwise the boolean value gets put
 * into the @bval buffer object
 */
kerror_t opt_parser_get_bool_ex(const char* boolkey, bool* bval)
{
    kopt_t* opt;

    if (!_opt_map)
        return -KERR_INVAL;

    opt = hashmap_get(_opt_map, (hashmap_key_t)boolkey);

    if (!opt || opt->type != KOPT_TYPE_BOOL)
        return -KERR_INVAL;

    *bval = opt->bool_value;
    return KERR_NONE;
}

kerror_t opt_parser_get_str(const char* strkey, const char** bstr)
{
    kernel_panic("TODO: opt_parser_get_str");
}

kerror_t opt_parser_get_num(const char* numkey, uint64_t* bval)
{
    kernel_panic("TODO: opt_parser_get_num");
}

/*!
 * @brief: Initialize the kernel commandline parser
 *
 * Grab the commandline from the multiboot header and try to parse it
 */
void init_cmdline_parser()
{
    kerror_t error;

    if (!g_system_info.cmdline)
        return;

    g_system_info.sys_flags |= SYSFLAGS_HAS_CMDLINE;

    _opt_map = create_hashmap(128, HASHMAP_FLAG_SK);
    _cmdline = g_system_info.cmdline->string;
    _cmdline_size = g_system_info.cmdline->size - sizeof(struct multiboot_tag_string);

    error = _start_parse();

    /* If the parse fails, this means the cmdline is probably corrupted and we can't use it =/ */
    if (error) {
        destroy_cmdline_parser();
        g_system_info.sys_flags &= ~SYSFLAGS_HAS_CMDLINE;
    }
}
