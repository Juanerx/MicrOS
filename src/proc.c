#include <proc.h>

#define MAX_NR_PROC 4

void naive_uload(PCB *pcb, const char *filename);
void context_kload(PCB *pcb, void (*entry)(void *), void *arg);
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]);

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  int j = 1;
  while (1) {
    if (j % 1000 == 0)
      Log("Hello World from Nanos-lite with arg %s for the %dth time!", (char *)arg, j / 1000);
    j ++;
    // yield();
  }
}

void init_proc() {
  // char *argv1[] = {"--skip", NULL};
  // char *argv2[] = {"/bin/nterm", NULL};
  // context_kload(&pcb[1], hello_fun, "hello");
  // context_uload(&pcb[1], "/bin/pal", NULL, NULL);
  context_uload(&pcb[0], "/bin/hello", NULL, NULL);
  context_uload(&pcb[1], "/bin/nterm", NULL, NULL);
  switch_boot_pcb();

  Log("Initializing processes...");

  // load program here
  // naive_uload(NULL, "/bin/dummy");
}

Context* schedule(Context *prev) {
  // save the context pointer
  current->cp = prev;

  // always select pcb[0] as the new process
  current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
  // printf("sp and ksp is %p, %p\n", current->cp, current->cp->np);
  // then return the new context
  return current->cp;
}
