/*
 * file   : mongo_msg.h
 * author : ning
 * date   : 2012-09-29 14:06:09
 */

#ifndef _MONGO_MSG_H_
#define _MONGO_MSG_H_

#include "bson.h"
#include "buffer.h"

typedef struct mongomsg_header_s{
    int32_t   message_length; // total message size, including this
    int32_t   request_id;     // identifier for this message
    int32_t   response_to;    // requestID from the original request
                              //   (used in reponses from db)
    int32_t   op_code;        // request type - see table below
}mongomsg_header_t;

int mongomsg_encode_int_command(buffer_t * buf, const char *db,
        const char *cmdstr, int arg);


int mongomsg_encode_ismaster(buffer_t *buf);
int mongomsg_encode_ping(buffer_t *buf);


int mongomsg_decode_ismaster(buffer_t *buf, int * ismaster, buffer_t * hosts);

int mongomsg_decode_ping(buffer_t *buf, int * ok);

#endif

