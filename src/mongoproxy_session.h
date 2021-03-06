/*
 * file   : mongoproxy_session.h
 * author : ning
 * date   : 2012-09-24 00:44:56
 */

#ifndef _MONGOPROXY_SESSION_H_
#define _MONGOPROXY_SESSION_H_

#include "mongoproxy.h"

typedef enum mongoproxy_session_state_s {
    SESSION_STATE_UNSET = 0,
    SESSION_STATE_READ_CLIENT_REQUEST,
    SESSION_STATE_PROCESSING,
    SESSION_STATE_SEND_BACK_TO_CLIENT,
    SESSION_STATE_FINISH
} mongoproxy_session_state_t;

typedef struct mongoproxy_session_s {
    mongo_conn_t *backend_conn;
    mongoproxy_session_state_t proxy_state;

    int fd;
    struct event *ev;
    char *client_ip;
    int client_port;
    uint64_t sessionid;

    int bytes_sent;

    buffer_t *buf;
} mongoproxy_session_t;

mongoproxy_session_t *mongoproxy_session_new();
void mongoproxy_session_free(mongoproxy_session_t *);
int mongoproxy_session_close(mongoproxy_session_t * sess);

int mongoproxy_session_select_backend(mongoproxy_session_t * sess, int primary);

#endif
