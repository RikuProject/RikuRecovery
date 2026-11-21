#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static FILE *g_log;
static char g_path[256] = "";

void log_init(const char *path)
{
    if (g_log)
        fclose(g_log);
    snprintf(g_path, sizeof(g_path), "%s", path);
    g_log = fopen(g_path, "a");
    if (g_log)
        setvbuf(g_log, NULL, _IOLBF, 0);
}

const char *log_path(void)
{
    return g_path;
}

void log_msg(const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    if (g_log)
        fprintf(g_log, "[%5ld.%03ld] %s\n", (long)ts.tv_sec, ts.tv_nsec / 1000000, buf);
    fprintf(stderr, "[rec] %s\n", buf);
}