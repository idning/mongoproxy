/*
 * file   : mongo_msg.h
 * author : ning
 * date   : 2012-09-29 14:06:09
 */

#ifndef _MONGO_MSG_H_
#define _MONGO_MSG_H_

#include "bson.h"
#include "buffer.h"
#include "log.h"

typedef struct mongomsg_header_s {
    int32_t message_length;     // total message size, including this
    int32_t request_id;         // identifier for this message
    int32_t response_to;        // requestID from the original request
    //   (used in reponses from db)
    int32_t op_code;            // request type - see table below
} mongomsg_header_t;


#define MONGOMSG_QUERY_HEADER_SIZE (9*sizeof(int))

// db ops
#define OP_REPLY 1
#define OP_MSG 1000
#define OP_UPDATE 2001
#define OP_INSERT 2002
#define OP_GET_BY_OID 2003
#define OP_QUERY 2004
#define OP_GET_MORE 2005
#define OP_DELETE 2006
#define OP_KILL_CURSORS 2007


int mongomsg_encode_int_command(buffer_t * buf, const char *db, const char *cmdstr, int arg);

int mongomsg_encode_ismaster(buffer_t * buf);
int mongomsg_encode_ping(buffer_t * buf);

int mongomsg_decode_ismaster(buffer_t * buf, int *ismaster, buffer_t * hosts, buffer_t * primary);
int mongomsg_decode_ping(buffer_t * buf, int *ok);

#endif
