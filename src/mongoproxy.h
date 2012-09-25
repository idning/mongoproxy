/*
 *  file   : mongoproxy.h
 *  author : ning
 *  date   : 2012-09-24 16:38:05
 */

#ifndef _MONGOPROXY_H_
#define _MONGOPROXY_H_

#include "mongoproxy_session.h"
#include "mongo_backend.h"

typedef struct mongoproxy_cfg_s {
    char *backend;
    char *listen_host;
    int listen_port;
} mongoproxy_cfg_t;

typedef struct mongoproxy_server_s{
    mongoproxy_cfg_t cfg;
    mongo_replset_t replset;
} mongoproxy_server_t;

int mongoproxy_mainloop(); 

extern mongoproxy_server_t g_server;

#endif
