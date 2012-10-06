/*
 * file   : mongo_msg.c
 * author : ning
 * date   : 2012-09-29 14:23:39
 */

#include <stdio.h>

#include "mongo_msg.h"

static int _req_id = 100000000;

static int mongomsg_encode_int_command(buffer_t * buf, const char *db, const char *cmdstr, int arg)
{
    mongomsg_header_t header;
    bson cmd;
    int option = 0;
    int skip = 0;
    int limit = 1;

    bson_init(&cmd);
    bson_append_int(&cmd, cmdstr, arg);
    bson_finish(&cmd);

    header.message_length = 0;
    header.request_id = _req_id++;
    header.response_to = 0;
    header.op_code = OP_QUERY;

    buffer_append_memory(buf, &header, sizeof(header));
    buffer_append_raw_int32(buf, option);
    //ns
    buffer_append_memory(buf, db, strlen(db));
    buffer_append_memory(buf, ".$cmd", strlen(".$cmd"));
    buffer_append_memory(buf, "\0", 1);

    buffer_append_raw_int32(buf, skip);
    buffer_append_raw_int32(buf, limit);

    buffer_append_memory(buf, bson_data(&cmd), bson_size(&cmd));

    //update header message_length
    mongomsg_header_t *h = (mongomsg_header_t *) buf->ptr;
    h->message_length = buf->used;

    bson_destroy(&cmd);

    return 0;
}

int mongomsg_encode_ismaster(buffer_t * buf)
{
    return mongomsg_encode_int_command(buf, "admin", "ismaster", 1);
}

int mongomsg_encode_ping(buffer_t * buf)
{
    return mongomsg_encode_int_command(buf, "admin", "ping", 1);
}

/*

struct {
    MsgHeader header;         // standard message header
    int32     responseFlags;  // bit vector - see details below
    int64     cursorID;       // cursor id if client needs to do get more's
    int32     startingFrom;   // where in the cursor this reply is starting
    int32     numberReturned; // number of documents in the reply
    document* documents;      // documents
}

 * */
int mongomsg_decode_ismaster(buffer_t * buf, int *ismaster, buffer_t * hosts, buffer_t * primary)
{
    bson_iterator it;
    bson_iterator it_sub;

    const char *data;
    const char *host_string;

    bson response;
    bson_init_data(&response, buf->ptr + MONGOMSG_QUERY_HEADER_SIZE);

    buffer_prepare_copy(hosts, sizeof("255.255.255.255:65536") * 15);

    bson_print(&response);      //TODO , print to log

    if (bson_find(&it, &response, "hosts")) {
        DEBUG("has hosts");
        data = bson_iterator_value(&it);
        bson_iterator_from_buffer(&it_sub, data);

        while (bson_iterator_next(&it_sub)) {
            host_string = bson_iterator_string(&it_sub);
            buffer_append_memory(hosts, host_string, strlen(host_string));
            buffer_append_memory(hosts, ",", 1);
        }
        buffer_append_memory(hosts, "\0", 1);
    }

    if (bson_find(&it, &response, "primary")) {
        DEBUG("has primary");
        host_string = bson_iterator_string(&it);
        buffer_append_memory(primary, host_string, strlen(host_string));
        buffer_append_memory(primary, "\0", 1);
    }

    return 0;
}

int mongomsg_decode_ping(buffer_t * buf, int *ok)
{
    bson response;
    bson_iterator it;

    bson_init_data(&response, buf->ptr + sizeof(mongomsg_header_t));

    if (BSON_EOO == bson_find(&it, &response, "ok")) {
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return 0;
}


int mongomsg_read_done(buffer_t * buf)
{
    mongomsg_header_t *header;

    if (buf->used < sizeof(mongomsg_header_t))
        return 0;

    header = (mongomsg_header_t *) buf->ptr;
    int body_len = header->message_length;

    DEBUG("[body_len:%d] [buf->used:%d]", body_len, buf->used);
    return buf->used >= body_len;
}


const char *mongo_proxy_op_code2str(int op)
{
    switch (op) {
    case OP_REPLY:
        return "OP_REPLY";
    case OP_MSG:
        return "OP_MSG";
    case OP_GET_BY_OID:
        return "OP_GET_BY_OID";
    case OP_QUERY:
        return "OP_QUERY";
    case OP_GET_MORE:
        return "OP_GET_MORE";
    case OP_KILL_CURSORS:
        return "OP_KILL_CURSORS";
    case OP_UPDATE:
        return "OP_UPDATE";
    case OP_INSERT:
        return "OP_INSERT";
    case OP_DELETE:
        return "OP_DELETE";
    default:
        return "none";
    }
}

