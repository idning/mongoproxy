/*
 * file   : test_ptrace.c
 * author : ning
 * date   : 2012-01-13 16:10:27
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "buffer.h"

int util_set_max_files(int max_files)
{
    struct rlimit rls;
    rls.rlim_cur = max_files;
    rls.rlim_max = max_files;

    if (setrlimit(RLIMIT_NOFILE, &rls) < 0) {
        /*fprintf(stderr, "can't change open files limit to %u\n", max_files); */
        WARNING("can't change open files limit to %u", max_files);
    }
    return 0;
}

int daemonize(int nochdir, int noclose)
{
    int fd;

    switch (fork()) {
    case -1:
        return (-1);
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    if (setsid() == -1)
        return (-1);

    if (nochdir == 0) {
        if (chdir("/") != 0) {
            perror("chdir");
            return (-1);
        }
    }

    if (noclose == 0 && (fd = open("/dev/null", O_RDWR, 0)) != -1) {
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 stdin");
            return (-1);
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            return (-1);
        }
        if (dup2(fd, STDERR_FILENO) < 0) {
            perror("dup2 stderr");
            return (-1);
        }

        if (fd > STDERR_FILENO) {
            if (close(fd) < 0) {
                perror("close");
                return (-1);
            }
        }
    }
    return (0);
}



char _hex2int(unsigned char hex) {
    hex = hex - '0';
    if (hex > 9) {
        hex = (hex + '0' - 1) | 0x20;
        hex = hex - 'a' + 11;
    }
    if (hex > 15)
        hex = 0xFF;

    return hex;
}



u_char *
util_raw_to_hex(u_char *dst, u_char *src, size_t src_len)
{
    static u_char  hex[] = "0123456789ABCDEF";

    while (src_len--) {
        *dst++ = hex[*src >> 4];
        *dst++ = hex[*src++ & 0xf];
    }

    return dst;
}


u_char *
util_hex_to_raw(u_char *dst, u_char *src, size_t src_len){
    int high, low;
    size_t i;
    u_char * p = src;
    for (i = 0; i < src_len/2; i++) {
        high = _hex2int(*p);
        low = _hex2int(*(p+1));
        p+=2;
        dst[i] = (high << 4) | low;
    }
    return dst+i;
}

//urlencode & urldecode
//i need modify it!! 

static const char c2x_table[] = "0123456789ABCDEF";

static unsigned char *c2x(unsigned what, unsigned char *where)
{
    *where++ = '%';
    *where++ = c2x_table[what >> 4];
    *where++ = c2x_table[what & 0xf];
    return where;
}

static char x2c(const char *what)
{
    register char digit;

    digit = ((what[0] >= 'A') ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
    return (digit);
}

static int unsafechar(unsigned char ch)
{
    unsigned char *p = &ch;

    if (*p == ' ' || *p == 0 || *p == '%' || *p == '\\' || *p == '^' || *p == '[' || *p == ']' || *p == '`'
        || *p == '+' || *p == '$' || *p == ',' || *p == '@' || *p == ':' || *p == ';'
        || *p == '/' || *p == '!' || *p == '#' || *p == '?' || *p == '=' || *p == '&' || /**p=='.'||*/ *p > 0x80) {
        return (1);
    } else {
        return (0);
    }
}

size_t url_decode(const char *src, char *dest)
{
    char *cp = dest;

    while (*src != '\0') {
        if (*src == '+') {
            *dest++ = ' ';
        } else if (*src == '%') {
            int ch;
            ch = x2c(src + 1);
            *dest++ = ch;
            src += 2;
        } else {
            *dest++ = *src;
        }
        src++;
    }
    *dest = '\0';
    return (dest - cp);
}

int url_encode(const char *src, int src_len, char *dest, int *dest_len)
{
    char *cp = dest;
    int i = 0;
    while (i < src_len && *src != '\0') {
        unsigned char *p = (unsigned char *)src;
        if (*p == ' ') {
            *dest++ = '+';
        } else if (unsafechar(*p)) {
            unsigned char w[3] = { '\0' };
            c2x(*p, w);
            *dest = w[0];
            *(dest + 1) = w[1];
            *(dest + 2) = w[2];
            dest += 3;
        } else {
            *dest++ = *p;
        }
        i++;
        src++;
    }
    *dest = '\0';
    *dest_len = dest - cp;
    return *dest_len;
}


#define _min(x,y) ((x)<(y)?(x):(y))

int util_print_buffer(char * hint, buffer_t * b){
    char dst[1024*4*2];
    int dst_len;
    int src_len = _min(b->used, 1024*4);
    DEBUG("src_len : %d", src_len);
    util_raw_to_hex(dst, b->ptr, src_len);

    /*url_encode(b->ptr, src_len, dst, &dst_len);*/
    DEBUG("%s print_buffer(url_encoded): %s", hint, dst);

    return 0;
}

