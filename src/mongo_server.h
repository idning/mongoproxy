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

    mongo_conn_t * free_conn;
    int connection_cnt; // used for load balance
} mongo_server_t;

typedef struct mongo_replica_set_s {
    int replica_set_size;
    mongo_server_t *slaves[MONGO_MAX_SERVERS];
    mongo_server_t *master;
} mongo_replica_set_t;

void mongo_replica_set_init(mongo_replica_set_t * replica_set, mongo_proxy_cfg_t * cfg);

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

mongo_conn_t *mongo_server_new_conn(mongo_server_t * server);
//mongo_conn_t *mongo_server_get_conn(mongo_server_t * server);

//mongo_conn_t *mongo_replica_set_new_conn(mongo_replica_set_t* replica_set, int master);
mongo_conn_t *mongo_replica_set_get_conn(mongo_replica_set_t* replica_set, int master);

void mongo_conn_connect(mongo_conn_t * conn);
void mongo_conn_send(mongo_conn_t * conn, void *buf, int len);
void mongo_conn_recv(mongo_conn_t * conn, void *buf, int len);
void mongo_conn_close(mongo_conn_t * conn);


#endif
