#include <fs.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t open_offset; // record the read/write offset
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);
size_t serial_write(const void *buf, size_t offset, size_t len);
size_t events_read(void *buf, size_t offset, size_t len);
size_t dispinfo_read(void *buf, size_t offset, size_t len);
size_t fb_write(const void *buf, size_t offset, size_t len);

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin", 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
  [FD_FB] = {"/dev/fb", 0, 0, invalid_read, fb_write},
  {"/dev/events", 0, 0, events_read, fb_write},
  {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write},
#include "files.h"
};

void init_fs() {
  // TODO: initialize the size of /dev/fb
  int w = io_read(AM_GPU_CONFIG).width;
  int h = io_read(AM_GPU_CONFIG).height;
  file_table[FD_FB].size = w * h * sizeof(uint32_t);
}

int fs_open(const char *pathname, int flags, int mode){
  for (int i = 0; i < sizeof(file_table) / sizeof(Finfo); i ++){
    if (strcmp(pathname, file_table[i].name) == 0){
      return i;
    }
  }
  printf("failed to open %s\n", pathname);
  // assert(0);
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len){
  // printf("len:%d open_offset:%d file_size:%d\n", len, file_table[fd].open_offset, file_table[fd].size);
  // assert(len <= (file_table[fd].size - file_table[fd].open_offset));
  Finfo *file = &file_table[fd];
  size_t read_len;
  if (file->read){
    read_len = file->read(buf, file->open_offset, len);
    file->open_offset += read_len;
  }
  else{
    read_len = len <= file->size - file->open_offset ? len : file->size - file->open_offset;
    ramdisk_read(buf, file->disk_offset + file->open_offset, read_len);
    file->open_offset += read_len;
  }
  return read_len;
}

size_t fs_write(int fd, const void *buf, size_t len){
  Finfo *file = &file_table[fd];
  // assert(len <= (file_table[fd].size - file_table[fd].open_offset));
  // ramdisk_write(buf, file_table[fd].disk_offset + file_table[fd].open_offset, len);
  // file_table[fd].open_offset += len;
  if (file->write){
    file->write(buf, file->open_offset, len);
  }
  else{
    assert(len <= (file->size - file->open_offset));
    ramdisk_write(buf, file->disk_offset + file->open_offset, len);
  }
  file->open_offset += len;
  return len;
}

int fs_close(int fd){
  /* always success */
  file_table[fd].open_offset = 0;
  return 0;
}

size_t fs_lseek(int fd, size_t offset, int whence){
  switch (whence)
  {
  case SEEK_SET:
    file_table[fd].open_offset = offset;
    break;

  case SEEK_CUR:
    file_table[fd].open_offset += offset;
    break;

  case SEEK_END:
    file_table[fd].open_offset = file_table[fd].size + offset;
    break;

  default:
    assert(0);
  }

  return file_table[fd].open_offset;
}
