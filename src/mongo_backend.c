/*
 * file   : test_ptrace.c
 * author : ning
 * date   : 2012-01-13 16:10:27
 */

#include "mongoproxy.h"
#include "mongo_backend.h"

/*mongo_replset_t * replset;*/

static char mongo_conn_state_names[][50] = {
    "mongo_conn_state_UNSET",
    "mongo_conn_state_CONNECTING",
    "mongo_conn_state_CONNECTED",
    "mongo_conn_state_SEND_REQUEST",
    "mongo_conn_state_RECV_RESPONSE",
    "mongo_conn_state_CLOSED"
};

char *mongo_conn_state_name(mongo_conn_state_t state)
{
    return mongo_conn_state_names[(int)state];
}


int mongo_conn_set_state(mongo_conn_t * conn, mongo_conn_state_t state)
{
    DEBUG("set mongo_conn state %s => %s", mongo_conn_state_name(conn->conn_state), mongo_conn_state_name(state));
    conn->conn_state = state;
    return 0;
}

/**
 * we use the same function on reading data from client and mongobackend
 * it will be used in mongo_backend_on_event and mongo_session_on_event
 */
event_handler_ret_t mongo_backend_on_read(int fd, mongoproxy_session_t * sess)
{
    int len;

    DEBUG("[fd:%d] on read", fd);

    if (sess->buf->used > sizeof(mongomsg_header_t)){
        mongomsg_header_t *header = (mongomsg_header_t *) sess->buf->ptr;
        buffer_prepare_append(sess->buf, header->message_length - sess->buf->used);
    }

    len = network_read(fd, sess->buf->ptr+sess->buf->used, sess->buf->size - sess->buf->used);
    if (len < 0) {
        ERROR("error on read [errno:%d(%s)]", errno, strerror(errno));
        return EVENT_HANDLER_ERROR;
    }
    if (len == 0) {
        ERROR("connection closed by peer [errno:%d(%s)]", errno, strerror(errno));
        return EVENT_HANDLER_FINISHED;
    }
    sess->buf->used += len;

    if (mongomsg_read_done(sess->buf)){
        return EVENT_HANDLER_FINISHED;
    }else {
        return EVENT_HANDLER_WAIT_FOR_EVENT;
    }
}

/**
 * we use the same function on writing data to client and mongobackend
 * it will be used in mongo_backend_on_event and mongo_session_on_event
 */
event_handler_ret_t mongo_backend_on_write(int fd, mongoproxy_session_t * sess)
{
    int len;

    DEBUG("[fd:%d] on write", fd);

    util_print_buffer("sess->buf", sess->buf);
    len = network_write(fd, sess->buf->ptr, sess->buf->used);
    if (len < 0) {
        ERROR("[fd:%d]error on write [errno:%d(%s)]", fd, errno, strerror(errno));
        return EVENT_HANDLER_ERROR;
    }
    if (len == sess->buf->used) {   //all sent
        DEBUG("[fd:%d]write done ", fd);
        return EVENT_HANDLER_FINISHED;
    }
    return EVENT_HANDLER_WAIT_FOR_EVENT;
}

void mongo_backend_on_event(int fd, short what, void *arg)
{
    mongoproxy_session_t *sess = (mongoproxy_session_t *) arg;
    DEBUG("[fd:%d] [what:0x%x]", fd, what);

    if (what & EV_READ) {
        int ret = mongo_backend_on_read(fd, sess);
        DEBUG("[fd:%d] on_read return : %d", fd, ret);

        if (ret == EVENT_HANDLER_WAIT_FOR_EVENT) {
            //TODO: we need reenable event , or we will fall on large object, let's test it first
        } else if (ret == EVENT_HANDLER_FINISHED) {
            mongoproxy_state_machine(sess);
        } else if (ret == EVENT_HANDLER_ERROR) {                //error
            DEBUG("[fd:%d] got EVENT_HANDLER_ERROR", fd);
            goto err;
        }
    }

    if (what & EV_WRITE) {
        int ret = mongo_backend_on_write(fd, sess);
        DEBUG("[fd:%d] on_write return : %d", fd, ret);

        if (ret == EVENT_HANDLER_WAIT_FOR_EVENT) {
            //TODO: we need reenable event , or we will fall on large object, let's test it first
        } else if (ret == EVENT_HANDLER_FINISHED) {
            mongoproxy_state_machine(sess);

        } else if (ret == EVENT_HANDLER_ERROR) {                //error
            DEBUG("[fd:%d] got EVENT_HANDLER_ERROR", fd);
            goto err;
        }
    }
    return;

err:
    /*mongo_backend_close_conn(sess->backend_conn);*/
    sess->backend_conn = NULL;
    mongoproxy_session_close(sess);
    mongoproxy_session_free(sess);
}

mongo_backend_t *mongo_backend_new(char *host, int port)
{
    mongo_backend_t *backend = malloc(sizeof(mongo_backend_t));
    memset(backend, 0, sizeof(mongo_backend_t));

    backend->host = strdup(host);
    backend->port = port;
    backend->is_primary = 0;

    return backend;
}

mongo_conn_t *mongo_backend_new_conn(mongo_backend_t * backend)
{
    mongo_conn_t *conn;
    int fd;

    fd = network_client_socket(backend->host, backend->port);
    if (fd <= 0) {
        ERROR("error on get new connection to backend");
        return NULL;
    }

    conn = (mongo_conn_t *) malloc(sizeof(mongo_conn_t));
    conn->next = NULL;
    conn->backend = backend;
    conn->fd = fd;
    conn->conn_state = MONGO_CONN_STATE_CONNECTING;
    conn->ev = event_new(g_server.event_base, fd, EV_READ, NULL, NULL);

    backend->connection_cnt++;

    return conn;
}

mongo_conn_t *mongo_backend_get_conn(mongo_backend_t * backend)
{
    mongo_conn_t *conn;
    if (backend->free_conn) {   //get a free conn on backend
        conn = backend->free_conn;
        backend->free_conn = conn->next;
        return conn;
    } else {                    // new conn on backend
        return mongo_backend_new_conn(backend);
    }
}

/*if no replset, the only backend is primary*/

mongo_conn_t *mongo_replset_get_conn(mongo_replset_t * replset, int primary)
{
    mongo_backend_t *backend;
    mongo_conn_t *conn;
    int i;

    if (replset->slave_cnt == 0) {
        primary = 1;
        TRACE("no slaves, we will use primary");
    }

    if (primary) {
        backend = replset->primary;
        return mongo_backend_get_conn(backend);
    }
    // find one slave conn
    for (i = 0; i < replset->slave_cnt; i++) {
        backend = replset->slaves[i];
        if (backend->free_conn) {
            conn = backend->free_conn;
            backend->free_conn = conn->next;
            return conn;
        }
    }

    //not found , we have to new one conn
    //first we find the backend with smallest connection_cnt;
    backend = replset->slaves[0];
    for (i = 0; i < replset->slave_cnt; i++) {
        if (replset->slaves[i]->connection_cnt < backend->connection_cnt)
            backend = replset->slaves[i];
    }
    return mongo_backend_new_conn(backend);
}

int mongo_replset_release_conn(mongo_conn_t * conn)
{
    mongo_backend_t *backend = conn->backend;
    conn->next = backend->free_conn;
    backend->free_conn = conn;
    return 0;
}

#define _min(x,y) ((x)<(y)?(x):(y))

/*
 * parse config like "1.1.1.1:9527,2.2.2.2:9258"
 * return end possition
 * TODO: need unit test
 * */
char *_parse_next_ip_port(char *s, char *host, int host_len, int *p_port)
{
    char *p1 = NULL;
    char *p2 = NULL;
    p1 = strchr(s, ',');
    p2 = strchr(s, ':');

    strncpy(host, s, _min(p2 - s, host_len));
    if (!p2) {
        return NULL;
    }
    if (p2 - s >= host_len) {
        return NULL;
    }
    host[p2 - s] = '\0';

    *p_port = atoi(p2 + 1);
    if (p1)
        return p1 + 1;
    else
        return NULL;
}

static int mongo_backend_is_equal(char *host1, int port1, char *host2, int port2)
{

    return (strcmp(host1, host2) == 0) && (port1 == port2);
}

static int mongo_replset_exist(mongo_replset_t * replset, char *host, int port)
{
    int i;
    if (replset->primary && mongo_backend_is_equal(host, port, replset->primary->host, replset->primary->port)) {
        return 1;
    }
    for (i = 0; i < replset->slave_cnt; i++) {
        if (mongo_backend_is_equal(host, port, replset->slaves[i]->host, replset->slaves[i]->port)) {
            return 1;
        }
    }
    return 0;
}

int mongo_replset_update(mongo_replset_t * replset, buffer_t * hosts, buffer_t * primary)
{
    char host[256];
    int port;
    char *p = hosts->ptr;
    int i, j;

    while (p && *p) {
        p = _parse_next_ip_port(p, host, sizeof(host), &port);
        DEBUG("parse hosts get: %s:%d", host, port);
        if (!mongo_replset_exist(replset, host, port)) {
            replset->slaves[replset->slave_cnt] = mongo_backend_new(host, port);
            replset->slave_cnt++;
        }
        DEBUG("[slave_cnt:%d]", replset->slave_cnt);
    }

    //check if primary changed
    p = _parse_next_ip_port(primary->ptr, host, sizeof(host), &port);

    if (!replset->primary) {
        for (i = 0; i < replset->slave_cnt; i++) {
            if (mongo_backend_is_equal(host, port, replset->slaves[i]->host, replset->slaves[i]->port)) {
                replset->primary = replset->slaves[i];
            }
        }
    }

    if (replset->primary && !mongo_backend_is_equal(host, port, replset->primary->host, replset->primary->port)) {  //we got a new primary

        replset->slaves[replset->slave_cnt] = replset->primary;
        replset->slave_cnt++;

        for (i = 0; i < replset->slave_cnt; i++) {
            if (mongo_backend_is_equal(host, port, replset->slaves[i]->host, replset->slaves[i]->port)) {
                replset->primary = replset->slaves[i];
            }
        }
        j = 0;
        for (i = 0; i < replset->slave_cnt; i++) {
            if (replset->primary != replset->slaves[i]) {
                replset->slaves[j++] = replset->slaves[i];
            }
        }
        replset->slave_cnt = j;
        TRACE("update set [slave_cnt:%d]", replset->slave_cnt);
    }
    return 0;
}

int mongo_replset_init(mongo_replset_t * replset, mongoproxy_cfg_t * cfg)
{
    char host[256];
    int port;
    char *p = cfg->backend;
    while (p && *p) {
        p = _parse_next_ip_port(p, host, sizeof(host), &port);
        DEBUG("parse backend get: %s:%d", host, port);
        replset->slaves[replset->slave_cnt] = mongo_backend_new(host, port);
        replset->slave_cnt++;
    }

    if ((replset->slave_cnt > 1) && (!cfg->use_replset)) {
        ERROR("!replset, too many backend!!: %s", cfg->backend);
        return -1;
    }

    if (replset->slave_cnt == 0) {
        ERROR("backend config error");
        return -1;
    }

    mongo_replset_set_check_isprimary(replset);

    replset->primary = replset->slaves[replset->slave_cnt - 1];
    replset->slave_cnt--;
    return 0;
}

int mongo_replset_set_check_isprimary(mongo_replset_t * replset)
{
    mongoproxy_cfg_t *cfg = &(g_server.cfg);
    int i;
    //get conn for every backend
    for (i = 0; i < replset->slave_cnt; i++) {
        mongoproxy_session_t *sess = mongoproxy_session_new();
        mongoproxy_set_state(sess, SESSION_STATE_PROCESSING);

        sess->backend_conn = mongo_backend_get_conn(replset->slaves[i]);
        buffer_copy_memory(sess->buf, g_server.msg_ismaster.ptr, g_server.msg_ismaster.used);   //copy the ismaster msg

        struct timeval tm;
        evutil_timerclear(&tm);
        tm.tv_sec = cfg->ping_interval / 1000;  // second
        tm.tv_usec = cfg->ping_interval % 1000; // u second  TODO. 1000*1000?

        event_assign(sess->backend_conn->ev, g_server.event_base, sess->backend_conn->fd, EV_WRITE,
                     mongo_backend_on_event, sess);
        event_add(sess->backend_conn->ev, &tm);
    }

    return 0;
}

