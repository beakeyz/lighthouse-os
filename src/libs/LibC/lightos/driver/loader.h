#ifndef __ANIVA_DRV_APP_LOADER_API__
#define __ANIVA_DRV_APP_LOADER_API__

/* 
 * Define constants for the dyn_ldr driver
 */

#define DYN_LDR_NAME "dynldr"
#define DYN_LDR_URL "other/dynldr"

struct dynamic_library;
typedef struct dynamic_library dynamic_library_t;

enum DYNLDR_MSG_CODE {
  /* Ask the loader to load an entire app with all it's shared libraries */
  DYN_LDR_LOAD_APP = 150,
  /* Same as DYN_LDR_LOAD_APP but we pass a file obj */
  DYN_LDR_LOAD_APPFILE,
  /* Called from a process to ask the loader to load a library at runtime */
  DYN_LDR_LOAD_LIB,
  /*
   * Called from a process to ask the loader for the address of 
   * an exported function inside a dynamically linked binary
   * IN: (const char*) symbol name of the exported function
   * OUT: (u64) virtual address of the function entry
   */
  DYN_LDR_GET_FUNC_ADDR,
  /*
   * Grabs the best matching symname for a given address
   * IN: (u64) virtual address of the function
   * OUT: (const char*) ref to the symname
   */
  DYN_LDR_GET_FUNC_NAME,
  /*
   * Return a handle to a loaded library 
   * IN: (const char*) name of the library
   * OUT: (HANDLE) handle to the library
   */
  DYN_LDR_GET_LIB,
};

typedef struct dynldr_getfuncname_msg {
  unsigned int pid;
  void* func_addr;
} dynldr_getfuncname_msg_t;

//typedef int (*DYNAPP_ENTRY_t)(int argc, char** argv, char* envp);
typedef int (*DYNAPP_ENTRY_t)();
typedef int (*DYNLIB_ENTRY_t)();

#endif // !__ANIVA_DRV_APP_LOADER_API__
