/*
 *  file   : mongoproxy.h
 *  author : ning
 *  date   : 2012-09-24 16:38:05
 */

#ifndef _MONGOPROXY_H_
#define _MONGOPROXY_H_

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

//#include <event.h>
#include <event2/event.h>

#include "log.h"
#include "cfg.h"
#include "buffer.h"
#include "network.h"
#include "utils.h"

typedef struct mongoproxy_cfg_s {
    char *backend;
    char *listen_host;
    int listen_port;
    int use_replset;
    int ping_interval;          //ping interval 
    int check_interval;         //is_master interval
} mongoproxy_cfg_t;

#include "mongo_backend.h"
#include "mongoproxy_session.h"

typedef struct mongoproxy_server_s {
    mongoproxy_cfg_t cfg;
    mongo_replset_t replset;
    struct event_base *event_base;
} mongoproxy_server_t;

int mongoproxy_init();
int mongoproxy_mainloop();

extern mongoproxy_server_t g_server;

#define MONGOPROXY_DEFAULT_BUF_SIZE (1024*1024*4)

#endif
