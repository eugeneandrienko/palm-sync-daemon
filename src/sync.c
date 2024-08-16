#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "helper.h"
#include "log.h"
#include "palm.h"
#include "pdb_memos.h"
#include "org_notes.h"
#include "sync.h"


static int _sync_memos(char * pdbPath, char * prevPdbPath, char * orgPath, int dryRun);
static int _compute_record_statuses(PDB * pdb, char * prevPdbPath);


int sync_this(SyncSettings * syncSettings)
{
	int palmfd = 0;
	if((palmfd = palm_open(syncSettings->device)) == -1)
	{
		return PALM_NOT_CONNECTED;
	}

	if(check_previous_pdbs(syncSettings))
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

	if(!syncSettings->dryRun && save_as_previous_pdbs(syncSettings, palmData))
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

/**
   Synchonize Memos data and OrgMode notes file.

   @param[in] pdbPath Path to temporary PDB file from Palm PDA.
   @param[in] prevPdbPath Path to PDB file from previous synchronization cycle.
   @param[in] orgPath Path to OrgMode file with notes.
   @param[in] dryRun If non-zero - do not sync data, just simulate process.
   @return Zero on sucessfull or non-zero on error.
*/
static int _sync_memos(char * pdbPath, char * prevPdbPath, char * orgPath, int dryRun)
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
	const OrgNotes * notes;
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
			record->status = RECORD_ADDED;
			log_write(LOG_DEBUG, "Record %02x%02x%02x: %d", record->id[2],
					  record->id[1], record->id[0], record->status);
		}
		return 0;
	}

	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		const uint8_t attribute = record->attributes & 0xf0;
		if(attribute & PDB_RECORD_ATTR_SECRET ||
		   attribute & PDB_RECORD_ATTR_LOCKED)
		{
			record->status = RECORD_NO_RECORD;
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
					record->status = RECORD_DELETED;
					break;
				case PDB_RECORD_ATTR_DIRTY:
					record->status = RECORD_ADDED;
					break;
				default:
					record->status = RECORD_ADDED;
				}
				break;
			case PDB_RECORD_ATTR_DIRTY:
				switch(attribute)
				{
				case PDB_RECORD_ATTR_DELETED:
					record->status = RECORD_DELETED;
					break;
				case PDB_RECORD_ATTR_DIRTY:
					record->status = RECORD_CHANGED;
					break;
				default:
					record->status = RECORD_NOT_CHANGED;
				}
				break;
			default: /* Record just exists */
				switch(attribute)
				{
				case PDB_RECORD_ATTR_DELETED:
					record->status = RECORD_DELETED;
					break;
				case PDB_RECORD_ATTR_DIRTY:
					record->status = RECORD_CHANGED;
					break;
				default:
					record->status = RECORD_NOT_CHANGED;
				}
			}
		}

		if(!matchedPrevRecordFound)
		{
			record->status = (attribute & PDB_RECORD_ATTR_DELETED) ?
				RECORD_NO_RECORD :
				RECORD_ADDED;
		}

		log_write(LOG_DEBUG, "Record %02x%02x%02x: %d", record->id[2],
				  record->id[1], record->id[0], record->status);
	} /* TAILQ_FOREACH(record, &pdb->records, pointers) */

	pdb_free(prevFd, prevPdb);
	return 0;
}
