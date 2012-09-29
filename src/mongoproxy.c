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

#define MONGO_HEAD_LEN (sizeof(int))

typedef enum event_handler_ret_s {
    EVENT_HANDLER_FINISHED = 0,
    EVENT_HANDLER_ERROR,
    EVENT_HANDLER_WAIT_FOR_EVENT,
} event_handler_ret_t;

event_handler_ret_t on_write(int fd, mongoproxy_session_t * sess);
void on_event(int fd, short what, void *arg);

static int _mongoproxy_read_done(mongoproxy_session_t * sess)
{
    if (sess->buf->used < MONGO_HEAD_LEN)
        return 0;
    int body_len = *(int *)sess->buf->ptr;

    DEBUG("[body_len:%d] [sess->buf->used:%d]", body_len, sess->buf->used);
    return sess->buf->used >= body_len;
}

static char mongoproxy_session_state_names[][50] = {
    "SESSION_STATE_UNSET",
    "SESSION_STATE_READ_CLIENT_REQUEST",
    "SESSION_STATE_PROCESSING",
    "SESSION_STATE_SEND_BACK_TO_CLIENT",
    "SESSION_STATE_FINISH"
};

char *mongoproxy_session_state_name(mongoproxy_session_state_t state)
{
    return mongoproxy_session_state_names[(int)state];
}

static void _mongoproxy_set_state(mongoproxy_session_t * sess, mongoproxy_session_state_t state)
{
    DEBUG("set mongo_proxy_state %s => %s",
          mongoproxy_session_state_name(sess->proxy_state), mongoproxy_session_state_name(state));
    sess->proxy_state = state;
}

/*
 * the sub state_machine
 * */
int mongo_conn_state_machine(mongoproxy_session_t * sess)
{
    mongo_conn_t *conn = sess->backend_conn;

    DEBUG("in state machine 2 [conn->conn_state:%s]", mongo_conn_state_name(conn->conn_state));

    if (conn->conn_state == MONGO_CONN_STATE_RECV_RESPONSE) {
        if (_mongoproxy_read_done(sess)) {
            _mongoproxy_set_state(sess, SESSION_STATE_SEND_BACK_TO_CLIENT);
        }
        mongo_conn_set_state(conn, MONGO_CONN_STATE_CONNECTED);
    }
    return 0;
}

int mongoproxy_state_machine(mongoproxy_session_t * sess)
{
    DEBUG("in state machine [sess->proxy_state:%s]", mongoproxy_session_state_name(sess->proxy_state));

    mongo_replset_t *replset;

    replset = &(g_server.replset);

    if (sess->proxy_state == SESSION_STATE_READ_CLIENT_REQUEST) {
        if (_mongoproxy_read_done(sess)) {
            _mongoproxy_set_state(sess, SESSION_STATE_PROCESSING);
            mongoproxy_session_select_backend(sess, 0);
            /*return 0; */
        }
    }
    if (sess->proxy_state == SESSION_STATE_PROCESSING) {    //run sub state machine
        mongo_conn_state_machine(sess);
    }

    if (sess->proxy_state == SESSION_STATE_SEND_BACK_TO_CLIENT) {
        DEBUG("[fd:%d] enable write event ", sess->fd);
        event_set(&(sess->ev), sess->fd, EV_WRITE, on_event, sess);
        event_add(&(sess->ev), NULL);
    } else if (sess->proxy_state == SESSION_STATE_READ_CLIENT_REQUEST) {
        DEBUG("[fd:%d] enable read event ", sess->fd);
        event_set(&(sess->ev), sess->fd, EV_READ, on_event, sess);
        event_add(&(sess->ev), NULL);
    }

    return 0;
}

event_handler_ret_t on_write(int fd, mongoproxy_session_t * sess)
{
    int len;
    len = network_write(fd, sess->buf->ptr, sess->buf->used);
    if (len < 0) {
        ERROR("[fd:%d]error on write [errno:%d(%s)]", fd, errno, strerror(errno));
        return EVENT_HANDLER_ERROR;
    }
    if (len == sess->buf->used) {   //all sent
        return EVENT_HANDLER_WAIT_FOR_EVENT;
    } else {
        ERROR("TODO ...");
        return EVENT_HANDLER_ERROR;
    }
}

event_handler_ret_t on_read(int fd, mongoproxy_session_t * sess)
{
    int len;
    DEBUG("[fd:%d] on read", fd);

    len = network_read(fd, sess->buf->ptr, sess->buf->size);
    if (len < 0) {
        ERROR("error on read [errno:%d(%s)]", errno, strerror(errno));
        return EVENT_HANDLER_ERROR;
    }
    if (len == 0) {
        ERROR("connection peer closed [errno:%d(%s)]", errno, strerror(errno));
        return EVENT_HANDLER_FINISHED;
    }
    sess->buf->used += len;

    return EVENT_HANDLER_WAIT_FOR_EVENT;
}

void on_event(int fd, short what, void *arg)
{
    mongoproxy_session_t *sess = (mongoproxy_session_t *) arg;

    DEBUG("[fd:%d] [what:0x%x]", fd, what);
    if (what & EV_READ) {
        int ret = on_read(fd, sess);

        DEBUG("[fd:%d] on_read return : %d", fd, ret);
        if (ret == EVENT_HANDLER_WAIT_FOR_EVENT) {
            mongoproxy_state_machine(sess);
            /*event_add(&(sess->ev), NULL); */
            /*sess->buf->used=0; */
        } else if (ret == EVENT_HANDLER_ERROR) {

        } else {
            DEBUG("[fd:%d] we will close ", fd);
            close(fd);
            mongoproxy_session_close(sess);
            mongoproxy_session_free(sess);
        }
    }

    if (what & EV_WRITE) {
        int ret = on_write(fd, sess);
        if (ret == EVENT_HANDLER_WAIT_FOR_EVENT) {
            _mongoproxy_set_state(sess, SESSION_STATE_READ_CLIENT_REQUEST);
            sess->buf->used = 0;
            mongoproxy_state_machine(sess);
            /*event_add(&(sess->ev), NULL); */
        } else if (ret == EVENT_HANDLER_ERROR) {

        } else {

        }
    }
}

void on_accept(int fd, short what, void *arg)
{
    int client_fd;
    DEBUG("[fd:%d] on accept", fd);
    mongoproxy_session_t *sess;
    client_fd = network_accept(fd, NULL, 0, NULL);
    if (client_fd == -1) {
        WARNING("accept failed");
        return;
    }

    DEBUG("accept new fd [fd:%d]", client_fd);
    sess = mongoproxy_session_new();
    sess->fd = client_fd;
    _mongoproxy_set_state(sess, SESSION_STATE_READ_CLIENT_REQUEST);

    event_set(&(sess->ev), client_fd, EV_READ, on_event, sess);
    event_add(&(sess->ev), NULL);
}

int mongoproxy_init()
{
    mongo_replset_t *replset = &(g_server.replset);
    mongoproxy_cfg_t *cfg = &(g_server.cfg);
    cfg->backend = strdup(cfg_getstr("MONGOPROXY_BACKEND", ""));
    cfg->listen_host = strdup(cfg_getstr("MONGOPROXY_BIND", "0.0.0.0"));
    cfg->listen_port = cfg_getint32("MONGOPROXY_PORT", 7111);
    cfg->use_replset = cfg_getint32("MONGOPROXY_USE_REPLSET", 0);

    if (strlen(cfg->backend) == 0) {
        ERROR("no backend");
        return -1;
    }
    return mongo_replset_init(replset, cfg);
}

int mongoproxy_mainloop()
{
    struct event ev_accept;
    mongoproxy_cfg_t *cfg = &(g_server.cfg);
    int listen_fd;

    listen_fd = network_server_socket(cfg->listen_host, cfg->listen_port);

    event_init();               //init libevent
    event_set(&ev_accept, listen_fd, EV_READ | EV_PERSIST, on_accept, NULL);
    event_add(&ev_accept, NULL);

    /* Start the libevent event loop. */
    event_dispatch();
    return 0;
}
