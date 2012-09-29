/*
 * file   : mongo_msg.c
 * author : ning
 * date   : 2012-09-29 14:23:39
 */

#include <stdio.h>

#include "mongo_msg.h"

int mongomsg_encode_int_command(buffer_t* buf, const char *db,
        const char *cmdstr, int arg)
{
    return 0;
}

int mongomsg_encode_ismaster(buffer_t *buf)
{
    return mongomsg_encode_int_command(buf, "admin", "ismaster", 1);
}

int mongomsg_encode_ping(buffer_t *buf)
{
    return mongomsg_encode_int_command(buf, "admin", "ping", 1);
}

int mongomsg_decode_ismaster(buffer_t *buf, int * ismaster, buffer_t * hosts)
{
    bson_iterator it;
    bson_iterator it_sub;

    const char *data;
    const char *host_string;


    bson response ;
    bson_init_data(&response, buf->ptr + sizeof(mongomsg_header_t));

    buffer_prepare_copy(hosts, sizeof("255.255.255.255:65536") * 15);

    if( bson_find(&it, &response, "hosts" ) ) {
        data = bson_iterator_value( &it );
        bson_iterator_from_buffer( &it_sub, data );

        while( bson_iterator_next( &it_sub ) ) {
            host_string = bson_iterator_string( &it_sub );
            buffer_append_memory(hosts, host_string, strlen(host_string));
            buffer_append_memory(hosts, ",", 1);
        }
        buffer_append_memory(hosts, "\0", 1);
    }
}

int mongomsg_decode_ping(buffer_t *buf, int * ok)
{
    bson response;
    bson_iterator it;

    bson_init_data(&response, buf->ptr + sizeof(mongomsg_header_t));

    if(BSON_EOO == bson_find(&it, &response, "ok"))
    {
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return 0;
}

