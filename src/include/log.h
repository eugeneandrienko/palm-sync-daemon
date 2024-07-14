/**
   @author Eugene Andrienko
   @brief Simple logging subsystem realization
   @file log.h

   Supports writing to STDERR or to system logger via syslog() GNU C library
   call.
*/

/**
   @page logging Logging subsystem

   There is a simple and naive realization of logging subsystem.

   To initialize logging call log_init() after program starts. Module can log
   into syslog (via syslog()) or to console (directly to STDERR). **Note:** if
   program runs in daemon mode — it is meaningless to log to console, because
   process will be disconnected from terminal and close STDERR.

   Module can supress debug message by passing debug parameter equals to zero to
   log_init().

   After initialization log_write() can be called. To reduce unnecessary actions
   if debug messages are supressed — log_is_debug() function exists.

   Before process end call log_close() function to correctly close logging
   facility.
*/

#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>

/**
   Init logging facility.

   Must be called before start using of logging facility. Option fg = 1 is
   meaningless if program started in daemon mode.

   @param[in] fg Set to 0 to write to syslog. Set to 1 to write directly to STDERR.
   @param[in] debug Set to 0 to skip debug messages, and to 1 to print debug messages.
*/
void log_init(int fg, int debug);

/**
   Write message to log.

   @param[in] priority Log priority as in syslog() call from GNU C library.
   @param[in] format Format string for message.
   @param[in] ... Parameters for format string.
*/
void log_write(int priority, const char * format, ...);

/**
   Shutdown logging system.

   Must be called on program shutdown. Any calls to other functions from this
   module (except log_init()) is meaningless after this call.
*/
void log_close();

/**
   Check is debug logging level enabled.

   Return non-zero value if debug messages can be written to log.

   @return Non-zero value if debug mode is enabled.
*/
int log_is_debug();

#endif
