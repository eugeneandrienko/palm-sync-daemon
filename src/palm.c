#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libpisock/pi-dlp.h>
#include <libpisock/pi-file.h>
#include <libpisock/pi-socket.h>
#include "log.h"
#include "palm.h"
#include "pdb.h"

#define PALM_PDB_FNAME_BUFFER_LEN 128 /* Maximal length for PDB filename */
#define PALM_PDB_TMP_DIR "/tmp"       /* Directory to store temporary PDB files */
#define PALM_SYNCLOG_ENTRY_LEN 512    /* Maximal length for synclog string */
#define PALM_CLOSE_WAIT_SEC 5         /* Seconds to wait while device disappering after close */
#define PALM_CANNOT_BIND_MAX_ERRORS 3 /* Count of sequental logged errors from pi_bind */

static void _palm_log_system_info(struct SysInfo * info);
static void _palm_read_database(int sd, const char * dbname, char ** path);
static void _palm_write_database(int sd, const char * dbname, const char * path);

static unsigned char cannotBindErrorsCount = 0;

int palm_open(char * device)
{
	int sd = -1;
	int result = 0;

	if((sd = pi_socket(PI_AF_PILOT, PI_SOCK_STREAM, PI_PF_DLP)) < 0)
	{
		log_write(LOG_WARNING, "Cannot create socket for Palm: %s", strerror(errno));
		return -1;
	}

	if((result = pi_bind(sd, device)) < 0)
	{
		if(cannotBindErrorsCount < PALM_CANNOT_BIND_MAX_ERRORS)
		{
			log_write(LOG_DEBUG, "Cannot bind %s", device);
			if(result == PI_ERR_SOCK_INVALID)
			{
				log_write(LOG_ERR, "Socket is invalid for %s", device);
			}
			cannotBindErrorsCount++;
		}
		if(result != PI_ERR_SOCK_INVALID)
		{
			pi_close(sd);
		}
		return -1;
	}
	cannotBindErrorsCount = 0;

	if(pi_listen(sd, 1) < 0)
	{
		log_write(LOG_ERR, "Cannot listen %s", device);
		pi_close(sd);
		return -1;
	}

	result = pi_accept_to(sd, 0, 0, 0);
	if(result < 0)
	{
		log_write(LOG_ERR, "Cannot accept data on %s", device);
		pi_close(sd);
		return -1;
	}
	sd = result;

	struct SysInfo sysInfo;
	if(dlp_ReadSysInfo(sd, &sysInfo) < 0)
	{
		log_write(LOG_ERR, "Cannot read system info from Palm on %s", device);
		pi_close(sd);
		return -1;
	}
	_palm_log_system_info(&sysInfo);

	if(dlp_OpenConduit(sd) < 0)
	{
		log_write(LOG_ERR, "Cannot open conduit");
		pi_close(sd);
		return -1;
	}

	return sd;
}

PalmData * palm_read(int sd)
{
	if(sd < 0)
	{
		log_write(LOG_ERR, "Wrong Palm descriptor: %d", sd);
		return NULL;
	}
	PalmData * data;
	if((data = calloc(1, sizeof(PalmData))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for PalmData structure: %s",
				  strerror(errno));
		return NULL;
	}

	_palm_read_database(sd, "DatebookDB", &data->datebookDBPath);
	_palm_read_database(sd, "MemoDB",     &data->memoDBPath);
	_palm_read_database(sd, "ToDoDB",     &data->todoDBPath);

	return 0;
}

int palm_write(int sd, PalmData * data)
{
	if(sd < 0)
	{
		log_write(LOG_ERR, "Wrong Palm descriptor: %d", sd);
		return -1;
	}
	if(data == NULL)
	{
		log_write(LOG_ERR, "Uninitialized PalmData structure");
		return -1;
	}
	if(data->datebookDBPath == NULL ||
	   data->memoDBPath == NULL ||
	   data->todoDBPath == NULL)
	{
		log_write(LOG_ERR, "Empty PalmData structure");
		return -1;
	}

	_palm_write_database(sd, "DatebookDB", data->datebookDBPath);
	_palm_write_database(sd, "MemoDB", data->memoDBPath);
	_palm_write_database(sd, "ToDoDB", data->todoDBPath);

	return 0;
}

int palm_close(int sd, char * device)
{
	pi_close(sd);

	/* Wait while device disconnects */
	int secondsToWait = PALM_CLOSE_WAIT_SEC;
	while((secondsToWait > 0) && (access(device, F_OK) == 0))
	{
		log_write(LOG_DEBUG, "Waiting for %s to disappear...", device);
		sleep(1);
		secondsToWait--;
	}

	if(secondsToWait == 0)
	{
		log_write(LOG_CRIT, "Timeout when waiting %s to disappear from system", device);
		return -1;
	}
	else
	{
		return 0;
	}
}

void palm_free(PalmData * data)
{
	if(data->datebookDBPath == NULL &&
	   data->memoDBPath == NULL &&
	   data->todoDBPath == NULL)
	{
		free(data);
		// Nothing to clean.
		return;
	}

 	if(data->datebookDBPath != NULL)
	{
		if(unlink(data->datebookDBPath))
		{
			log_write(LOG_ERR, "Cannot delete %s: %s", data->datebookDBPath,
					  strerror(errno));
		}
		free(data->datebookDBPath);
		data->datebookDBPath = NULL;
	}
	if(data->memoDBPath != NULL)
	{
		if(unlink(data->memoDBPath))
		{
			log_write(LOG_ERR, "Cannot delete %s: %s", data->memoDBPath,
					  strerror(errno));
		}
		free(data->memoDBPath);
		data->memoDBPath = NULL;
	}
	if(data->todoDBPath != NULL)
	{
		if(unlink(data->todoDBPath))
		{
			log_write(LOG_ERR, "Cannot delete %s: %s", data->todoDBPath,
					  strerror(errno));
		}
		free(data->todoDBPath);
		data->todoDBPath = NULL;
	}

	free(data);
}

/**
   Prints Palm system info.

   @param[in] info System info from Palm device.
*/
static void _palm_log_system_info(struct SysInfo * info)
{
	log_write(LOG_DEBUG, "Device ROM version: major=%d, minor=%d, fix=%d, stage=%d, build=%d",
			  (info->romVersion >> 32) & 0x00000000ff,
			  (info->romVersion >> 24) & 0x00000000ff,
			  (info->romVersion >> 16) & 0x00000000ff,
			  (info->romVersion >> 8)  & 0x00000000ff,
			  (info->romVersion)       & 0x00000000ff);
	log_write(LOG_DEBUG, "DLP protocol: %d.%d", info->dlpMajorVersion, info->dlpMinorVersion);
	log_write(LOG_DEBUG, "Compatible DLP protocol: %d.%d",
			  info->compatMajorVersion,
			  info->compatMinorVersion);
}

/**
   Read database from Palm to temporary file.

   @param[in] sd Palm device descriptor.
   @param[in] dbname Name of database to fetch.
   @param[out] path Path to temporary PDB-file where Palm DB is saved.
   @return Void.
*/
static void _palm_read_database(int sd, const char * dbname, char ** path)
{
	struct DBInfo info;
	struct pi_file * f;

	if(strlen(dbname) > PDB_DBNAME_LEN - 1)
	{
		log_write(LOG_ERR, "Given Palm DB name (%s) has more than %s characters!",
				  dbname, PDB_DBNAME_LEN - 1);
		log_write(LOG_ERR, "Cannot read %s database", dbname);
		return;
	}

	if(dlp_FindDBInfo(sd, 0, 0, dbname, 0, 0, &info) < 0)
	{
		log_write(LOG_ERR, "Unable to locate database %s on the Palm", dbname);
		return;
	}

	/* Some magic from pilot-link/src/pilot-xfer.c:682 */
	info.flags &= 0x2fd;

	*path = calloc(PALM_PDB_FNAME_BUFFER_LEN, sizeof(char));
	if(*path == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for path to PDB file for %s", dbname);
		return;
	}
	pid_t pid = getpid();
	sprintf(*path, PALM_PDB_TMP_DIR "/%s.%d.pdb\0", dbname, pid);

	f = pi_file_create(*path, &info);
	if(f == 0)
	{
		log_write(LOG_ERR, "Unable to create file %s", *path);
		free(*path);
		*path = NULL;
		return;
	}

	if(pi_file_retrieve(f, sd, 0, NULL) < 0)
	{
		log_write(LOG_ERR, "Unable to fetch database %s from Palm to %s", dbname, *path);
		pi_file_close(f);
		unlink(*path);
		free(*path);
		*path = NULL;
		return;
	}
	log_write(LOG_INFO, "Read %s to %s", dbname, *path);

	char synclog[PALM_SYNCLOG_ENTRY_LEN];
	snprintf(synclog, sizeof(synclog) - 1, "Read %s to PC\n", dbname);
	dlp_AddSyncLogEntry(sd, synclog);
	pi_file_close(f);
}

/**
   Write Palm database from given file to Palm device.

   @param[in] sd Palm device descriptor.
   @param[in] dbname Database name to write.
   @param[in] path Path to PDB file with database data.
   @return Void.
*/
static void _palm_write_database(int sd, const char * dbname, const char * path)
{
	if(strlen(dbname) > PDB_DBNAME_LEN - 1)
	{
		log_write(LOG_ERR, "Given Palm DB name (%s) has more than %s characters!",
				  dbname, PDB_DBNAME_LEN - 1);
		log_write(LOG_ERR, "Cannot write data to %s database", dbname);
		return;
	}

	struct stat sbuf;
	if(stat(path, &sbuf))
	{
		log_write(LOG_ERR, "Cannot stat %s file: %s", path, strerror(errno));
		return;
	}

	struct pi_file * f;
	const char * basename = strrchr(path, '/');
	f = pi_file_open(path);
	if(f == NULL)
	{
		log_write(LOG_ERR, "Cannot open %s to write to Palm device", path);
		return;
	}
	if(f->file_name == NULL)
	{
		f->file_name = strdup(basename);
	}

	struct CardInfo card;
	card.card = -1;
	card.more = 1;

	while(card.more)
	{
		if(dlp_ReadStorageInfo(sd, card.card + 1, &card) < 0)
		{
			break;
		}
	}

	if((unsigned long)sbuf.st_size > card.ramFree)
	{
		log_write(LOG_ERR, "Insufficient space on Palm device to install file %s", path);
		log_write(LOG_ERR, "We need %lu and have only %lu available",
				  (unsigned long)sbuf.st_size, card.ramFree);
		pi_file_close(f);
		return;
	}

	if(pi_file_install(f, sd, 0, NULL) < 0)
	{
		log_write(LOG_ERR, "Cannot install %s file to Palm (%d, PalmOS 0x%04x)",
				  path, pi_error(sd), pi_palmos_error(sd));
		pi_file_close(f);
		return;
	}

	char synclog[PALM_SYNCLOG_ENTRY_LEN];
	snprintf(synclog, sizeof(synclog) - 1, "Write %s (%ld bytes) from PC\n", dbname, sbuf.st_size);
	dlp_AddSyncLogEntry(sd, synclog);
	pi_file_close(f);
	log_write(LOG_INFO, "Write %s from %s (%ld bytes)", dbname, path, sbuf.st_size);
}
