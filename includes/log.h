#ifndef LEARN_LIBAV_LOG_H
#define LEARN_LIBAV_LOG_H

#define DEBUG true

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static void logging(const char *fmt, ...) {
    if (!DEBUG) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
}

static void debug(const char *fmt, ...) {
    va_list args;
    fprintf(stdout, "\033[1;33m[DEBUG] ");
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\033[0m");
    fprintf(stdout, "\n");
}

#endif //LEARN_LIBAV_LOG_H
