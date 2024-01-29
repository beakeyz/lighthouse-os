
extern void __init_libc(void);

/*!
 * @brief: Entry for shared library
int lib_entry(void)
{
  __init_libc();
  return 0;
}
 */
