/*
 * file   : test_ptrace.c
 * author : ning
 * date   : 2012-01-13 16:10:27
 */

#include "mongoproxy.h"
#include "mongo_backend.h"

/*mongo_replset_t * replset;*/

void _mongo_backend_on_connected(int fd, short ev, void *arg){

}

mongo_backend_t * mongo_backend_new(char * host, int port){
    mongo_backend_t * backend = malloc(sizeof(mongo_backend_t));
    memset(backend, 0, sizeof(mongo_backend_t));

    backend -> host = strdup(host);
    backend -> port = port;
    backend -> is_primary = 0;
    return backend;
}

mongo_conn_t *mongo_backend_new_conn(mongo_backend_t * backend){
    mongo_conn_t * conn; 
    int fd;


    fd = network_client_socket(backend->host, backend->port);
    if (fd <= 0){
        ERROR("error on get new connection to backend");
    }

    conn = (mongo_conn_t * ) malloc(sizeof(mongo_conn_t));
    conn->next = NULL;
    conn->backend = backend;
    conn->fd = fd;
    conn->conn_state = MONGO_CONN_STATE_CONNECTING;

    backend->connection_cnt++;

    event_set(&(conn->ev), fd, EV_WRITE, _mongo_backend_on_connected, conn);
    event_add(&(conn->ev), NULL);

    return conn;
}

/*if no replset, the only backend is primary*/

mongo_conn_t *mongo_replset_get_conn(mongo_replset_t* replset, int primary){
    mongo_backend_t * backend;
    mongo_conn_t *conn;
    int i;
    
    if (replset->slave_cnt == 0){
        primary = 1;
        TRACE("no slaves, we will use primary");
    }

    if (primary){
        backend = replset->primary;
        if (backend->free_conn){ //get a free conn on primary
            conn = backend->free_conn;
            backend->free_conn = conn->next;
            return conn;
        }else{                  // new conn on primary
            return mongo_backend_new_conn(backend);
        }
    }

    // find one slave conn
    for (i=0; i<replset->slave_cnt; i++){
        backend = replset->slaves[i];
        if (backend->free_conn){
            conn = backend->free_conn;
            backend->free_conn = conn->next;
            return conn;
        }
    }

    //not found , we have to new one conn
    //first we find the backend with smallest connection_cnt;
    backend = replset->primary;
    for (i=0; i<replset->slave_cnt; i++){
        if(replset->slaves[i]->connection_cnt < backend->connection_cnt) 
            backend = replset->slaves[i];
    }
    return mongo_backend_new_conn(backend);
}

int mongo_replset_release_conn(mongo_conn_t * conn){
    mongo_backend_t * backend = conn->backend;
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
char * _parse_next_ip_port(char * s, char *host, int host_len, int * p_port){
    char * p1 = NULL;
    char * p2 = NULL;
    p1 = strchr(s, ',');
    p2 = strchr(s, ':');

    strncpy(host, s, _min(p2-s, host_len));
    if(!p2){
        return NULL;
    }
    if (p2-s >=host_len){
        return NULL;
    }
    host[p2-s] = '\0';

    *p_port = atoi(p2+1);
    return p1;
}

int mongo_replset_init(mongo_replset_t* replset, char * backend){
    char host[256];
    int port;
    char * p = backend;

    while(p && *p){
        p = _parse_next_ip_port(p, host, sizeof(host), &port);
        DEBUG("parse backend get: %s:%d", host, port);
        replset->slaves[replset->slave_cnt] = mongo_backend_new(host, port);
        replset->slave_cnt++;

    }
    if (replset->slave_cnt == 0){
        ERROR("backend config error");
        return -1;
    }
    replset->primary = replset->slaves[replset->slave_cnt-1];
    replset->slave_cnt --;
    return 0;
}

int mongo_replset_set_primary(char * host, int port){
    return 0;

}


