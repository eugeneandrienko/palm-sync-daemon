/**
   @author Eugene Andrienko
   @brief Simple logging subsystem realization
*/

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "log.h"

#define LOG_BUFFER_SIZE 200 /** Size of buffer for log message strings */

static int foreground = 0; /** 0 if program runs as daemon, 1 if program runs foreground */

/**
   Initialize logging system.

   @param fg 0 if program runs as daemon, 1 if program runs foreground
*/
void log_init(int fg)
{
	foreground = fg;
	if(!foreground)
	{
		openlog(PACKAGE_NAME, LOG_PID, LOG_DAEMON);
	}
}

/**
   Writes message to log.

   Records message to syslog.
   Last parameters - parameters for format string.

   @param priority log priority (as for syslog(...))
   @param format Format string
*/
void log_write(int priority, const char * format, ...)
{
	va_list vlist;
	va_start(vlist, format);

	if(foreground)
	{
		char * priorityStr = "";
		char timeStr[LOG_BUFFER_SIZE] = "\0";
		char messageStr[LOG_BUFFER_SIZE] = "\0";
		time_t currentTime;

		switch(priority)
		{
		case LOG_EMERG:
			priorityStr = "EMERGENCY";
			break;
		case LOG_ALERT:
			priorityStr = "ALERT";
			break;
		case LOG_CRIT:
			priorityStr = "CRITICAL";
			break;
		case LOG_ERR:
			priorityStr = "ERROR";
			break;
		case LOG_WARNING:
			priorityStr = "WARNING";
			break;
		case LOG_NOTICE:
			priorityStr = "NOTICE";
			break;
		case LOG_INFO:
			priorityStr = "INFO";
			break;
		case LOG_DEBUG:
			priorityStr = "DEBUG";
			break;
		default:
			priorityStr = "UNKNOWN PRIORITY";
		}

		time(&currentTime);
		if(currentTime == ((time_t) -1))
		{
			sprintf(messageStr, "UNKNOWN TIME");
			fprintf(stderr, "%s: Cannot get current time: %s", PACKAGE_NAME, strerror(errno));
		}
		else
		{
			sprintf(timeStr, "%s", ctime(&currentTime));
			timeStr[strlen(timeStr) - 1] = '\0';
		}
		fprintf(stderr, "%s [%s]: ", timeStr, priorityStr);
		vsprintf(messageStr, format, vlist);
		fprintf(stderr, "%s\n", messageStr);
	}
	else
	{
		vsyslog(priority, format, vlist);
	}

	va_end(vlist);
}

/**
   Close logging system.
*/
void log_close()
{
	if(!foreground)
	{
		closelog();
	}
}
