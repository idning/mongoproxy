/*
 * file   : main.c
 * author : ning
 * date   : 2012-01-13 16:10:27
 */

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "log.h"
#include "cfg.h"

#include "mongoproxy.h"

#define __MONGOPROXY_VERSION__ "0.0.1"


int onexit()
{
    fprintf(stderr, "is going to exit!");
}

void sig_handler(int signum)
{
    onexit();
    exit(0);
}

void init_sig_handler()
{
    if (signal(SIGINT, sig_handler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, sig_handler) == SIG_IGN)
        signal(SIGHUP, SIG_IGN);
    if (signal(SIGTERM, sig_handler) == SIG_IGN)
        signal(SIGTERM, SIG_IGN);
}

int usage()
{
    printf("mongoproxy -c path_to_config, default: conf/mongoproxy.cfg \n");
    printf("mongoproxy -v : show version\n");
}

int init(int argc, char **argv)
{
    int ch;
    int rundaemon = 1;
    int logundefined = 1;
    char cfgfile[999] = "conf/mongoproxy.cfg";  //default cfg file

    init_sig_handler();

    while ((ch = getopt(argc, argv, "vduc:h?")) != -1) {
        switch (ch) {
        case 'd':              //no daemon
            rundaemon = 0;
            break;
        case 'c':
            strncpy(cfgfile, optarg, sizeof(cfgfile));
            break;
        case 'u':
            logundefined = 1;
            break;
        case 'v':
            printf("version: %s\n", __MONGOPROXY_VERSION__);
            exit(0);
        default:
            usage();
            exit(0);
        }
    }
    fprintf(stderr, "use %s as config file\n", cfgfile);

    if (cfg_load(cfgfile, logundefined) == 0) {
        fprintf(stderr, "can't load config file: %s\n", cfgfile);
        exit(0);
    }
    cfg_add("CFG_FILE", cfgfile);

    // set log
    char *logfile = cfg_getstr("MONGOPROXY_LOG_FILE", "log/mongoproxy.log");
    log_init(logfile);
    NOTICE("server start");

    int log_level = cfg_getint32("MONGOPROXY_LOG_LEVEL", 0);
    log_set_level(log_level);

    //set mox_files
    int max_files = cfg_getint32("MONGOPROXY_MAX_FILES", 65535);
    util_set_max_files(max_files);

    argc -= optind;
    argv += optind;
    return 0;
}


int main(int argc, char **argv)
{
    init(argc, argv);
    mongoproxy_mainloop();
    return 0;
}
