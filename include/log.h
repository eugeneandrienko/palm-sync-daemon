#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>

/**
   Initialize logging system.

   @param fg 0 if program runs as daemon, 1 if program runs foreground
   @param debug 0 to skip debug messages, 1 to print debug messages
*/
void log_init(int fg, int debug);

/**
   Writes message to log.

   Records message to syslog.
   Last parameters - parameters for format string.

   @param priority log priority (as for syslog(...))
   @param format Format string
*/
void log_write(int priority, const char * format, ...);

/**
   Close logging system.
*/
void log_close();

/**
   Check is debug mode enabled

   @return Non-zero value of debug mode is enabled
*/
int log_is_debug();

#endif
