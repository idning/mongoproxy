/*
 * file   : buf.c
 * author : ning
 * date   : 2012-09-25 00:28:39
 * this code is from lighttpd
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "buffer.h"
#include "log.h"

buffer_t *buffer_new(size_t len)
{
    buffer_t *b;

    b = malloc(sizeof(*b));

    assert(b);

    b->ptr = NULL;
    b->size = 0;
    b->used = 0;

    if (len) {
        buffer_prepare_copy(b, len);
    }

    return b;
}

void buffer_free(buffer_t * b)
{
    if (!b)
        return;
    if (b->ptr) {
        free(b->ptr);
        b->ptr = NULL;
    }
    free(b);
}

#define BUFFER_MAX_REUSE_SIZE  (32 * 1024)

void buffer_reset(buffer_t * b)
{
    if (!b)
        return;

    /* limit don't reuse buffer_t larger than ... bytes */
    if (b->size > BUFFER_MAX_REUSE_SIZE) {
        free(b->ptr);
        b->ptr = NULL;
        b->size = 0;
    }

    b->used = 0;
}

#define BUFFER_PIECE_SIZE 64

int buffer_prepare_copy(buffer_t * b, size_t size)
{
    if (!b)
        return -1;

    if ((0 == b->size) || (size > b->size)) {
        if (b->size && b->ptr) {
            free(b->ptr);
            b->ptr = NULL;
        }

        b->size = size;

        /* always allocate a multiple of BUFFER_PIECE_SIZE */
        b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

        b->ptr = malloc(b->size);

        assert(b->ptr);
    }
    b->used = 0;
    return 0;
}

/**
 *
 * increase the internal buffer_t (if necessary) to append another 'size' byte
 * ->used isn't changed
 *
 */

int buffer_prepare_append(buffer_t * b, size_t size)
{
    if (!b)
        return -1;

    if (0 == b->size) {
        b->size = size;

        /* always allocate a multiple of BUFFER_PIECE_SIZE */
        b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

        b->ptr = malloc(b->size);
        b->used = 0;
        assert(b->ptr);
    } else if (b->used + size > b->size) {
        b->size += size;

        /* always allocate a multiple of BUFFER_PIECE_SIZE */
        b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

        b->ptr = realloc(b->ptr, b->size);
        assert(b->ptr);
    }
    return 0;
}

int buffer_append_memory(buffer_t * b, const void *s, size_t s_len)
{
    if (!s || !b)
        return -1;
    if (s_len == 0)
        return 0;

    buffer_prepare_append(b, s_len);
    memcpy(b->ptr + b->used, s, s_len);
    b->used += s_len;

    return 0;
}

int buffer_append_raw_int32(buffer_t * b, int32_t arg)
{
    return buffer_append_memory(b, &arg, 4);
}

int buffer_append_raw_int64(buffer_t * b, int64_t arg)
{
    return buffer_append_memory(b, &arg, 8);
}

int buffer_append_int32(buffer_t * b, int32_t arg)
{
    return buffer_append_printf(b, "%d", arg);
}

/*
will NOT add trailing '\0'
*/
int buffer_append_printf(buffer_t * b, char *fmt, ...)
{
    va_list ap;
    int len;

    if (!b)
        return -1;

    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    buffer_prepare_append(b, len);
    vsprintf(b->ptr + b->used, fmt, ap);
    va_end(ap);
    b->used += len;
    return 0;
}

int buffer_append_trailing_null(buffer_t * b)
{
    return buffer_append_memory(b, "\0", 1);
}

int buffer_copy_memory(buffer_t * b, const void *s, size_t s_len)
{
    if (!s || !b)
        return -1;

    b->used = 0;

    return buffer_append_memory(b, s, s_len);
}

int buffer_is_empty(buffer_t * b)
{
    return (b->used == 0);
}

/**
 * check if two buffers contain the same data
 *
 * HISTORY: this function was pretty much optimized, but didn't handled
 * alignment properly.
 */

int buffer_is_equal(buffer_t * a, buffer_t * b)
{
    if (a->used != b->used)
        return 0;
    if (a->used == 0)
        return 1;

    return (0 == strncmp(a->ptr, b->ptr, a->used - 1));
}
