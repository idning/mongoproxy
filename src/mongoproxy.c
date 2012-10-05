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

/*#define MONGO_HEAD_LEN (sizeof(int))*/
#define MONGO_HEAD_LEN (sizeof(mongomsg_header_t))

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

void mongoproxy_set_state(mongoproxy_session_t * sess, mongoproxy_session_state_t state)
{
    DEBUG("set mongo_proxy_state %s => %s",
          mongoproxy_session_state_name(sess->proxy_state), mongoproxy_session_state_name(state));
    sess->proxy_state = state;
}

int mongo_backend_handler_ismaster(mongoproxy_session_t * sess)
{
    int ismaster;
    buffer_t * hosts = buffer_new(1024);
    buffer_t * primary = buffer_new(1024);
    int ret; 
    ret = mongomsg_decode_ismaster(sess->buf, &ismaster, hosts, primary);
    TRACE("we got ismaster response: [ismaster:%d] [hosts:%s] [primary:%s]", ismaster, hosts->ptr, primary->ptr);
    mongo_replset_update(&(g_server.replset), hosts, primary);
    buffer_free(hosts);
    buffer_free(primary);

    return 0;
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
            mongo_conn_set_state(conn, MONGO_CONN_STATE_CONNECTED);
            if (sess->fd){ //commong session connection.
                mongoproxy_set_state(sess, SESSION_STATE_SEND_BACK_TO_CLIENT);
            } else {       // connection for `ping` and `ismaster`
                mongo_backend_handler_ismaster(sess); //TODO : relaeae conn and session obj
                mongoproxy_session_close(sess);
                mongoproxy_session_free(sess);
            }
        }
    }
    return 0;
}

const char * _mongo_proxy_op_code2str(int op)
{
    switch (op)
    {
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
if need send to primary, return 1
*/
static int _mongoproxy_query_should_sendto_primary(mongoproxy_session_t * sess){

    mongoproxy_cfg_t *cfg = &(g_server.cfg);
    int *flag;

    mongomsg_header_t * header = (mongomsg_header_t *)sess->buf->ptr;
    TRACE("[opcode: %s]", _mongo_proxy_op_code2str(header->op_code));
    if (header->op_code == OP_UPDATE 
            || header->op_code == OP_DELETE
            || header->op_code == OP_INSERT){
        return 1;
    }

    if (cfg->slaveok){ //set "slaveOk" flag
        DEBUG("set 'slaveOk' flag");
        flag = (int*)((void*)sess->buf->ptr + sizeof(mongomsg_header_t));
        *flag =(*flag) | (1<<2);
    }

    return 0;
}

int mongoproxy_state_machine(mongoproxy_session_t * sess)
{
    DEBUG("in state machine [sess->proxy_state:%s]", mongoproxy_session_state_name(sess->proxy_state));

    mongo_replset_t *replset;
    int primary = 0;

    replset = &(g_server.replset);

    if (sess->proxy_state == SESSION_STATE_READ_CLIENT_REQUEST) {
        if (_mongoproxy_read_done(sess)) {
            mongoproxy_set_state(sess, SESSION_STATE_PROCESSING);
            primary = _mongoproxy_query_should_sendto_primary(sess);
            mongoproxy_session_select_backend(sess, primary);

            mongo_conn_t *conn = sess->backend_conn;
            event_assign(conn->ev, g_server.event_base, conn->fd, EV_WRITE, mongo_backend_on_write, sess);
            event_add(conn->ev, NULL);

            /*return 0; */
        }
    }
    if (sess->proxy_state == SESSION_STATE_PROCESSING) {    //run sub state machine
        mongo_conn_state_machine(sess);
    }

    if (sess->proxy_state == SESSION_STATE_SEND_BACK_TO_CLIENT) {
        DEBUG("[fd:%d] enable write event ", sess->fd);
        event_assign(sess->ev, g_server.event_base, sess->fd, EV_WRITE, on_event, sess);
        event_add(sess->ev, NULL);
    } else if (sess->proxy_state == SESSION_STATE_READ_CLIENT_REQUEST) {
        DEBUG("[fd:%d] enable read event ", sess->fd);
        event_assign(sess->ev, g_server.event_base, sess->fd, EV_READ, on_event, sess);
        event_add(sess->ev, NULL);
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
            /*event_add(sess->ev, NULL); */
            /*sess->buf->used=0; */
        } else if (ret == EVENT_HANDLER_ERROR) {

        } else {//error
            DEBUG("[fd:%d] we will close ", fd);
            close(fd);
            mongoproxy_session_close(sess);
            mongoproxy_session_free(sess);
        }
    }

    if (what & EV_WRITE) {
        int ret = on_write(fd, sess);
        if (ret == EVENT_HANDLER_WAIT_FOR_EVENT) {
            mongoproxy_set_state(sess, SESSION_STATE_READ_CLIENT_REQUEST);
            sess->buf->used = 0;
            mongoproxy_state_machine(sess);
            /*event_add(sess->ev, NULL); */
        } else if (ret == EVENT_HANDLER_ERROR) {

        } else {//error

        }
    }
}

int mongoproxy_print_status()
{
    mongo_replset_t *replset = &(g_server.replset);
    mongo_backend_t * backend;
    int i;

    char buf[1024];

    backend = replset->primary;
    TRACE("primary [%s:%d=>%d]", 
            backend->host, backend->port,backend->connection_cnt );

    for (i=0; i< replset->slave_cnt; i++){
        backend = replset->slaves[i];
        TRACE("slaves [%s:%d=>%d]", 
                backend->host, backend->port,backend->connection_cnt );
    }
}

void on_timer(int fd, short what, void *arg){
    DEBUG("[fd:%d] on timer", fd);
    mongo_replset_t *replset = &(g_server.replset);

    mongo_replset_set_check_isprimary(replset);

    mongoproxy_print_status();
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
    mongoproxy_set_state(sess, SESSION_STATE_READ_CLIENT_REQUEST);

    sess->ev = event_new(g_server.event_base, client_fd, EV_READ, on_event, sess);
    event_add(sess->ev, NULL);
}

int mongoproxy_init()
{
    //init libevent
    event_init();
    g_server.event_base = event_base_new();

    //init msg
    mongomsg_encode_ismaster(&(g_server.msg_ismaster));
    mongomsg_encode_ping(&(g_server.msg_ping));


    mongo_replset_t *replset = &(g_server.replset);
    mongoproxy_cfg_t *cfg = &(g_server.cfg);
    cfg->backend = strdup(cfg_getstr("MONGOPROXY_BACKEND", ""));
    cfg->listen_host = strdup(cfg_getstr("MONGOPROXY_BIND", "0.0.0.0"));
    cfg->listen_port = cfg_getint32("MONGOPROXY_PORT", 7111);
    cfg->use_replset = cfg_getint32("MONGOPROXY_USE_REPLSET", 0);
    cfg->slaveok = cfg_getint32("MONGOPROXY_REPLSET_ENABLE_SLAVE", 1);

    cfg->ping_interval = cfg_getint32("MONGOPROXY_PING_INTERVAL", 1000);
    cfg->check_interval = cfg_getint32("MONGOPROXY_CHECK_INTERVAL", 1000);

    if (strlen(cfg->backend) == 0) {
        ERROR("no backend");
        return -1;
    }

    return mongo_replset_init(replset, cfg);
}

int mongoproxy_mainloop()
{
    struct event *ev_accept, *ev_timer;
    struct timeval tm;
    mongoproxy_cfg_t *cfg = &(g_server.cfg);
    int listen_fd;

    listen_fd = network_server_socket(cfg->listen_host, cfg->listen_port);

    //init ev_accept
    ev_accept = event_new(g_server.event_base, listen_fd, EV_READ | EV_PERSIST, on_accept, NULL);
    event_add(ev_accept, NULL);

    //init ev_timer
    evutil_timerclear(&tm);
    tm.tv_sec = cfg->ping_interval / 1000;  // second
    tm.tv_usec = cfg->ping_interval % 1000; // u second  TODO. 1000*1000?
    ev_timer = event_new(g_server.event_base, -1, EV_PERSIST, on_timer, NULL);
    event_add(ev_timer, &tm);

    /* Start the libevent event loop. */
    event_base_dispatch(g_server.event_base);
    return 0;
}

