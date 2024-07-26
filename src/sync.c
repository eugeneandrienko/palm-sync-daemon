#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"
#include "palm.h"
#include "pdb_memos.h"
#include "org_notes.h"
#include "sync.h"


/**
   Filename of Datebook PDB file from previous iteration.
*/
#define PREV_DATEBOOK_PDB "previousDatebook.pdb"
/**
   Filename of Memos PDB file from previous iteration.
*/
#define PREV_MEMOS_PDB "previousMemos.pdb"
/**
   Filename of TODO PDB file from previous iteration.
*/
#define PREV_TODO_PDB "previousTodo.pdb"

/**
   Max length of PDB filepath for file from previous iteration.
*/
#define MAX_PATH_LEN 300

/**
   Copy buffer length
*/
#define COPY_BUFFER_LENGTH 4096

static int _check_previous_pdbs(SyncSettings * syncSettings);
static int _sync_memos(char * pdbPath, char * prevPdbPath, char * orgPath, int dryRun);
static int _compute_record_statuses(PDB * pdb, char * prevPdbPath);
static int _save_as_previous_pdbs(SyncSettings * syncSettings, PalmData * palmData);


int sync_this(SyncSettings * syncSettings)
{
	int palmfd = 0;
	if((palmfd = palm_open(syncSettings->device)) == -1)
	{
		return PALM_NOT_CONNECTED;
	}

	if(_check_previous_pdbs(syncSettings))
	{
		log_write(LOG_ERR, "Failed to check PDB files from previous iteration");
		return -1;
	}

	PalmData * palmData;
	if((palmData = palm_read(palmfd)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read PDBs from Palm");
		if(palm_close(palmfd, syncSettings->device))
		{
			log_write(LOG_ERR, "Failed to close Palm device");
		}
		return -1;
	}

	if(_sync_memos(palmData->memoDBPath, syncSettings->prevMemosPDB,
				   syncSettings->notesOrgFile, syncSettings->dryRun))
	{
		log_write(LOG_ERR, "Failed to synchronize Memos");
		goto sync_this_error;
	}

	if(!syncSettings->dryRun && palm_write(palmfd, palmData))
	{
		log_write(LOG_ERR, "Failed to write PDB files to Palm");
		goto sync_this_error;
	}

	if(!syncSettings->dryRun && _save_as_previous_pdbs(syncSettings, palmData))
	{
		log_write(LOG_ERR, "Failed to save PDB files as files from previous iteration");
		goto sync_this_error;
	}

	palm_free(palmData);
	if(palm_close(palmfd, syncSettings->device))
	{
		return -1;
	}
	return 0;
sync_this_error:
	palm_free(palmData);
	if(palm_close(palmfd, syncSettings->device))
	{
		log_write(LOG_ERR, "Failed to close Palm device");
	}
	return -1;
}

static int __check_previous_pdb(char * dataDir, const char * pdbFileName, char ** result);

/**
   Check for PDB files from previous synchronization iteration.

   Function check for PDB files existence and write paths to it in syncSettings
   structure. Or NULL if file not found.

   @param[in] syncSettings Synchronization settings structure with path to data
   directory inside.
   @return Zero on success, non-zero value if some of system calls are failed.
*/
int _check_previous_pdbs(SyncSettings * syncSettings)
{
	/* Check for Datebook PDB file from previous iteration */
	if(__check_previous_pdb(syncSettings->dataDir, PREV_DATEBOOK_PDB,
							&syncSettings->prevDatebookPDB))
	{
		return -1;
	}
	/* Check for Memos PDB file from previous iteration */
	if(__check_previous_pdb(syncSettings->dataDir, PREV_MEMOS_PDB,
							&syncSettings->prevMemosPDB))
	{
		return -1;
	}
	/* Check for TODO PDB file from previous iteration */
	if(__check_previous_pdb(syncSettings->dataDir, PREV_TODO_PDB,
							&syncSettings->prevTodoPDB))
	{
		return -1;
	}
	return 0;
}

/**
   Function check one of the PDB file from previous synchronization iteration.

   Function checks existence of file and copy corresponding filepath
   to result variable if file exists and accessible. Or if file not exists NULL
   will be set as result.

   If file exists and not accessible, the error will be returned.

   @param[in] dataDir path to data directory.
   @param[in] pdbFileName name of PDB file to check
   @param[out] result Pointer to string with resulting path.
   @return Zero value on success, otherwise non-zero value will be returned.
*/
int __check_previous_pdb(char * dataDir, const char * pdbFileName, char ** result)
{
	char prevPDBPath[MAX_PATH_LEN] = "\0";

	strncpy(prevPDBPath, dataDir, strlen(dataDir));
	strcat(prevPDBPath, pdbFileName);

	if(access(prevPDBPath, F_OK))
	{
		log_write(LOG_DEBUG, "PDB file %s from previous sync cycle not found",
				  prevPDBPath);
		*result = NULL;
		return 0;
	}
	if(access(prevPDBPath, R_OK | W_OK))
	{
		log_write(LOG_WARNING, "No access to PDB file from previous iteration: %s",
				  prevPDBPath);
		return -1;
	}

	if((*result = calloc(strlen(prevPDBPath) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for %s file path",
				  pdbFileName);
		return -1;
	}
	strncpy(*result, prevPDBPath, strlen(prevPDBPath));
	log_write(LOG_DEBUG, "Found PDB file %s from previous sync cycle",
			  *result);
	return 0;
}

/**
   Synchonize Memos data and OrgMode notes file.

   @param[in] pdbPath Path to temporary PDB file from Palm PDA.
   @param[in] prevPdbPath Path to PDB file from previous synchronization cycle.
   @param[in] orgPath Path to OrgMode file with notes.
   @param[in] dryRun If non-zero - do not sync data, just simulate process.
   @return Zero on sucessfull or non-zero on error.
*/
int _sync_memos(char * pdbPath, char * prevPdbPath, char * orgPath, int dryRun)
{
	/* Read memos from PDB file */
	PDB * pdb;
	if((pdb = pdb_memos_read(pdbPath)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read MemosDB");
		return -1;
	}
	if(_compute_record_statuses(pdb, prevPdbPath))
	{
		log_write(LOG_ERR, "Cannot compute statuses for records from %s", pdbPath);
		return -1;
	}

	/* Read notes from OrgMode file */
	OrgNotes * notes;
	if((notes = org_notes_parse(orgPath)) == NULL)
	{
		log_write(LOG_ERR, "Failed to parse file with notes: %s", orgPath);
		return -1;
	}

	/* Compare and sync notes */
	// Notes in Palm handheld and in org file have (almost) the same order.
	// So it is unnecessary to sort it to perform search.
	return 0;
}

/**
   Compute status of each record in PDB structure.

   @param[in] pdb
   @param[in] prevPdbPath
   @return Zero on success or non-zero value on error.
*/
int _compute_record_statuses(PDB * pdb, char * prevPdbPath)
{
	int prevFd;
	PDB * prevPdb;
	PDBRecord * record;
	if((prevFd = pdb_read(prevPdbPath, 1, &prevPdb)) == -1)
	{
		log_write(LOG_WARNING, "Cannot open %s file as PDB from previous synchronization",
				  prevPdbPath);
		log_write(LOG_NOTICE, "Set all records statuses to ADDED");
		TAILQ_FOREACH(record, &pdb->records, pointers)
		{
			record->recordStatus = RECORD_ADDED;
			log_write(LOG_DEBUG, "Record %02x%02x%02x: %d", record->id[2],
					  record->id[1], record->id[0], record->recordStatus);
		}
		return 0;
	}

	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		const uint8_t attribute = record->attributes & 0xf0;
		if(attribute & PDB_RECORD_ATTR_SECRET ||
		   attribute & PDB_RECORD_ATTR_LOCKED)
		{
			record->recordStatus = RECORD_NO_RECORD;
			continue;
		}

		PDBRecord * prevRecord;
		char matchedPrevRecordFound = 0;
		TAILQ_FOREACH(prevRecord, &prevPdb->records, pointers)
		{
			if(record->id[0] != prevRecord->id[0] ||
			   record->id[1] != prevRecord->id[1] ||
			   record->id[2] != prevRecord->id[2])
			{
				continue;
			}
			matchedPrevRecordFound = 1;
			const uint8_t prevAttribute = prevRecord->attributes & 0x0f;
			switch(prevAttribute)
			{
			case PDB_RECORD_ATTR_DELETED:
				switch(attribute)
				{
				case PDB_RECORD_ATTR_DELETED:
					record->recordStatus = RECORD_DELETED;
					break;
				case PDB_RECORD_ATTR_DIRTY:
					record->recordStatus = RECORD_ADDED;
					break;
				default:
					record->recordStatus = RECORD_ADDED;
				}
				break;
			case PDB_RECORD_ATTR_DIRTY:
				switch(attribute)
				{
				case PDB_RECORD_ATTR_DELETED:
					record->recordStatus = RECORD_DELETED;
					break;
				case PDB_RECORD_ATTR_DIRTY:
					record->recordStatus = RECORD_CHANGED;
					break;
				default:
					record->recordStatus = RECORD_NOT_CHANGED;
				}
				break;
			default: /* Record just exists */
				switch(attribute)
				{
				case PDB_RECORD_ATTR_DELETED:
					record->recordStatus = RECORD_DELETED;
					break;
				case PDB_RECORD_ATTR_DIRTY:
					record->recordStatus = RECORD_CHANGED;
					break;
				default:
					record->recordStatus = RECORD_NOT_CHANGED;
				}
			}
		}

		if(!matchedPrevRecordFound)
		{
			record->recordStatus = attribute & PDB_RECORD_ATTR_DELETED ?
				RECORD_NO_RECORD :
				RECORD_ADDED;
		}

		log_write(LOG_DEBUG, "Record %02x%02x%02x: %d", record->id[2],
				  record->id[1], record->id[0], record->recordStatus);
	} /* TAILQ_FOREACH(record, &pdb->records, pointers) */

	pdb_free(prevFd, prevPdb);
	return 0;
}

static int __save_as_previous_pdb(char ** pathToPrevPDB, char * pathToCurrentPDB,
								  char * dataDir, const char * prevPdbFname);
static int __cp(char * from, char * to);

/**
   Save current PDB file as old PDB file.

   Save PDB file currently downloaded from Palm handheld as PDB file
   from previous synchronization iteration. File will be stored to
   daemon data directory.

   @param[in] syncSettings synchronization settings.
   @param[in] palmData structure with paths to PDB files, downloaded from
   handheld.
   @return Zero on success, otherwise non-zero value will be returned.
*/
int _save_as_previous_pdbs(SyncSettings * syncSettings, PalmData * palmData)
{
	if(__save_as_previous_pdb(&syncSettings->prevDatebookPDB,
							  palmData->datebookDBPath,
							  syncSettings->dataDir,
							  PREV_DATEBOOK_PDB))
	{
		log_write(LOG_ERR, "Failed to copy %s as old Datebook PDB file",
				  palmData->datebookDBPath);
		return -1;
	}
	if(__save_as_previous_pdb(&syncSettings->prevMemosPDB,
							  palmData->memoDBPath,
							  syncSettings->dataDir,
							  PREV_MEMOS_PDB))
	{
		log_write(LOG_ERR, "Failed to copy %s as old Memos PDB file",
				  palmData->memoDBPath);
		return -1;
	}
	if(__save_as_previous_pdb(&syncSettings->prevTodoPDB,
							  palmData->todoDBPath,
							  syncSettings->dataDir,
							  PREV_TODO_PDB))
	{
		log_write(LOG_ERR, "Failed to copy %s as old TODO PDB file",
				  palmData->todoDBPath);
		return -1;
	}
	return 0;
}

/**
   Copy PDB file to given path.

   Function check given path to previous PDB file for existence. If it
   is NULL, then it will be constructed as concatenation of dataDir
   and prevPdbFname strings.

   Then a new hardlink will be created at given path. Hardlink will
   point to pathtoCurrentPDB file.

   @param[in] pathToPrevPDB path to copy previous PDB file to.
   @param[in] pathToCurrentPDB path to PDB file, downloaded from Palm handheld
   in this synchronization cycle.
   @param[in] dataDir path to directory, where PDB files from previous
   synchronization is stored.
   @param[in] prevPdbFName designated filename for PDB file from previous
   synchronization cycle.
   @return Zero on success, otherwise non-zero value will be returned.
*/
int __save_as_previous_pdb(char ** pathToPrevPDB, char * pathToCurrentPDB,
						   char * dataDir, const char * prevPdbFname)
{
	if(*pathToPrevPDB == NULL)
	{
		if((*pathToPrevPDB = calloc(
				strlen(dataDir) + strlen(prevPdbFname) + 1,
				sizeof(char))) == NULL)
		{
			log_write(LOG_ERR, "Failed to allocate memory for %s filepath",
					  prevPdbFname);
			return -1;
		}
		strncpy(*pathToPrevPDB, dataDir, strlen(dataDir));
		strcat(*pathToPrevPDB, prevPdbFname);
		log_write(LOG_DEBUG, "Constructed next file path: %s - to store PDB file as from prev sync",
				  *pathToPrevPDB);
	}

	if(__cp(pathToCurrentPDB, *pathToPrevPDB))
	{
		log_write(LOG_ERR, "Failed copy %s to %s: %s", pathToCurrentPDB,
				  *pathToPrevPDB, strerror(errno));
		return -1;
	}
	log_write(LOG_DEBUG, "Copy %s to %s", pathToCurrentPDB, *pathToPrevPDB);
	return 0;
}

/**
   Simple realization of cp here.

   @param[in] from Path for copy source.
   @param[in] to Path to copy target.
   @return Zero on success, non-zero value on error.
*/
int __cp(char * from, char * to)
{
	int fromFd = -1;
	int toFd = -1;

	if((fromFd = open(from, O_RDONLY)) < 0)
	{
		log_write(LOG_ERR, "Cannot open %s to copy", from);
		return -1;
	}
	if((toFd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
					S_IRUSR | S_IWUSR | S_IRGRP)) < 0)
	{
		log_write(LOG_ERR, "Cannot open %s as copy target", to);
		goto cp_error;
	}

	uint8_t buffer[COPY_BUFFER_LENGTH];
	ssize_t readed;
	while(readed = read(fromFd, buffer, COPY_BUFFER_LENGTH), readed > 0)
	{
		uint8_t * outBuffer = buffer;
		ssize_t written;

		do
		{
			written = write(toFd, outBuffer, readed);
			if(written >= 0)
			{
				readed -= written;
				outBuffer += written;
			}
			else if(errno != EINTR)
			{
				goto cp_error;
			}
		}
		while(readed > 0);
	}

	if(readed == 0)
	{
		if(close(toFd))
		{
			log_write(LOG_ERR, "Cannot close %s file", to);
			toFd = -1;
			goto cp_error;
		}
		if(close(fromFd))
		{
			log_write(LOG_ERR, "Cannot close %s file", from);
			return -1;
		}
		return 0;
	}

cp_error:
	if(toFd != -1 && close(toFd))
	{
		log_write(LOG_ERR, "Cannot close %s file", to);
	}
	if(close(fromFd))
	{
		log_write(LOG_ERR, "Cannot close %s file", from);
	}
	return -1;
}
