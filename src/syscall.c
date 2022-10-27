#include <common.h>
#include "syscall.h"
#include <sys/time.h>
#include <stdint.h>
#include <proc.h>

int fs_open(const char *pathname, int flags, int mode);
size_t fs_read(int fd, void *buf, size_t len);
int fs_close(int fd);
size_t fs_lseek(int fd, size_t offset, int whence);
size_t fs_write(int fd, const void *buf, size_t len);
void naive_uload(PCB *pcb, const char *filename);

void strace(uintptr_t *p){
  switch (*p)
  {
  case SYS_brk:
    Log("system call: %s  new_program_break: %x", sys_num[*p], *(p + 1));
    break;
  
  default: Log("system call: %s", sys_num[*p]);
    break;
  }
}

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

  // strace(a);

  switch (a[0]) {
    case SYS_yield: 
      yield(); 
      c->GPRx = 0; 
      break;

    case SYS_write: 
      // if (a[1] == 1 || a[1] == 2){
      //   for (int i = 0; i < a[3]; i++){
      //     putch(*((char *)a[2] + i));
      //   }
      //   c->GPRx = a[3];
      // }
      // else{
        c->GPRx = fs_write(a[1], (void *)a[2], a[3]);
      // }
      break;

    case SYS_open: 
      c->GPRx = fs_open((void *)a[1], a[2], a[3]); 
      break;

    case SYS_read: 
      c->GPRx = fs_read(a[1], (void *)a[2], a[3]);
      break;

    case SYS_close: 
      c->GPRx = fs_close(a[1]);
      break;

    case SYS_lseek: 
      c->GPRx = fs_lseek(a[1], a[2], a[3]);
      break;

    case SYS_brk:
      c->GPRx = 0;
      break;

    case SYS_gettimeofday:
        ; // empty statement 
      struct timeval *tv = (struct timeval *)a[1];
      __uint64_t time = io_read(AM_TIMER_UPTIME).us;
      tv->tv_sec = (time / 1000000);
      tv->tv_usec = (time % 1000000);
      c->GPRx = 0;
      break;

    case SYS_execve:
        ; // empty statement 
      const char *envp = (const char *)a[3];
      char pathname[64];
      /* assuming only one environment value.
        user should input e.g. nslider */
      if (strlen(envp) > 2)
        sprintf(pathname, "%s/%s", envp, (const char*)a[1]);
      else
        sprintf(pathname, "%s", (const char*)a[1]);
      naive_uload(NULL, pathname);
      break;

    case SYS_exit:  
      // halt(0);
      naive_uload(NULL, "/bin/menu");
      break;

    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
