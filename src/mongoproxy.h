/*
 *  * file   : mongoproxy.h
 *   * author : ning
 *    * date   : 2012-09-24 16:38:05
 *     */

#ifndef _MONGOPROXY_H_
#define _MONGOPROXY_H_


typedef struct mongoproxy_cfg_s {
    char *backend;
    char *listen_host;
    int listen_port;
} mongoproxy_cfg_t;

int mongoproxy_mainloop(); 

#endif
