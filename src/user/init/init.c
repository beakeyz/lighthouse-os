
int Main() {

  unsigned long long result = 0;
  unsigned long long function = 0;

  asm volatile("syscall"
                 : "=a"(result)
                 : "a"(function)
                 : "rcx", "r11", "memory");

  return 999;
}
