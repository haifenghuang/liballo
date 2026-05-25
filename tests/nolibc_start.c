extern int main(int argc, char **argv);

// Manual system calls for x86_64 Linux
static inline long sys_syscall1(long n, long a1) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1)
                   : "rcx", "r11", "memory");
  return ret;
}

#define SYS_exit_group 231

void _start(void) {
  long argc;
  char **argv;

  // On x86_64, the stack layout at _start is:
  // [rsp] = argc
  // [rsp + 8] = argv[0]
  // ...
  // [rsp + 8*argc] = argv[argc-1]
  // [rsp + 8*argc + 8] = NULL
  __asm__ volatile("movq (%%rsp), %0\n"
                   "leaq 8(%%rsp), %1\n"
                   : "=r"(argc), "=r"(argv));

  int ret = main((int)argc, argv);
  sys_syscall1(SYS_exit_group, ret);

  // Should not reach here
  for (;;)
    ;
}
