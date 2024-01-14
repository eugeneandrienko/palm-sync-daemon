/**
   @author Eugene Andrienko
   @brief Main file of palm-sync-daemon
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <popt.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "log.h"

#define ENV_NOTES_FILE "PALM_SYNC_NOTES_ORG"
#define ENV_TODO_FILE "PALM_SYNC_TODO_ORG"

void _on_exit_actions()
{
	log_write(LOG_INFO, "Closing...");
	log_close();
static char * _get_orgfile_path(char * env)
{
	char * path = getenv(env);
	if(path == NULL)
	{
		fprintf(stderr, "%s: no %s environment variable defined\n", PACKAGE_NAME, env);
		return NULL;
	}
	if(access(path, F_OK | R_OK | W_OK))
	{
		fprintf(stderr, "%s: no access to %s file\n", PACKAGE_NAME, path);
		return NULL;
	}
	return path;
}

int main(int argc, const char * argv[])
{
	/* Parse command-line arguments */
	int foreground = 0;
	char palmDeviceFile[ARGUMENT_BUFFER_SIZE] = "/dev/ttyUSB1";
	struct poptOption optionsTable[] = {
		{"foreground", 'f', POPT_ARG_NONE, &foreground, 0, "Run in foreground", NULL},
		{"device", 'd', POPT_ARG_STRING, palmDeviceFile, 0, "Palm device to connect", "DEVICE"},
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

	/* Read necessary environment variables */
	char * notesFile;
	char * todoFile;
	if((notesFile = _get_orgfile_path(ENV_NOTES_FILE)) == NULL ||
	   (todoFile = _get_orgfile_path(ENV_TODO_FILE)) == NULL)
	{
		return 1;
	}

	/* Daemonize program */
	if(!foreground)
	{
		if(daemon(0, 0))
		{
			log_write(LOG_EMERG, "Fail to daemonize: %s", strerror(errno));
			return 3;
		}
	}

	/* Main actions of program */
	log_write(LOG_INFO, "%s started successfully", PACKAGE_NAME);

	return 0;
}
