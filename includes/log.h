#ifndef LEARN_LIBAV_LOG_H
#define LEARN_LIBAV_LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static void logging(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
}

#endif //LEARN_LIBAV_LOG_H
