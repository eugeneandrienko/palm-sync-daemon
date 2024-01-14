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

#define ARGUMENT_BUFFER_SIZE 50
#define PID_BUFFER_SIZE 10
#define ENV_NOTES_FILE "PALM_SYNC_NOTES_ORG"
#define ENV_TODO_FILE "PALM_SYNC_TODO_ORG"
#define LOCK_FILE_PATH "/tmp/" PACKAGE_NAME ".pid"


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
		log_write(LOG_CRIT, "Lock file owned by process with PID %s\n", pid);
		close(fd);
		return 1;
	}
	else if(fd < 0)
	{
		log_write(LOG_EMERG, "Cannot create lock file %s: %s\n", LOCK_FILE_PATH, strerror(errno));
		return 1;
	}

	sprintf(pid, "%d", getpid());
	if(write(fd, pid, strlen(pid)) == -1)
	{
		log_write(LOG_EMERG, "Failed to write PID to lock file %s: %s\n",
				  LOCK_FILE_PATH, strerror(errno));
		close(fd);
		unlink(LOCK_FILE_PATH);
		return 1;
	}

	close(fd);
	return 0;
}

void _unlock_process()
{
	unlink(LOCK_FILE_PATH);
}

void _on_exit_actions()
{
	log_write(LOG_INFO, "Closing...");
	log_close();
	_unlock_process();
}

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

volatile static int terminateProgram = 0;

void sig_handler(int signum)
{
	terminateProgram = 1;
}

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
			return 1;
		}
	}

	log_init(foreground);

	if(_lock_process())
	{
		log_close();
		return 1;
	}

	if(_setup_sig_handler())
	{
		return 1;
	}

	if(atexit(_on_exit_actions))
	{
		log_write(LOG_EMERG, "%s: failed to set atexit function\n", PACKAGE_NAME);
		return 1;
	}

	/* Main actions of program */
	log_write(LOG_INFO, "%s started successfully", PACKAGE_NAME);
	log_write(LOG_DEBUG, "Device: %s", palmDeviceFile);
	log_write(LOG_DEBUG, "Path to notes org-file: %s", notesFile);
	log_write(LOG_DEBUG, "Path to todo and calendar org-file: %s", todoFile);

	while(1)
	{
		if(terminateProgram)
		{
			exit(0);
		}
		sleep(1);
	}

	return 0;
}
