/*
 * errlog.h — Error logging and diagnostics reporter for the compiler pipeline.
 *
 * Emits uniform diagnostics to stderr and appends structured entries to `.errlog`.
 */

#ifndef PSEUDO_ERRLOG_H
#define PSEUDO_ERRLOG_H

#include <stdarg.h>
#include <stdbool.h>

void errlog_init(const char *log_filepath);
void errlog_report(int code, const char *phase, int line, int col, const char *fmt, ...);
int errlog_get_count(void);
void errlog_reset_count(void);
void errlog_close(void);

#endif /* PSEUDO_ERRLOG_H */
