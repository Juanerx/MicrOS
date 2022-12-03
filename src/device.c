#include <common.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  yield();
  for (int i = 0; i < len; i ++){
    putch(*((char *)buf + i));
  }
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  yield();
  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  if (ev.keycode == AM_KEY_NONE) return 0;
  const char *key_down = ev.keydown ? "kd " : "ku ";
  const char *key_type = keyname[ev.keycode];
  strcpy(buf, key_down);
  strcat(buf, key_type);
  strcat(buf, "\n"); // need inspection
  size_t read_len = strlen(buf);
  assert(read_len <= len);
  return read_len;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  int w = io_read(AM_GPU_CONFIG).width;
  int h = io_read(AM_GPU_CONFIG).height;
  return sprintf(buf, "WIDTH:%d\nHEIGHT:%d\n", w, h);
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  yield();
  int pixel_size = sizeof(uint32_t);
  int w = io_read(AM_GPU_CONFIG).width;
  int h = io_read(AM_GPU_CONFIG).height;
  int x = (offset / pixel_size) % w;
  int y = (offset / pixel_size) / w;
  if(offset + len > w * h * pixel_size) 
    len = w * h * pixel_size - offset;
  // draw a w * h rect at (x, y)
  io_write(AM_GPU_FBDRAW, x, y, (uint32_t*)buf, len / pixel_size, 1, true);
  //io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);
  return len;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
