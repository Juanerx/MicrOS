#include <proc.h>
#include <elf.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

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
    if (phdr_entry.p_type == PT_LOAD){
      fs_lseek(fd, phdr_entry.p_offset, 0);
      fs_read(fd, (void *)(phdr_entry.p_vaddr), phdr_entry.p_memsz);
      memset((void *)phdr_entry.p_vaddr + phdr_entry.p_filesz, 0, phdr_entry.p_memsz - phdr_entry.p_filesz); 
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

