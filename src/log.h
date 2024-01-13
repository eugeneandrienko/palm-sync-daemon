#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>

void log_init(int fg);
void log_write(int priority, const char * format, ...);
void log_close();

#endif
