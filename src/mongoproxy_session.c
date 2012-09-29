/*
 * file   : mongoproxy_session.c
 * author : ning
 * date   : 2012-09-24 00:54:30
 */

#include "mongoproxy.h"

mongoproxy_session_t *mongoproxy_session_new()
{
    mongoproxy_session_t *sess;

    sess = malloc(sizeof(*sess));
    if (!sess) {
        ERROR("error on malloc");
        return NULL;
    }
    memset(sess, 0, sizeof(*sess));

    DEBUG("proxy_session new : %p", sess);

    sess->proxy_state = SESSION_STATE_UNSET;
    sess->buf = buffer_new(MONGOPROXY_DEFAULT_BUF_SIZE);
    return sess;
}

void mongoproxy_session_free(mongoproxy_session_t * sess)
{
    DEBUG("proxy_session free: %p", sess);
    if (!sess)
        return;
    if (sess->buf) {
        buffer_free(sess->buf);
        sess->buf = NULL;
    }

    free(sess);
    sess = NULL;

    return;
}

int mongoproxy_session_close(mongoproxy_session_t * sess)
{
    mongo_replset_release_conn(sess->backend_conn);
    sess->backend_conn = NULL;
    /*mongoproxy_session_free(sess); */
    return 0;
}

int mongoproxy_session_select_backend(mongoproxy_session_t * sess, int primary)
{
    if (sess->backend_conn) {
        mongo_replset_release_conn(sess->backend_conn);
        sess->backend_conn = NULL;
    }
    mongo_replset_t *replset = &(g_server.replset);
    sess->backend_conn = mongo_replset_get_conn(replset, primary);
    if (!sess->backend_conn) {
        ERROR("get no connection");
        return -1;
    }

    mongo_conn_t *conn = sess->backend_conn;

    event_assign(conn->ev, g_server.event_base, conn->fd, EV_WRITE, mongo_backend_on_write, sess);
    event_add(conn->ev, NULL);

    return 0;
}
