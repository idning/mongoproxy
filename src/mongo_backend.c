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

mongo_conn_t *mongo_replset_get_conn(mongo_replset_t* replset, int primary){
    mongo_backend_t * backend;
    mongo_conn_t *conn;
    int i;
    
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

mongo_conn_t *mongo_replset_release_conn(mongo_conn_t * conn){
    mongo_backend_t * backend = conn->backend;
    conn->next = backend->free_conn;
    backend->free_conn = conn;
}
