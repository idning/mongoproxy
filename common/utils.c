/*
 * file   : test_ptrace.c
 * author : ning
 * date   : 2012-01-13 16:10:27
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "log.h"

int util_set_max_files(int max_files)
{
    struct rlimit rls;
    rls.rlim_cur = max_files;
    rls.rlim_max = max_files;

    if (setrlimit(RLIMIT_NOFILE, &rls) < 0) {
        /*fprintf(stderr, "can't change open files limit to %u\n", max_files);*/
        WARNING("can't change open files limit to %u", max_files);
    }
    return 0;
}
