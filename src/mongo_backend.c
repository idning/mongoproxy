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

/*void mongo_backend_ismaster_on_read(int fd, short ev, void *arg)*/
/*{*/
    /*DEBUG("[fd:%d] ismaster on read", fd);*/
    /*buffer_t * buf = buffer_new(1024*8);*/
    /*int ismaster;*/
    /*buffer_t * hosts = buffer_new(1024*8);*/

    /*len = network_read(fd, buf->ptr, buf->size);*/
    /*if (len < 0) {*/
        /*ERROR("error on read [errno:%d(%s)]", errno, strerror(errno));*/
        /*return;*/
    /*}*/
    /*if (len == 0) {*/
        /*ERROR("lost connection [errno:%d(%s)]", errno, strerror(errno));*/
        /*close(fd);*/
        /*return;*/
    /*}*/
    /*buf->used += len;*/

    /*mongomsg_decode_ismaster(buf, &ismaster, hosts);*/
    /*TRACE("ismaster return hosts: %s", host->ptr);*/
/*}*/

/*void mongo_backend_ismaster_on_write(int fd, short ev, void *arg)*/
/*{*/
    /*DEBUG("[fd:%d] ismaster on write", fd);*/

    /*buffer_t * buf = &(g_server.msg_ismaster);*/

    /*len = network_write(fd, buf->ptr, buf->used);*/
    /*if (len < 0) {*/
        /*ERROR("[fd:%d]error on write [errno:%d(%s)]", fd, errno, strerror(errno));*/
        /*return;*/
    /*}*/
    /*if (len == sess->buf->used) {   //all sent*/
        /*event_assign(conn->ev, g_server.event_base, fd, EV_READ, mongo_backend_ismaster_on_read, NULL);*/
        /*event_add(conn->ev, NULL);*/
    /*}*/

/*}*/


void mongo_backend_on_read(int fd, short ev, void *arg)
{
    int len;

    DEBUG("[fd:%d] on read", fd);
    mongoproxy_session_t *sess;
    sess = (mongoproxy_session_t *) arg;

    sess->buf->used = 0;
    len = network_read(fd, sess->buf->ptr, sess->buf->size);
    if (len < 0) {
        ERROR("error on read [errno:%d(%s)]", errno, strerror(errno));
        return;
    }
    if (len == 0) {
        ERROR("lost connection [errno:%d(%s)]", errno, strerror(errno));
        close(fd);
        return;
    }
    sess->buf->used += len;
    mongoproxy_state_machine(sess);
}

void mongo_backend_on_write(int fd, short ev, void *arg)
{
    int len;

    DEBUG("[fd:%d] on connected", fd);

    mongoproxy_session_t *sess = (mongoproxy_session_t *) arg;
    mongo_conn_t *conn = sess->backend_conn;

    mongo_conn_set_state(conn, MONGO_CONN_STATE_SEND_REQUEST);

    util_print_buffer("sess->buf", sess->buf);
    len = network_write(fd, sess->buf->ptr, sess->buf->used);
    if (len < 0) {
        ERROR("[fd:%d]error on write [errno:%d(%s)]", fd, errno, strerror(errno));
        return;
    }
    if (len == sess->buf->used) {   //all sent
        mongo_conn_set_state(conn, MONGO_CONN_STATE_RECV_RESPONSE);
        //enable read event
        //
        event_assign(conn->ev, g_server.event_base, fd, EV_READ, mongo_backend_on_read, sess);
        event_add(conn->ev, NULL);
    }
}

mongo_backend_t *mongo_backend_new(char *host, int port)
{
    mongo_backend_t *backend = malloc(sizeof(mongo_backend_t));
    memset(backend, 0, sizeof(mongo_backend_t));

    backend->host = strdup(host);
    backend->port = port;
    backend->is_primary = 0;

    backend->host_port = buffer_new(0);

    buffer_append_printf(backend->host_port, "%s:%d", host, port);
    
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
        if (backend->free_conn) {   //get a free conn on primary
            conn = backend->free_conn;
            backend->free_conn = conn->next;
            return conn;
        } else {                // new conn on primary
            return mongo_backend_new_conn(backend);
        }
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
    backend = replset->primary;
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
        return p1+1;
    else 
        return NULL;
}


/*static int mongo_replset_exist2(mongo_replset_t * replset, buffer_t * host_port){*/
    /*int i;*/
    /*if (buffer_is_equal(host_port, replset->primary->host_port)){*/
        /*return 1;*/
    /*}*/
    /*for (i=0; i< replset->slave_cnt; i++){*/
        /*if (buffer_is_equal(host_port, replset->slaves[i]->host_port)){*/
            /*return 1;*/
        /*}*/
    /*}*/
    /*return 0;*/
/*}*/

static int mongo_backend_is_equal(char * host1, int port1, char *host2, int port2){

    return (strcmp(host1, host2) == 0) && (port1 == port2);
}

static int mongo_replset_exist(mongo_replset_t * replset, char * host, int port)
{
    int i;
    if (replset->primary && mongo_backend_is_equal(host, port, replset->primary->host, replset->primary->port)){
        return 1;
    }
    for (i=0; i< replset->slave_cnt; i++){
        if (mongo_backend_is_equal(host, port, replset->slaves[i]->host, replset->slaves[i]->port)){
            return 1;
        }
    }
    return 0;
}

int mongo_replset_update(mongo_replset_t * replset, buffer_t * hosts, buffer_t * primary){
    char host[256];
    int port;
    char *p = hosts->ptr;
    int i;

    while (p && *p) {
        p = _parse_next_ip_port(p, host, sizeof(host), &port);
        DEBUG("parse hosts get: %s:%d", host, port);
        if (!mongo_replset_exist(replset, host, port)){
            replset->slaves[replset->slave_cnt] = mongo_backend_new(host, port);
            replset->slave_cnt++;
        }
    }

    //check if primary changed
    p = _parse_next_ip_port(primary->ptr, host, sizeof(host), &port);

    if(!replset->primary){
        for (i=0; i< replset->slave_cnt; i++){
            if (mongo_backend_is_equal(host, port, replset->slaves[i]->host, replset->slaves[i]->port)){
                replset->primary = replset->slaves[i];
            }
        }
    }

    if (replset->primary && !mongo_backend_is_equal(host, port, replset->primary->host, replset->primary->port)){ //we got a new primary
        //TODO: do something.
    }
}

int mongo_replset_init(mongo_replset_t * replset, mongoproxy_cfg_t * cfg)
{
    char host[256];
    int port;
    char *p = cfg->backend;
    int i ;
    mongo_conn_t * conn = NULL;
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

    //get conn for every backend
    //
    for (i=0; i< replset->slave_cnt; i++){
        mongoproxy_session_t *sess = mongoproxy_session_new();
        mongoproxy_set_state(sess, SESSION_STATE_PROCESSING);

        sess->backend_conn = mongo_backend_new_conn(replset->slaves[i]);
        buffer_copy_memory(sess->buf, g_server.msg_ismaster.ptr, g_server.msg_ismaster.used); //copy the ismaster msg

        event_assign(sess->backend_conn->ev, g_server.event_base, sess->backend_conn->fd, EV_WRITE, mongo_backend_on_write, sess);
        event_add(sess->backend_conn->ev, NULL);
    }

    replset->primary = replset->slaves[replset->slave_cnt - 1];
    replset->slave_cnt--;
    return 0;
}

int mongo_replset_set_primary(char *host, int port)
{
    return 0;

}
