/*
 * errlog.c — Error logger and diagnostics implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "errlog.h"

static FILE *log_file = NULL;
static int total_error_count = 0;

void errlog_init(const char *log_filepath) {
    if (log_filepath != NULL) {
        log_file = fopen(log_filepath, "a");
    }
    total_error_count = 0;
}

void errlog_report(int code, const char *phase, int line, int col, const char *fmt, ...) {
    char msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    /* Output to stderr */
    fprintf(stderr, "[E%03d] [%s] line %d, col %d: %s\n", code, phase, line, col, msg);

    /* Append to .errlog */
    if (log_file != NULL) {
        fprintf(log_file, "[E%03d] [%s] line %d, col %d: %s\n", code, phase, line, col, msg);
        fflush(log_file);
    }

    total_error_count++;
}

int errlog_get_count(void) {
    return total_error_count;
}

void errlog_reset_count(void) {
    total_error_count = 0;
}

void errlog_close(void) {
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}
