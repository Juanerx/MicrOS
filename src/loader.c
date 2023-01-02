#include <proc.h>
#include <elf.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

#define NR_PAGE 8
#define PGSIZE 4096
#define OFFSET 0xfff
#define min(A,B) ((uintptr_t)(A) < (uintptr_t)(B) ? (A) : (B))
#define paddr_offset(A,B) ((void *)(((uintptr_t)(A) & (OFFSET)) | (uintptr_t)(((uintptr_t)(B) & ~OFFSET))))

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t get_ramdisk_size();
int fs_open(const char *pathname, int flags, int mode);
size_t fs_read(int fd, void *buf, size_t len);
int fs_close(int fd);
size_t fs_lseek(int fd, size_t offset, int whence);


static uintptr_t loader(PCB *pcb, const char *filename) {
  /* TODO */
	/* Read the first 4096 bytes from the exec_file.
	 * They should contain the ELF header and program headers. */
  int fd = fs_open(filename, 0, 0);
  // size_t ramdisk_size = fs_size(fd);
  uint8_t buf[sizeof(Elf_Ehdr)];
  fs_lseek(fd, 0, 0);
  assert(fs_read(fd, (void *)buf, sizeof(Elf_Ehdr)));

	/* The first several bytes contain the ELF header. */
  Elf_Ehdr *elf = (void *)buf;
  assert(*(uint32_t *)elf->e_ident == 0x464c457f);

  /* load segment to memory according to program header */
  for (int i = 0; i < elf->e_phnum; i++){
    Elf_Phdr phdr_entry;
    fs_lseek(fd, i * elf->e_phentsize + elf->e_phoff, 0);
    fs_read(fd, &phdr_entry, elf->e_phentsize);
    void *vaddr = NULL;
    void *paddr = NULL;
    uintptr_t file_vaddr = phdr_entry.p_vaddr + phdr_entry.p_filesz;
    uintptr_t mem_vaddr = phdr_entry.p_vaddr + phdr_entry.p_memsz;
    if (phdr_entry.p_type == PT_LOAD){
      fs_lseek(fd, phdr_entry.p_offset, 0);
      vaddr = (void *)(phdr_entry.p_vaddr);
      // fs_read(fd, (void *)(phdr_entry.p_vaddr), phdr_entry.p_memsz);
      while ((uintptr_t)(vaddr) < file_vaddr) {
        paddr = new_page(1);
        map(&pcb->as, vaddr, paddr, 0);
        size_t read_len = min(file_vaddr - (uintptr_t)vaddr, ((uintptr_t)vaddr & ~OFFSET) + PGSIZE - (uintptr_t)vaddr);
        fs_read(fd, paddr_offset(vaddr, paddr), read_len);
        vaddr += read_len;
      }
      // printf("vaddr: %p 2nd arg: %p\n", vaddr, phdr_entry.p_vaddr + phdr_entry.p_filesz);
      assert((uintptr_t)vaddr == file_vaddr);

      while ((uintptr_t)vaddr < mem_vaddr) {
        if (((uintptr_t)vaddr & OFFSET) == 0) {
          paddr = new_page(1);
          map(&pcb->as, vaddr, paddr, 0);
        }
        size_t read_len = min(((uintptr_t)vaddr & ~OFFSET) + PGSIZE - (uintptr_t)vaddr, mem_vaddr - (uintptr_t)vaddr);
        memset(paddr_offset(vaddr, paddr), 0, read_len);
        vaddr += read_len;
      }
      assert((uintptr_t)vaddr == mem_vaddr);
      pcb->max_brk = ROUNDUP((uintptr_t)vaddr, PGSIZE);
      // memset((void *)phdr_entry.p_vaddr + phdr_entry.p_filesz, 0, phdr_entry.p_memsz - phdr_entry.p_filesz); 
    }
  }

  fs_close(fd);
  return elf->e_entry;
}


void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void(*)())entry) (); // convert entry to function pointer and then call it (jump to the entry)
}

/* create context for kernel thread */
void context_kload(PCB *pcb, void (*entry)(void *), void *arg){
  Area karea;
  karea.start = &pcb->cp;
  karea.end = &pcb->cp + STACK_SIZE;

  pcb->cp = kcontext(karea, entry, arg);
  // printf("the address of kcontext: %p\n", pcb->cp);
}

/* Load user process */
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]){
  /* user stack */
  Area uarea;
  int argc = 0, envc = 0, size_argv = 0, size_envp = 0, size = 0;
  if (argv){
    while (argv[argc]){
      size_argv += strlen(argv[argc]) + 1;
      argc ++;
    }
  }
  if (envp){
    while (envp[envc]){
      size_envp += strlen(envp[envc]) + 1;
      envc ++;
    }
  }

  /* allocate page starting from heap.start */
  protect(&pcb->as);
  void *page_start = new_page(NR_PAGE);
  void *page_end = page_start + NR_PAGE * PGSIZE;
  for (int i = 1; i <= NR_PAGE; i++) {
    map(&pcb->as, pcb->as.area.end - i * PGSIZE, page_end - i * PGSIZE, 0);
    // printf("user pdir: %x\n", page_end - i * PGSIZE);
  }

  size = size_argv + size_envp + sizeof(uintptr_t) * (argc + envc + 3);
  void *ustack = page_end - size;
  void *args_start = ustack;
  void *str_start = ustack + sizeof(uintptr_t) * (argc + envc + 3);
  uarea.start = (void *)ustack;
  uarea.end = (void *)(ustack + size);

  *(uintptr_t *)args_start = argc;
  for (int i = 0; i < argc; i++){
    *(uintptr_t *)(args_start + sizeof(uintptr_t) * (i + 1)) = (uintptr_t)str_start;
    memcpy(str_start, argv[i], strlen(argv[i]) + 1);
    str_start += strlen(argv[i]) + 1;
  }
  *(uintptr_t *)(args_start + sizeof(uintptr_t) * (argc + 1)) = 0;

  for (int i = 0; i < envc; i++){
    *(uintptr_t *)(args_start + sizeof(uintptr_t) * (argc + 2 + i)) = (uintptr_t)str_start;
    memcpy(str_start, envp[i], strlen(envp[i]) + 1);
    str_start += strlen(envp[i]) + 1;
  }
  *(uintptr_t *)(args_start + sizeof(uintptr_t) * (argc + envc + 2)) = 0;

  /* kernel stack */
  Area karea;
  karea.start = &pcb->cp;
  karea.end = &pcb->cp + STACK_SIZE;
  uintptr_t entry = loader(pcb, filename);
  pcb->cp = ucontext(&pcb->as, karea, (void *)entry);
  // pcb->cp->mscratch = (uintptr_t)uarea.start; // ???不用加
  pcb->cp->GPRx = (uintptr_t)uarea.start;

  // printf("argc: %d\n", argc);
  // printf("pcb->cp->GPRx: %d\n", *(uintptr_t *)pcb->cp->GPRx);
  // printf("address of argc: %x\n", pcb->cp->GPRx);
}
