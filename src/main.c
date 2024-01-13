/**
   @author Eugene Andrienko
   @brief Main file of palm-sync-daemon
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <popt.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "../config.h"

void _on_exit_actions()
{
	closelog();
}

int main(int argc, const char * argv[])
{
	/* Parse command-line arguments */
	int foreground = 0;
	struct poptOption optionsTable[] = {
		{"foreground", 'f', POPT_ARG_NONE, &foreground, 0, "Run in foreground", NULL},
		POPT_AUTOHELP
		{NULL, '\0', POPT_ARG_NONE, NULL, 0, NULL, NULL}
	};
	poptContext pContext = poptGetContext(PACKAGE_NAME, argc, argv, optionsTable,
										  POPT_CONTEXT_POSIXMEHARDER);
	int poptResult = 0;
	if((poptResult = poptGetNextOpt(pContext)) < -1)
	{
		fprintf(stderr, "%s: %s\n",
				poptBadOption(pContext, POPT_BADOPTION_NOALIAS),
				poptStrerror(poptResult));
		poptFreeContext(pContext);
		return 1;
	}
	poptFreeContext(pContext);

	if(atexit(_on_exit_actions))
	{
		fprintf(stderr, "%s: failed to set atexit function\n", PACKAGE_NAME);
		return 2;
	}

	/* Connect to system logger */
	openlog(PACKAGE_NAME, LOG_PID, LOG_DAEMON);

	/* Daemonize program */
	if(!foreground)
	{
		if(daemon(0, 0))
		{
			syslog(LOG_EMERG, "Fail to daemonize: %s", strerror(errno));
			return 3;
		}
	}

	/* Main actions of program */

	return 0;
}
