#ifndef  __MONGO_SERVER_H_
#define  __MONGO_SERVER_H_

#define  MONGO_MAX_SERVERS 12

#include "mongoproxy.h"

typedef struct mongo_conn_s mongo_conn_t;

typedef struct mongo_server_s {
    char *host;
    int port;
    int is_master;
    int last_ping;

    int connection_cnt;             // used for load balance
    mongo_conn_t * free_conn;

} mongo_server_t;


typedef enum mongo_conn_state_s {
    MONGO_CONN_STATE_UNSET = 0, 
    MONGO_CONN_STATE_CONNECTING, 
    MONGO_CONN_STATE_CONNECTED, 
    MONGO_CONN_STATE_SEND_REQUEST, 
    MONGO_CONN_STATE_RECV_RESPONSE, 
    MONGO_CONN_STATE_CLOSED
}mongo_conn_state_t;

struct mongo_conn_s {
    struct mongo_conn_s *next;  // in free_conn linked list
    mongo_server_t *server;
    int fd;
    mongo_conn_state_t  conn_state;
};

void mongo_conn_connect(mongo_conn_t * conn);
void mongo_conn_send(mongo_conn_t * conn, void *buf, int len);
void mongo_conn_recv(mongo_conn_t * conn, void *buf, int len);
void mongo_conn_close(mongo_conn_t * conn);

typedef struct mongo_replset_s {
    int replset_size;
    mongo_server_t *slaves[MONGO_MAX_SERVERS];
    mongo_server_t *master;
} mongo_replset_t;


mongo_conn_t *mongo_server_new_conn(mongo_server_t * server);


void mongo_replset_init(mongo_replset_t * replica_set, mongoproxy_cfg_t * cfg);
mongo_conn_t *mongo_replset_get_conn(mongo_replset_t* replica_set, int primary);

mongo_conn_t *mongo_replset_release_conn(mongo_conn_t * conn);

#endif
