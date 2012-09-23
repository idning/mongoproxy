#ifndef  __MONGO_SERVER_H_
#define  __MONGO_SERVER_H_

#define  MONGO_MAX_SERVERS 12

typedef struct mongo_proxy_cfg_s {
    char *host;
    int port;
    int is_master;
    int last_ping;
} mongo_proxy_cfg_t;

typedef struct mongo_server_s {
    char *host;
    int port;
    int is_master;
    int last_ping;
} mongo_server_t;

typedef struct mongo_replica_set_s {
    mongo_server_t *master;
    mongo_server_t *slaves[MONGO_MAX_SERVERS];
} mongo_replica_set_t;

void mongo_replica_set_init(mongo_replica_set_t * replica_set, mongo_proxy_cfg_t * cfg);

typedef struct mongo_conn_s {
    struct mongo_conn_s *next;  // in free_conn linked list
    mongo_server_t *server;
    int fd;
} mongo_conn_t;

mongo_conn_t *mongo_conn_new(mongo_server_t * server);
void mongo_conn_connect(mongo_conn_t * conn);
void mongo_conn_send(mongo_conn_t * conn, void *buf, int len);
void mongo_conn_recv(mongo_conn_t * conn, void *buf, int len);
void mongo_conn_close(mongo_conn_t * conn);

#endif
