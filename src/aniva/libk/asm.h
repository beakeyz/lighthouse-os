#ifndef __ANIVA_LIBK_ASM__
#define __ANIVA_LIBK_ASM__

#ifdef __GCC_ASM_FLAG_OUTPUTS__
#define CC_SET(c) "\n\t/* output condition code " #c "*/\n"
#define CC_OUT(c) "=@cc" #c
#else
#define CC_SET(c) "\n\tset" #c " %[_cc_" #c "]\n"
#define CC_OUT(c) [_cc_##c] "=qm"
#endif

#endif //__ANIVA_LIBK_ASM__
