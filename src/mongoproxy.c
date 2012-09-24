/*
 * file   : mongoproxy.c
 * author : ning
 * date   : 2012-09-24 16:22:51
 */

#include <stdio.h>
#include <errno.h>

#include <event.h>          //libevent

#include "log.h"

#include "mongoproxy.h"
#include "mongoproxy_session.h"
#include "mongo_server.h"

//globals
//
static mongo_replica_set_t *g_replica_set;
static mongoproxy_cfg_t *g_mongoproxy_cfg;

static int _mongoproxy_load_config(){

}

static int _mongoproxy_read_client_request_done(mongoproxy_session_t * sess){

}

static int _mongoproxy_state_machine(mongoproxy_session_t * sess){
    if(sess->proxy_state == SESSION_STATE_READ_CLIENT_REQUEST){
        if (_mongoproxy_read_client_request_done()){
            sess->proxy_state = SESSION_STATE_SEND_TO_BACKEND;
            return ;
        }
    }
}

void on_read(int fd, short ev, void *arg)
{
    int client_fd;
    int len;

    mongoproxy_session_t * sess;
    sess = (mongoproxy_session_t *) arg;

    len = network_read(fd, sess->buf, sess->buf_len);
    if (len < 0 ){
        ERROR("error on read [errno:%d]", errno);
        return;
    }
    if (len == 0) {
        printf("lost connection [errno:%d]", errno);
        close(fd);
        return;
    }

    event_set(sess->ev, client_fd, EV_READ, on_read, sess);
    event_add(sess->ev, NULL);
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
    event_set(sess->ev, client_fd, EV_READ, on_read, sess);
    event_add(sess->ev, NULL);

}

static int _mongoproxy_init(){
    struct event ev_accept;

    int listen_fd ; 
    
    listen_fd = network_server_socket(g_mongoproxy_cfg->listen_host, g_mongoproxy_cfg->listen_port);

    event_init(); //init libevent
    event_set(&ev_accept, listen_fd, EV_READ | EV_PERSIST, on_accept, NULL);
    event_add(&ev_accept, NULL);

    /* Start the libevent event loop. */
    event_dispatch();

}



int mongoproxy_mainloop(){

}
