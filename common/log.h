#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _LOG_H__
#define _LOG_H__

#define  LOG_CONN 0
#define  LOG_DEUBG 1
#define  LOG_INFO 2
#define  LOG_WARN 3
#define  LOG_ERROR 4
// which >=LOG_LEVEL  will be print


//#define DEBUG 1
//
//#ifdef DEBUG
//#define  LOG_LEVEL LOG_DEUBG
//#else
//#define  LOG_LEVEL LOG_WARN
//#endif

#define  LOG_LEVEL LOG_WARN

int logging(int level, char *fmt, ...);
int log_init(char *logfile);

#define DBG() (logging(LOG_DEUBG, "%s:%s: called\n", __FILE__, __func__))

#endif
