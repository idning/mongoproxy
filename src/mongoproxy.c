/*
 * file   : mongoproxy.c
 * author : ning
 * date   : 2012-09-24 16:22:51
 */

#include "mongoproxy.h"
#include "mongoproxy_session.h"
#include "mongo_backend.h"

//globals
//
mongoproxy_server_t g_server;

static int _mongoproxy_load_config(){
    return 0;
}

#define MONGO_HEAD_LEN (sizeof(int))


static int _mongoproxy_read_client_request_done(mongoproxy_session_t * sess){
    if ( sess->buf->used < MONGO_HEAD_LEN )
        return 0;

    int body_len = *(int*) sess->buf->ptr;
    return sess->buf->used >= body_len;
}

static void _mongoproxy_set_state(mongoproxy_session_t * sess, mongoproxy_session_state_t state){
    DEBUG("MONGO_PROXY_STATE CHANGED %d => %d", sess->proxy_state, state);
    sess->proxy_state = state;
}

static int _mongoproxy_state_machine(mongoproxy_session_t * sess){
    mongo_replset_t * replset;

    replset = &(g_server.replset);

    if(sess->proxy_state == SESSION_STATE_READ_CLIENT_REQUEST){
        if (_mongoproxy_read_client_request_done(sess)){
            _mongoproxy_set_state(sess, SESSION_STATE_PROCESSING);
            sess->backend_conn = mongo_replset_get_conn(replset, 0);

            return 0;
        }
    }
    return 0;
}

void on_read(int fd, short ev, void *arg)
{
    int len;

    mongoproxy_session_t * sess;
    sess = (mongoproxy_session_t *) arg;

    len = network_read(fd, sess->buf->ptr, sess->buf->size);
    if (len < 0 ){
        ERROR("error on read [errno:%d]", errno);
        return;
    }
    if (len == 0) {
        printf("lost connection [errno:%d]", errno);
        close(fd);
        return;
    }

    event_set(&(sess->ev), fd, EV_READ, on_read, sess);
    event_add(&(sess->ev), NULL);
    _mongoproxy_state_machine(sess);
}

void on_accept(int fd, short ev, void *arg)
{
    int client_fd;
    mongoproxy_session_t * sess;
    client_fd = network_accept(fd, NULL, 0, NULL);
    if (client_fd == -1) {
        WARNING("accept failed");
        return;
    }

    sess = mongoproxy_session_new();
    sess->fd = client_fd;

    /*event_set(sess->ev, client_fd, EV_READ | EV_PERSIST, on_read, sess);*/
    event_set(&(sess->ev), client_fd, EV_READ, on_read, sess);
    event_add(&(sess->ev), NULL);
}

int mongoproxy_init(){
    mongoproxy_cfg_t * cfg = &(g_server.cfg);
    cfg->backend = strdup(cfg_getstr("MONGOPROXY_BACKEND", ""));
    cfg->listen_host = strdup(cfg_getstr("MONGOPROXY_BIND", "0.0.0.0"));
    cfg->listen_port = cfg_getint32("MONGOPROXY_PORT", 7111);
    cfg->use_replset = cfg_getint32("MONGOPROXY_USE_REPLSET", 0);

    if(strlen(cfg->backend) == 0){
        ERROR("no backend");
        return -1;
    }
    return 0;
}



int mongoproxy_mainloop(){
    struct event ev_accept;
    mongoproxy_cfg_t * cfg = &(g_server.cfg);
    int listen_fd ; 

    listen_fd = network_server_socket(cfg->listen_host, cfg->listen_port);

    event_init(); //init libevent
    event_set(&ev_accept, listen_fd, EV_READ | EV_PERSIST, on_accept, NULL);
    event_add(&ev_accept, NULL);

    /* Start the libevent event loop. */
    event_dispatch();

    return 0;
}
