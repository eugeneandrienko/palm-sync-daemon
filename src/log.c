#include <stdarg.h>
#include <stdio.h>

#include "config.h"
#include "log.h"


static int foreground = 0;

void log_init(int fg)
{
	foreground = fg;
	if(!foreground)
	{
		openlog(PACKAGE_NAME, LOG_PID, LOG_DAEMON);
	}
}

void log_write(int priority, const char * format, ...)
{
	va_list vlist;
	va_start(vlist, format);

	if(foreground)
	{
		char * priorityStr = "";
		char * messageStr = "";

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

		// TODO: time instead of name:
		fprintf(stderr, "%s [%s]: ", PACKAGE_NAME, priorityStr);
		sprintf(messageStr, format, vlist);
		fprintf(stderr, "%s\n", messageStr);
	}
	else
	{
		syslog(priority, format, vlist);
	}

	va_end(vlist);
}

void log_close()
{
	if(!foreground)
	{
		closelog();
	}
}
