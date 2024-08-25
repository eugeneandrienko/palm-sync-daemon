#include <stdbool.h>
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


/**
   Maximal length of synclog string.
*/
#define SYNC_LOG_LENGTH 1000

/**
   Possible synchronization actions for items to sync.
*/
enum SyncAction
{
	ACTION_ERROR,               /**< Got error */
	ACTION_DO_NOTHING,          /**< Do nothing */
	ACTION_ADD_TO_DESKTOP,      /**< Add handheld record to desktop */
	ACTION_ADD_TO_HANDHELD,     /**< Add desktop record to handheld */
	ACTION_COPY_TO_DESKTOP,     /**< Copy handheld record to desktop */
	ACTION_REPLACE_ON_HANDHELD, /**< Replace handheld record with dekstop
								   record */
	ACTION_DELETE_ON_HANDHELD   /**< Delete record from handheld */
};
typedef enum SyncAction SyncAction;

static int _sync_memos(char * pdbPath, char * prevPdbPath, char * orgPath,
					   int palmfd, int dryRun);
static int _compute_record_statuses(PDB * pdb, char * prevPdbPath);
static SyncAction _compute_action_for_record(enum RecordStatus recordStatus,
											 bool orgNoteExists);


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
				   syncSettings->notesOrgFile, palmfd, syncSettings->dryRun))
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
		log_write(LOG_ERR, "Failed to save PDB files as files from previous "
				  "iteration");
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
   @param[in] palmfd Palm device descriptor.
   @param[in] dryRun If non-zero - do not sync data, just simulate process.
   @return Zero on sucessfull or non-zero on error.
*/
static int _sync_memos(char * pdbPath, char * prevPdbPath, char * orgPath,
					   int palmfd, int dryRun)
{
	/* Read memos from PDB file */
	PDB * pdb;
	if((pdb = pdb_memos_read(pdbPath)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read MemosDB");
		palm_log(palmfd, "Cannot parse Memos\n");
		return -1;
	}
	if(_compute_record_statuses(pdb, prevPdbPath))
	{
		log_write(LOG_ERR, "Cannot compute statuses for records from %s",
				  pdbPath);
		palm_log(palmfd, "Cannot parse Memos\n");
		return -1;
	}

	/* Read notes from OrgMode file */
	const OrgNotes * notes;
	if((notes = org_notes_parse(orgPath)) == NULL)
	{
		log_write(LOG_ERR, "Failed to parse file with notes: %s", orgPath);
		char log[SYNC_LOG_LENGTH];
		snprintf(log, SYNC_LOG_LENGTH, "Cannot parse OrgMode file: %s\n",
				 orgPath);
		palm_log(palmfd, log);
		return -1;
	}

	/* Open org-file for writing */
	int orgNoteFd = org_notes_open(orgPath);
	if(orgNoteFd == -1)
	{
		log_write(LOG_ERR, "Failed to open org-file %s for writing", orgPath);
		char log[SYNC_LOG_LENGTH];
		snprintf(log, SYNC_LOG_LENGTH, "Cannot parse OrgMode file: %s\n",
				 orgPath);
		palm_log(palmfd, log);
		return -1;
	}

	/* Compare and sync notes from handheld with notes from org-file */
	unsigned int qtyDesktopAdded = 0;
	unsigned int qtyHandheldAdded = 0;
	unsigned int qtyHandheldReplaced = 0;
	unsigned int qtyHandheldDeleted = 0;
	unsigned int qtyErrors = 0;
	PDBRecord * record;
	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		OrgNote * note = TAILQ_FIRST(notes);
		while(note != NULL && note->header_hash != record->hash)
		{
			note = TAILQ_NEXT(note, pointers);
		}

		PDBMemo * memo = (PDBMemo *)record->data;
		SyncAction action = _compute_action_for_record(
			record->status, note != NULL);
		switch(action)
		{
		case ACTION_DO_NOTHING:
			break;
		case ACTION_ADD_TO_DESKTOP:
		case ACTION_COPY_TO_DESKTOP:
			log_write(LOG_INFO, "Add note \"%s\" from handheld to desktop",
					  iconv_cp1251_to_utf8(memo->header));
			char * category = pdb_category_get_name(
				pdb, record->attributes & 0x0f);
			if(category == NULL)
			{
				log_write(LOG_ERR, "Failed to get note (\"%s\") category with "
						  "id = %d", iconv_cp1251_to_utf8(memo->header),
						  record->attributes & 0x0f);
				break;
			}
			if(dryRun)
			{
				break;
			}
			if(org_notes_write(orgNoteFd, memo->header, memo->text, category))
			{
				log_write(LOG_ERR, "Failed to write note (\"%s\") to org "
						  "file %s", iconv_cp1251_to_utf8(memo->header),
						  orgPath);
			}
			qtyDesktopAdded++;
			break;
		case ACTION_ADD_TO_HANDHELD:
			log_write(LOG_INFO, "Add note \"%s\" from desktop to handheld",
					  iconv_cp1251_to_utf8(note->header));
			if(pdb_memos_memo_add(
				   pdb, note->header, note->text, note->category) == NULL)
			{
				log_write(LOG_ERR,
						  "Failed to add note (\"%s\") from desktop to handheld",
						  iconv_cp1251_to_utf8(note->header));
			}
			qtyHandheldAdded++;
			break;
		case ACTION_REPLACE_ON_HANDHELD:
			log_write(LOG_INFO, "Replacing \"%s\" memo on handheld with "
					  "desktop version", iconv_cp1251_to_utf8(memo->header));
			if(pdb_memos_memo_edit(pdb, memo, note->header, note->text,
								   note->category))
			{
				log_write(LOG_ERR,
						  "Failed to replace memo (\"%s\") on handheld with "
						  "desktop note", iconv_cp1251_to_utf8(memo->header));
			}
			qtyHandheldReplaced++;
			break;
		case ACTION_DELETE_ON_HANDHELD:
			log_write(LOG_INFO, "Removing \"%s\" memo on handheld",
					  iconv_cp1251_to_utf8(memo->header));
			if(pdb_memos_memo_delete(pdb, memo))
			{
				log_write(LOG_ERR,
						  "Failed to remove memo (\"%s\") on handheld",
						  iconv_cp1251_to_utf8(memo->header));
			}
			qtyHandheldDeleted++;
			break;
		case ACTION_ERROR:
		default:
			log_write(LOG_ERR, "Unknown record (%s) status: %d", memo->header,
					  record->status);
			log_write(LOG_ERR, "Unknown action number: %d", action);
			qtyErrors++;
		}
	} /* TAILQ_FOREACH(...) */

    /* Process notes from org-file which are not exists in Palm yet */
	OrgNote * note;
	TAILQ_FOREACH(note, notes, pointers)
	{
		record = TAILQ_FIRST(&pdb->records);
		while(record != NULL && note->header_hash != record->hash)
		{
			record = TAILQ_NEXT(record, pointers);
		}
		if(record == NULL)
		{
			log_write(LOG_INFO, "Adding new record (\"%s\") to handheld from "
					  "org-file",
					  iconv_cp1251_to_utf8(note->header));
			if(pdb_memos_memo_add(
				   pdb, note->header, note->text, note->category) == NULL)
			{
				log_write(LOG_ERR,
						  "Failed to add note (\"%s\") from desktop to handheld",
						  iconv_cp1251_to_utf8(note->header));
			}
			qtyHandheldAdded++;
		}
	}

	/* Writing changes back to files */
	char message[SYNC_LOG_LENGTH];
	snprintf(message, SYNC_LOG_LENGTH, "Notes added to desktop: %d\n"
			 "Notes added to handheld: %d\n"
			 "Notes replaced on handheld: %d\n"
			 "Notes deleted on handheld: %d\n"
			 "Notes with errors: %d\n",
			 qtyDesktopAdded, qtyHandheldAdded, qtyHandheldReplaced,
			 qtyHandheldDeleted, qtyErrors);
	palm_log(palmfd, message);
	if(org_notes_close(orgNoteFd))
	{
		log_write(LOG_ERR, "Failed to close org-file %s opened for writing",
				  orgPath);
		return -1;
	}
	if(!dryRun)
	{
		if(pdb_memos_write(pdbPath, pdb))
		{
			log_write(LOG_ERR, "Failed to write redacted PDB with memos to "
					  "file: %s", pdbPath);
			return -1;
		}
	}
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
		log_write(LOG_WARNING, "Cannot open %s file as PDB from previous "
				  "synchronization", prevPdbPath);
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

/**
   Compute action for given records from Palm handheld and from org-file.

   @param[in] recordStatus Status of record from Palm handheld.
   @param[in] orgNoteExists True if corresponding OrgMode record exists
   in org-files.
   @return Calculated action for these records.
*/
static SyncAction _compute_action_for_record(enum RecordStatus recordStatus,
											 bool orgNoteExists)
{
	switch(recordStatus)
	{
	case RECORD_NO_RECORD:
		return orgNoteExists ? ACTION_ADD_TO_HANDHELD : ACTION_DO_NOTHING;
	case RECORD_ADDED:
		return orgNoteExists ? ACTION_COPY_TO_DESKTOP : ACTION_ADD_TO_DESKTOP;
	case RECORD_NOT_CHANGED:
		return orgNoteExists ?
			ACTION_REPLACE_ON_HANDHELD : ACTION_ADD_TO_DESKTOP;
	case RECORD_CHANGED:
		return orgNoteExists ? ACTION_COPY_TO_DESKTOP : ACTION_ADD_TO_DESKTOP;
	case RECORD_DELETED:
		return orgNoteExists ? ACTION_DO_NOTHING : ACTION_DELETE_ON_HANDHELD;
	default:
		return ACTION_ERROR;
	}
}
