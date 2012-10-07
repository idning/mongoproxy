#include "log.h"
#include <pthread.h>

static int putthread(char **buf)
{
    pthread_t t = pthread_self();
    sprintf(*buf, "<%x>", (unsigned int)t);
    while (**buf)
        (*buf)++;
    return 0;
}

static int puttime(char **buf)
{
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;

    strftime(*buf, 30, "%Y-%m-%d %T.", localtime(&curtime));
    sprintf(*buf + 20, "%06ld ", tv.tv_usec);
    while (**buf)
        (*buf)++;
    return 0;
}

static int putlevel(char **buf, int level)
{
    switch (level) {
    case LOG_DEUBG:
        strcpy(*buf, "[DEBUG] ");
        *buf += strlen("[DEBUG] ");
        return 0;
    case LOG_TRACE:
        strcpy(*buf, "[TRACE] ");
        *buf += strlen("[TRACE] ");
        return 0;
    case LOG_NOTICE:
        strcpy(*buf, "[NOTICE] ");
        *buf += strlen("[NOTICE] ");
        return 0;
    case LOG_WARN:
        strcpy(*buf, "[WARN] ");
        *buf += strlen("[WARN] ");
        return 0;
    case LOG_ERROR:
        strcpy(*buf, "[ERROR] ");
        *buf += strlen("[ERROR] ");
        return 0;
    default:
        strcpy(*buf, "[UNKNOWN] ");
        *buf += strlen("[UNKNOWN] ");
        return 0;
    }
}

static FILE *fout = NULL;
static int log_level = 0;

int log_init(char *logfile)
{
    fprintf(stderr, "log_init:  %s\n", logfile);
    mkdir("log", 0755);
    fout = fopen(logfile, "a");
    return 0;
}

int log_print(int level, char *fmt, ...)
{
    char stmp[10240];
    char *buf = stmp;
    int ret;
    int size;

    if (level < log_level)
        return 0;
    puttime(&buf);
    putlevel(&buf, level);
    putthread(&buf);


    size = sizeof(stmp) - (buf-stmp);
    va_list ap;
    va_start(ap, fmt);
    ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);

    if(ret >= size ){//truncated.
        stmp[sizeof(stmp)-7] = '.';
        stmp[sizeof(stmp)-6] = '.';
        stmp[sizeof(stmp)-5] = '.';
        stmp[sizeof(stmp)-4] = '.';
        stmp[sizeof(stmp)-3] = '.';
        stmp[sizeof(stmp)-2] = '.';
        stmp[sizeof(stmp)-1] = '\0';
    }
#ifdef DDEBUG
    fprintf(stderr, "%s\n", stmp);
#endif
    if (fout) {
        fprintf(fout, "%s\n", stmp);
        fflush(fout);
    }

    return 0;
}

int log_set_level(int level)
{
    DEBUG("set log level to :%d", level);
    int old_level = log_level;
    log_level = level;
    /*DEBUG("set log level to :%d", level); */
    return old_level;
}
