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
    if (!sess)
        return;

    if (sess->ev) {
        event_free(sess->ev);
        sess->ev= NULL;
    }
    if (sess->client_ip) {
        free(sess->client_ip);
        sess->client_ip= NULL;
    }
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
    if (sess->backend_conn){
        mongo_replset_release_conn(sess->backend_conn);
        sess->backend_conn = NULL;
        return 0;
    }
    if (sess->fd){
        close(sess->fd);
    }

    return 0;
}

int mongoproxy_session_select_backend(mongoproxy_session_t * sess, int primary)
{
    mongo_replset_t *replset = &(g_server.replset);
    if (sess->backend_conn) {   //already got a connection
        if (sess->backend_conn->backend == replset->primary) {
            return 0;
        } else {                //my conn is not to primary
            if (primary) {
                mongo_replset_release_conn(sess->backend_conn);
                sess->backend_conn = NULL;
            } else {
                return 0;
            }
        }
    }
    DEBUG("we will use a new backend_conn, [primary:%d]", primary);

    sess->backend_conn = mongo_replset_get_conn(replset, primary);
    if (!sess->backend_conn) {
        ERROR("get no connection");
        return -1;
    }

    return 0;
}
