/*
 * file   : mongo_msg.c
 * author : ning
 * date   : 2012-09-29 14:23:39
 */

#include <stdio.h>

#include "mongo_msg.h"

static void _bson_dump(const char *data , int depth, buffer_t * out ) ;

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


/*
 * what a protocol...
 * we need strlen(ns)..
 * copy this from chensi
*/
int mongomsg_dump(buffer_t * buf, buffer_t * out)
{
    mongomsg_header_t *header= (mongomsg_header_t *) buf->ptr;

    int i;
    char* ns ;
    int32_t *flags = NULL;
    int32_t *skip = NULL;
    int32_t *limit = NULL;
    void * doc1 = NULL;
    void * doc2 = NULL;

    int64_t * cursor = NULL;

    if (!buf || !out){
        return -1; 
    }

    buffer_append_printf(out, "{message_length:%d, request_id:%u, response_to:%d, op:%s} ", 
            header->message_length, header->request_id, header->response_to, mongo_proxy_op_code2str(header->op_code));

    if(OP_KILL_CURSORS == header->op_code || OP_MSG == header->op_code) {
        ns = "none";
    }else {
        ns = buf->ptr+ sizeof(mongomsg_header_t) + 4; 
    }
    buffer_append_printf(out, "{ns:%s} ",ns); 

    switch(header->op_code)
    {
    case OP_MSG:
        buffer_append_printf(out, "{OP_MSG} "); 
    case OP_GET_BY_OID:
        buffer_append_printf(out, "{OP_GET_BY_OID} "); 
    case OP_REPLY:
        buffer_append_printf(out, "{OP_REPLY} "); 
    case OP_QUERY:
        flags = (int32_t *) (buf->ptr+sizeof(mongomsg_header_t));
        skip = (int32_t *) ((void*)flags + sizeof(int32_t)+ strlen(ns) + 1); 
        limit = (int32_t *) skip + 1;
        doc1 = (void*) (limit + 1);
        buffer_append_printf(out, "{flags:0x%x, skip:%d, limit:%d} ", *flags, *skip, *limit); 
        _bson_dump(doc1, 0, out);
        break;

    case OP_GET_MORE:
        limit = (int32_t*) (buf->ptr + sizeof(mongomsg_header_t)
                + sizeof(int32_t)  + strlen(ns) + 1);
        cursor = (int64_t*)(buf->ptr + sizeof(mongomsg_header_t)
                + sizeof(int32_t) * 2 + strlen(ns) + 1);
        buffer_append_printf(out, "[cursor:%ld] [limit:%d]", *cursor, *limit);
        break;
    case OP_KILL_CURSORS:
        limit = (int32_t*)(buf->ptr + sizeof(mongomsg_header_t) +
                sizeof(int32_t));
    
        buffer_append_printf(out, "cursors:[");
        for(i = 0; i < (*limit ); ++i)
        {
            cursor = (int64_t*) (buf->ptr + sizeof(mongomsg_header_t) + sizeof(int32_t) * 2 + sizeof(int64_t) * i);
            buffer_append_printf(out, "%ld,", *cursor);
        }
        buffer_append_printf(out, "]");
        break;
    case OP_INSERT:
        flags = (int32_t*)(buf->ptr + sizeof(mongomsg_header_t));
        doc1 = buf->ptr + sizeof(mongomsg_header_t) + sizeof(int32_t) + strlen(ns) + 1;

        buffer_append_printf(out, "[flags:0x%x]", *flags);
        _bson_dump(doc1, 0, out);
        break;
    case OP_DELETE:
        flags = (int32_t*)(buf->ptr + sizeof(mongomsg_header_t) + strlen(ns) + 1 + sizeof(int32_t));
        doc1 = buf->ptr + sizeof(mongomsg_header_t) + sizeof(int32_t) * 2 + strlen(ns) + 1;

        buffer_append_printf(out, "[flags:0x%x]", *flags);
        _bson_dump(doc1, 0, out);
        break;
    case OP_UPDATE:
        flags = (int32_t*)(buf->ptr + sizeof(mongomsg_header_t) + strlen(ns) + 1 + sizeof(int32_t));
        doc1 = buf->ptr + sizeof(mongomsg_header_t) + sizeof(int32_t) * 2 + strlen(ns) + 1;
        buffer_append_printf(out, "[flags:0x%x]", *flags);
        _bson_dump(doc1, 0, out);

        //TODO
        break;
    default:
        WARNING("bad mongo op [op:%d]", header->op_code);
    }

    return 0;
}


void _bson_dump(const char *data , int depth, buffer_t * out ) {
    bson_iterator i;
    const char *key;
    int temp;
    bson_timestamp_t ts;
    char oidhex[25];
    bson scope;

    bson_iterator_from_buffer( &i, data );

    buffer_append_printf(out, "{");

    while ( bson_iterator_next( &i ) ) {
        bson_type t = bson_iterator_type( &i );
        if ( t == 0 )
            break;
        key = bson_iterator_key( &i );

        buffer_append_printf(out, "%s:" , key);
        switch ( t ) {
            case BSON_DOUBLE:
                buffer_append_printf(out, "%f" , bson_iterator_double( &i ) );
                break;
            case BSON_STRING:
                buffer_append_printf(out, "%s" , bson_iterator_string( &i ) );
                break;
            case BSON_SYMBOL:
                buffer_append_printf(out, "SYMBOL: %s" , bson_iterator_string( &i ) );
                break;
            case BSON_OID:
                bson_oid_to_string( bson_iterator_oid( &i ), oidhex );
                buffer_append_printf(out, "%s" , oidhex );
                break;
            case BSON_BOOL:
                buffer_append_printf(out, "%s" , bson_iterator_bool( &i ) ? "true" : "false" );
                break;
            case BSON_DATE:
                buffer_append_printf(out, "%ld" , ( long int )bson_iterator_date( &i ) );
                break;
            case BSON_BINDATA:
                buffer_append_printf(out, "BSON_BINDATA" );
                break;
            case BSON_UNDEFINED:
                buffer_append_printf(out, "BSON_UNDEFINED" );
                break;
            case BSON_NULL:
                buffer_append_printf(out, "BSON_NULL" );
                break;
            case BSON_REGEX:
                buffer_append_printf(out, "BSON_REGEX: %s", bson_iterator_regex( &i ) );
                break;
            case BSON_CODE:
                buffer_append_printf(out, "BSON_CODE: %s", bson_iterator_code( &i ) );
                break;
            case BSON_CODEWSCOPE:
                buffer_append_printf(out, "BSON_CODE_W_SCOPE: %s", bson_iterator_code( &i ) );
                /* bson_init( &scope ); */ /* review - stepped on by bson_iterator_code_scope? */
                bson_iterator_code_scope( &i, &scope );
                buffer_append_printf(out, "\n\t SCOPE: " );
                bson_print( &scope );
                /* bson_destroy( &scope ); */ /* review - causes free error */
                break;
            case BSON_INT:
                buffer_append_printf(out, "%d" , bson_iterator_int( &i ) );
                break;
            case BSON_LONG:
                buffer_append_printf(out, "%lld" , ( uint64_t )bson_iterator_long( &i ) );
                break;
            case BSON_TIMESTAMP:
                ts = bson_iterator_timestamp( &i );
                buffer_append_printf(out, "i: %d, t: %d", ts.i, ts.t );
                break;
            case BSON_OBJECT:
            case BSON_ARRAY:
                buffer_append_printf(out, "\n" );
                _bson_dump( bson_iterator_value( &i ) , depth + 1, out);
                break;

            default:
                ERROR( "can't dump type : %d\n" , t );
        }

        buffer_append_printf(out, "," );
    }

    buffer_append_printf(out, "}");
}

