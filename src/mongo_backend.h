#ifndef  __MONGO_SERVER_H_
#define  __MONGO_SERVER_H_

// 12 is enough
#define  MONGO_MAX_SERVERS 32

#include "mongoproxy.h"
#include "mongo_backend.h"

typedef struct mongo_conn_s mongo_conn_t;

typedef struct mongo_backend_s {
    char *host;
    int port;
    int is_primary;
    int last_ping;

    int connection_cnt;             // used for load balance
    mongo_conn_t * free_conn;

} mongo_backend_t;


typedef enum mongo_conn_state_s {
    MONGO_CONN_STATE_UNSET = 0, 
    MONGO_CONN_STATE_CONNECTING, 
    MONGO_CONN_STATE_CONNECTED, 
    MONGO_CONN_STATE_SEND_REQUEST, 
    MONGO_CONN_STATE_RECV_RESPONSE, 
    MONGO_CONN_STATE_CLOSED
}mongo_conn_state_t;


typedef struct mongo_replset_s {
    int slave_cnt;
    mongo_backend_t *slaves[MONGO_MAX_SERVERS];
    mongo_backend_t *primary;
} mongo_replset_t;


mongo_conn_t *mongo_backend_new_conn(mongo_backend_t * backend);

//void mongo_replset_init(mongo_replset_t * replset, mongoproxy_cfg_t * cfg);
mongo_conn_t *mongo_replset_get_conn(mongo_replset_t* replset, int primary);

int mongo_replset_release_conn(mongo_conn_t * conn);

int mongo_replset_init(mongo_replset_t* replset, char * backen);

struct mongo_conn_s {
    mongo_conn_t *next;  // in free_conn linked list
    mongo_backend_t *backend;
    int fd;
    struct event ev;
    mongo_conn_state_t conn_state;
};

void mongo_conn_connect(mongo_conn_t * conn);
void mongo_conn_send(mongo_conn_t * conn, void *buf, int len);
void mongo_conn_recv(mongo_conn_t * conn, void *buf, int len);
void mongo_conn_close(mongo_conn_t * conn);


#endif
