#ifndef __UTILS_H__
#define __UTILS_H__

int util_set_max_files(int max_files);

int daemonize(int nochdir, int noclose);

int util_print_buffer(char *hint, buffer_t * b);

int url_encode(const char *src, int src_len, char *dest, int *dest_len);

size_t url_decode(const char *src, char *dest);

#endif
