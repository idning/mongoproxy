#ifndef  __MONGO_SERVER_H_
#define  __MONGO_SERVER_H_

#define  MONGO_MAX_SERVERS 12

typedef struct mongo_conn_s{
    struct mongo_conn_s * next; // in pool
    char * host;
    int port;
    int is_master;
    int fd;
}mongo_conn_t;

void mongo_conn_send(mongo_conn_t * conn, void * buf, int len);
void mongo_conn_recv(mongo_conn_t * conn, void * buf, int len);


typedef struct mongo_server_s{
    char * host;
    int port;
    int is_master;
    int last_ping;
}mongo_server_t;

typedef struct mongo_replica_set_s{
    mongo_server_t * master;
    mongo_server_t * slaves[MONGO_MAX_SERVERS];
}mongo_replica_set_t;




#endif
