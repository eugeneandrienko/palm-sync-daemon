/**
   @author Eugene Andrienko
   @brief Main file of palm-sync-daemon
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <popt.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "log.h"
#include "palm.h"


#define ARGUMENT_BUFFER_SIZE 50                    /** Buffer size for command-line argument */
#define PID_BUFFER_SIZE 10                         /** Buffer size for PID as string */
#define ENV_NOTES_FILE "PALM_SYNC_NOTES_ORG"       /** Environment variable with path to notes org-file */
#define ENV_TODO_FILE "PALM_SYNC_TODO_ORG"         /** Environment variable with path to todo/calendar org-file */
#define LOCK_FILE_PATH "/tmp/" PACKAGE_NAME ".pid" /** Path to lock-file */


/**
   Lock process to not execute another one instance of daemon.

   Creates lock-file in LOCK_FILE_PATH which prevents execution of another
   daemon instance.
   If lock-file already exists - prevent program execution.

   @return 0 is lock-file created, 1 if lock-file exists or some error happened.
*/
static int _lock_process()
{
	char pid[PID_BUFFER_SIZE] = "\0";
	int fd = open(LOCK_FILE_PATH, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if(fd < 0 && errno == EEXIST)
	{
		log_write(LOG_CRIT, "File %s already locked", LOCK_FILE_PATH);
		fd = open(LOCK_FILE_PATH, O_RDONLY, 0);
		if(read(fd, pid, PID_BUFFER_SIZE) == -1)
		{
			log_write(LOG_EMERG, "Failed to read PID of locking process from %s file: %s",
					  LOCK_FILE_PATH, strerror(errno));
			strcpy(pid, "UNKNOWN");
		}
		log_write(LOG_CRIT, "Lock file owned by process with PID %s", pid);
		close(fd);
		return 1;
	}
	else if(fd < 0)
	{
		log_write(LOG_EMERG, "Cannot create lock file %s: %s", LOCK_FILE_PATH, strerror(errno));
		return 1;
	}

	sprintf(pid, "%d", getpid());
	if(write(fd, pid, strlen(pid)) == -1)
	{
		log_write(LOG_EMERG, "Failed to write PID to lock file %s: %s",
				  LOCK_FILE_PATH, strerror(errno));
		close(fd);
		unlink(LOCK_FILE_PATH);
		return 1;
	}

	close(fd);
	return 0;
}

/**
   Unlock process.

   Remove lock-file and unlocks process.
   This function should be called right before program exit (or termination).
*/
void _unlock_process()
{
	unlink(LOCK_FILE_PATH);
}

/**
   Actions, which should be called on program exit.
*/
void _on_exit_actions()
{
	log_write(LOG_INFO, "Closing...");
	log_close();
	_unlock_process();
}

/**
   Read path to file from given environment variable.

   Reads path to file from environment variable. Also checks is file exists and
   process can read and write to it.

   @param env Environment variable name
   @return Path to file or NULL if function failed
*/
static char * _get_file_path(char * env)
{
	char * path = getenv(env);
	if(path == NULL)
	{
		log_write(LOG_EMERG, "%s: no %s environment variable defined", PACKAGE_NAME, env);
		return NULL;
	}
	if(access(path, R_OK | W_OK))
	{
		log_write(LOG_EMERG, "%s: no access to %s file", PACKAGE_NAME, path);
		return NULL;
	}
	return path;
}

volatile static int terminateProgram = 0; /** Flag that shows necessity of program termination */

/**
   Signal handler
*/
void sig_handler(int signum)
{
	terminateProgram = 1;
}

/**
   Setup handler for signals.

   Setup signal handler for SIGINT, SIGQUIT and SIGTERM.

   @return 0 if handler successfully set, 1 if error happened
*/
static int _setup_sig_handler()
{
	struct sigaction sa;
	sigset_t maskedSignals;

	if(sigemptyset(&maskedSignals))
	{
		log_write(LOG_EMERG, "Cannot clear signal set: %s", strerror(errno));
		return 1;
	}
	if(sigaddset(&maskedSignals, SIGINT))
	{
		log_write(LOG_EMERG, "Cannot add SIGINT to signal set: %s", strerror(errno));
		return 1;
	}
	if(sigaddset(&maskedSignals, SIGQUIT))
	{
		log_write(LOG_EMERG, "Cannot add SIGQUIT to signal set: %s", strerror(errno));
		return 1;
	}
	if(sigaddset(&maskedSignals, SIGTERM))
	{
		log_write(LOG_EMERG, "Cannot add SIGTERM to signal set: %s", strerror(errno));
		return 1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sa.sa_mask = maskedSignals;
	if(sigaction(SIGINT, &sa, NULL))
	{
		log_write(LOG_EMERG, "Cannot set signal handler for SIGINT: %s", strerror(errno));
		return 1;
	}
	if(sigaction(SIGQUIT, &sa, NULL))
	{
		log_write(LOG_EMERG, "Cannot set signal handler for SIGQUIT: %s", strerror(errno));
		return 1;
	}
	if(sigaction(SIGTERM, &sa, NULL))
	{
		log_write(LOG_EMERG, "Cannot set signal handler for SIGTERM: %s", strerror(errno));
		return 1;
	}

	return 0;
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

	log_init(foreground);

	/* Daemonize program */
	if(!foreground)
	{
		if(daemon(0, 0))
		{
			log_write(LOG_EMERG, "Fail to daemonize: %s", strerror(errno));
			return 1;
		}
	}

	if(_lock_process())
	{
		log_close();
		return 1;
	}

	if(_setup_sig_handler())
	{
		log_close();
		return 1;
	}

	if(atexit(_on_exit_actions))
	{
		log_write(LOG_EMERG, "%s: failed to set atexit function\n", PACKAGE_NAME);
		log_close();
		return 1;
	}

	/* Read necessary environment variables */
	char * notesFile;
	char * todoFile;
	if((notesFile = _get_file_path(ENV_NOTES_FILE)) == NULL ||
	   (todoFile = _get_file_path(ENV_TODO_FILE)) == NULL)
	{
		return 1;
	}

	/* Main program actions */
	log_write(LOG_INFO, "%s started successfully", PACKAGE_NAME);
	log_write(LOG_DEBUG, "Device: %s", palmDeviceFile);
	log_write(LOG_DEBUG, "Path to notes org-file: %s", notesFile);
	log_write(LOG_DEBUG, "Path to todo and calendar org-file: %s", todoFile);

	/* First call of palm_device_test to catch possible errors from libraries */
	if(palm_device_test(palmDeviceFile, 1))
	{
		log_write(LOG_NOTICE, "Will wait for %s connection in infinite cycle...", palmDeviceFile);
	}

	while(1)
	{
		if(terminateProgram)
		{
			exit(0);
		}
		if(palm_device_test(palmDeviceFile, 0))
		{
			sleep(1);
			continue;
		}

		sleep(1);
	}

	return 0;
}
