
#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define  LOG_ALL 0
#define  LOG_DEUBG 1
#define  LOG_TRACE 2
#define  LOG_NOTICE 4
#define  LOG_WARN 8
#define  LOG_ERROR 322

// which >=LOG_LEVEL  will be print

//#define DEBUG 1
//
//#ifdef DEBUG
//#define  LOG_LEVEL LOG_DEUBG
//#else
//#define  LOG_LEVEL LOG_WARN
//#endif

#define  LOG_LEVEL LOG_WARN

int log_print(int level, char *fmt, ...);
int log_init(char *logfile);
int log_set_level(int level);

#define REMOVE_PATH(file) (file)

#define ERROR(fmt, ...) \
    (log_print(LOG_ERROR,  "(%s:%d) [func:%s] "fmt, REMOVE_PATH(__FILE__), __LINE__, __func__, ## __VA_ARGS__), 0)

#define WARNING(fmt, ...) \
    (log_print(LOG_WARN,  "(%s:%d) [func:%s] "fmt, REMOVE_PATH(__FILE__), __LINE__, __func__, ## __VA_ARGS__), 0)

#define NOTICE(fmt, ...) \
    (log_print(LOG_NOTICE,  "(%s:%d) [func:%s] "fmt, REMOVE_PATH(__FILE__), __LINE__, __func__, ## __VA_ARGS__), 0)

#define TRACE(fmt, ...) \
    (log_print(LOG_TRACE,  "(%s:%d) [func:%s] "fmt, REMOVE_PATH(__FILE__), __LINE__, __func__, ## __VA_ARGS__), 0)

#define DEBUG(fmt, ...) \
    (log_print(LOG_DEUBG,  "(%s:%d) [func:%s] " fmt, REMOVE_PATH(__FILE__), __LINE__, __func__,  ## __VA_ARGS__), 0)

#endif

