/**
   @author Eugene Andrienko
   @brief Main palm-sync-daemon module.
   @file palm-sync-daemon.c

   This module realize parsing of command-line options, reading configuration
   and other Unix-process-specific actions. After that main loop which
   synchronize Palm with PC will start.
*/

/**
   @mainpage palm-sync-daemon

   Main module of palm-sync-daemon.

   This daemon intended to synchornize between Palm PDA and files on PC when the
   user press sync button on the cable or cradle.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <popt.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>
#include "config.h"
#include "log.h"
#include "sync.h"


/**
   Buffer size for PID as string
*/
#define PID_BUFFER_SIZE 10
/**
   Environment variable with path to notes org-file
*/
#define ENV_NOTES_FILE "PALM_SYNC_NOTES_ORG"
/**
   Environment variable with path to todo/calendar org-file
*/
#define ENV_TODO_FILE "PALM_SYNC_TODO_ORG"
/**
   Path to lock-file
*/
#define LOCK_FILE_PATH "/tmp/" PACKAGE_NAME ".pid"


static int _process_init(int foreground);
static int _process_lock();
static void _process_unlock();
static void _process_on_exit_actions();
static void _process_sig_handler(int signum);
static int _process_setup_sig_handler();
static volatile int _processTerminate = 0; /* Flag shows necessity of
											  program termination */


/**
   Read path to file from given environment variable.

   Reads path to file from environment variable. Also checks is file exists and
   readable/writeable.

   @param env[in] Environment variable name.
   @return Path to file or NULL if failed.
*/
static char * _get_file_path(char * env)
{
	char * path = getenv(env);
	if(path == NULL)
	{
		log_write(LOG_EMERG, "%s: no %s environment variable defined",
				  PACKAGE_NAME, env);
		return NULL;
	}
	if(access(path, R_OK | W_OK))
	{
		log_write(LOG_EMERG, "%s: no access to %s file", PACKAGE_NAME, path);
		return NULL;
	}
	return path;
}

/**
   Checks given data directory path.

   Given directory should be exists and should has read, write and
   search permissions for current user.

   @param[in] dataDir path to directory with application data.
   @return Zero on successfull check, otherwise non-zero.
*/
int _check_data_directory(char * dataDir)
{
	if(dataDir == NULL)
	{
		log_write(LOG_EMERG, "Data directory is not specified and is NULL");
		return -1;
	}
	if(access(dataDir, R_OK | W_OK | X_OK))
	{
		if(errno == ENOENT)
		{
			if(mkdir(dataDir, S_IRWXU))
			{
				log_write(LOG_EMERG, "Cannot create %s data directory: %s",
						  dataDir, strerror(errno));
				return -1;
			}
			log_write(LOG_NOTICE, "Created %s directory", dataDir);
			return 0;
		}
		log_write(LOG_EMERG, "No read, write or execute permission "
				  "for %s catalog", dataDir);
		return -1;
	}
	return 0;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
int main(int argc, const char * argv[])
{
	SyncSettings syncSettings = {
		.device = "/dev/ttyUSB1",
		.notesOrgFile = NULL,
		.todoOrgFile = NULL,
		.dryRun = 0,
		.dataDir = "~/.palm-sync-daemon/",
		.prevDatebookPDB = NULL,
		.prevMemosPDB = NULL,
		.prevTodoPDB = NULL,
		.prevTasksPDB = NULL
	};
	/* Parse command-line arguments */
	int foreground = 0;
	int debug = 0;
	struct poptOption optionsTable[] = {
		{
			"data-dir",
			't',
			POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
			&syncSettings.dataDir,
			0,
			"Data directory",
			"DIRECTORY"
		},
		{
			"foreground",
			'f',
			POPT_ARG_NONE,
			&foreground,
			0,
			"Run in foreground",
			NULL
		},
		{
			"debug",
			'\0',
			POPT_ARG_NONE,
			&debug,
			0,
			"Log debug messages",
			NULL
		},
		{
			"dry-run",
			'\0',
			POPT_ARG_NONE,
			&syncSettings.dryRun,
			0,
			"Dry run, without real sync",
			NULL
		},
		{
			"device",
			'd',
			POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
			&syncSettings.device,
			0,
			"Palm device to connect",
			"DEVICE"
		},
		POPT_AUTOHELP
		POPT_TABLEEND
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

	log_init(foreground, debug);
	if(_process_init(foreground))
	{
		return 1;
	}

	/* Unexpand ~ in directory path and add trailing slash if not exists */
	wordexp_t we;
	if(wordexp(syncSettings.dataDir, &we, WRDE_NOCMD | WRDE_UNDEF))
	{
		log_write(LOG_EMERG, "Cannot expand %s string", syncSettings.dataDir);
		return 1;
	}
	unsigned int expandedDataDirLen = 0;
	for(size_t i = 0; i < we.we_wordc; i++)
	{
		expandedDataDirLen += strlen(we.we_wordv[i]);
	}
	if((syncSettings.dataDir = calloc(
			expandedDataDirLen + we.we_wordc + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_EMERG, "Cannot allocate memory for string "
				  "with data directory");
		return 1;
	}
	char * expandedDataDir = syncSettings.dataDir;
	for(size_t i = 0; i < we.we_wordc; i++)
	{
		strncpy(expandedDataDir, we.we_wordv[i], strlen(we.we_wordv[i]));
		expandedDataDir += strlen(we.we_wordv[i]);
		*expandedDataDir = ' ';
		expandedDataDir++;
	}
	if(*(expandedDataDir - 2) != '/')
	{
		*(--expandedDataDir) = '/';
		*(++expandedDataDir) = '\0';
	}
	else
	{
		*(--expandedDataDir) = '\0';
	}
	wordfree(&we);
	log_write(LOG_DEBUG, "Expanded data directory string: %s",
			  syncSettings.dataDir);

	if(_check_data_directory(syncSettings.dataDir))
	{
		return 1;
	}

	/* Read necessary environment variables */
	if((syncSettings.notesOrgFile = _get_file_path(ENV_NOTES_FILE)) == NULL ||
	   (syncSettings.todoOrgFile = _get_file_path(ENV_TODO_FILE)) == NULL)
	{
		return 1;
	}

	/* Main program actions */
	log_write(LOG_INFO, "%s started successfully", PACKAGE_NAME);
	log_write(LOG_DEBUG, "Device: %s", syncSettings.device);
	log_write(LOG_DEBUG, "Path to notes org-file: %s",
			  syncSettings.notesOrgFile);
	log_write(LOG_DEBUG, "Path to todo and calendar org-file: %s",
			  syncSettings.todoOrgFile);
	log_write(LOG_DEBUG, "Data directory: %s", syncSettings.dataDir);
	if(syncSettings.dryRun)
	{
		log_write(LOG_DEBUG, "--dry-run is enabled. No real sync will be done!");
	}

	while(1)
	{
		if(_processTerminate)
		{
			exit(0);
		}

		int syncResult = sync_this(&syncSettings);
		if(syncResult == PALM_NOT_CONNECTED)
		{
			sleep(1);
			continue;
		}
		else if(syncResult)
		{
			log_write(LOG_ERR, "Cannot synchronize Palm PDA with PC!");
			sleep(1);
			continue;
		}

		sleep(1);
	} /* while(1) */

	return 0;
}
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
   Process initialization.

   This function daemonize process (if necessary), acquire lockfile, setup
   signal handlers and set function to call at process exit.

   @param foreground Set to 1 if program should run in foreground,
   set to 0 if we need a daemon.
   @return Returns 0 if process successfully initialized, otherwise -1.
*/
static int _process_init(int foreground)
{
	/* Daemonize program */
	if(!foreground)
	{
		if(daemon(0, 0))
		{
			log_write(LOG_EMERG, "Fail to daemonize: %s", strerror(errno));
			return -1;
		}
	}

	/* Process initialization */
	if(_process_lock())
	{
		log_close();
		return -1;
	}
	if(_process_setup_sig_handler())
	{
		log_close();
		return -1;
	}
	if(atexit(_process_on_exit_actions))
	{
		log_write(LOG_EMERG, "%s: failed to set atexit function", PACKAGE_NAME);
		log_close();
		return -1;
	}

	return 0;
}

/**
   Create lockfile for process.

   Creates lock-file in LOCK_FILE_PATH which prevents execution of another
   processd instance. If lock-file already exists — returns an error.

   @return Returns 0 is lockfile successfully created. Returns 1 if lock-file
   exists or error happened.
*/
static int _process_lock()
{
	char pid[PID_BUFFER_SIZE] = "\0";
	int fd = open(LOCK_FILE_PATH, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if(fd < 0 && errno == EEXIST)
	{
		log_write(LOG_CRIT, "File %s already locked", LOCK_FILE_PATH);
		fd = open(LOCK_FILE_PATH, O_RDONLY, 0);
		if(read(fd, pid, PID_BUFFER_SIZE) == -1)
		{
			log_write(LOG_EMERG, "Failed to read PID of locking process "
					  "from %s file: %s", LOCK_FILE_PATH, strerror(errno));
			strcpy(pid, "UNKNOWN");
		}
		log_write(LOG_CRIT, "Lock file owned by process with PID %s", pid);
		close(fd);
		return 1;
	}
	else if(fd < 0)
	{
		log_write(LOG_EMERG, "Cannot create lock file %s: %s", LOCK_FILE_PATH,
				  strerror(errno));
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
   Remove process lockfile.

   Remove lockfile and unlocks process.

   This function should be called right before program exit (or termination).

   @return Void.
*/
static void _process_unlock()
{
	unlink(LOCK_FILE_PATH);
}

/**
   Function to call on program exit.

   @return Void.
*/
static void _process_on_exit_actions()
{
	log_write(LOG_INFO, "Closing...");
	log_close();
	_process_unlock();
}

/**
   Signal handler.

   Set special flag, which shows necessity of program termination.

   @param[in] signum Signal number.
   @return Void.
*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void _process_sig_handler(int signum)
{
	_processTerminate = 1;
}
#pragma GCC diagnostic pop

/**
   Setup handler for signals.

   Setup signal handler for SIGINT, SIGQUIT and SIGTERM.

   @return Returns 0 if handler successfully set, returns 1 if error happened.
*/
static int _process_setup_sig_handler()
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
		log_write(LOG_EMERG, "Cannot add SIGINT to signal set: %s",
				  strerror(errno));
		return 1;
	}
	if(sigaddset(&maskedSignals, SIGQUIT))
	{
		log_write(LOG_EMERG, "Cannot add SIGQUIT to signal set: %s",
				  strerror(errno));
		return 1;
	}
	if(sigaddset(&maskedSignals, SIGTERM))
	{
		log_write(LOG_EMERG, "Cannot add SIGTERM to signal set: %s",
				  strerror(errno));
		return 1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _process_sig_handler;
	sa.sa_mask = maskedSignals;
	if(sigaction(SIGINT, &sa, NULL))
	{
		log_write(LOG_EMERG, "Cannot set signal handler for SIGINT: %s",
				  strerror(errno));
		return 1;
	}
	if(sigaction(SIGQUIT, &sa, NULL))
	{
		log_write(LOG_EMERG, "Cannot set signal handler for SIGQUIT: %s",
				  strerror(errno));
		return 1;
	}
	if(sigaction(SIGTERM, &sa, NULL))
	{
		log_write(LOG_EMERG, "Cannot set signal handler for SIGTERM: %s",
				  strerror(errno));
		return 1;
	}

	return 0;
}
