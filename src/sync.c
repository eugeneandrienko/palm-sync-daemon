#include "log.h"
#include "palm.h"
#include "pdb_memos.h"
#include "org_notes.h"
#include "sync.h"


static int _sync_memos(char * pdbPath, char * orgPath, int dryRun);


int sync_this(SyncSettings * syncSettings)
{
	int palmfd = 0;
	if((palmfd = palm_open(syncSettings->device)) == -1)
	{
		return PALM_NOT_CONNECTED;
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

	if(_sync_memos(palmData->memoDBPath, syncSettings->notesOrgFile, syncSettings->dryRun))
	{
		log_write(LOG_ERR, "Failed to synchronize Memos");
		palm_free(palmData);
		return -1;
	}

	if(palm_write(palmfd, palmData))
	{
		log_write(LOG_ERR, "Failed to write PDB files to Palm");
		palm_free(palmData);
		if(palm_close(palmfd, syncSettings->device))
		{
			log_write(LOG_ERR, "Failed to close Palm device");
		}
		return -1;
	}

	palm_free(palmData);
	if(palm_close(palmfd, syncSettings->device))
	{
		return -1;
	}
	return 0;
}

/**
   Synchonize Memos data and OrgMode notes file.

   @param[in] pdbPath Path to temporary PDB file from Palm PDA.
   @param[in] orgPath Path to OrgMode file with notes.
   @param[in] dryRun If non-zero - do not sync data, just simulate process.
   @return Zero on sucessfull or non-zero on error.
*/
int _sync_memos(char * pdbPath, char * orgPath, int dryRun)
{
	/* Read memos from PDB file */
	PDB * pdb;
	if((pdb = pdb_memos_read(pdbPath)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read MemosDB");
		return -1;
	}

	/* Read notes from OrgMode file */
	OrgNotes * notes;
	if((notes = org_notes_parse(orgPath)) == NULL)
	{
		log_write(LOG_ERR, "Failed to parse file with notes: %s", orgPath);
		return -1;
	}

	/* Compare notes */
	/* Sync notes from Palm with OrgMode file */
	/* Sync notes from OrgMode file with Palm */
	/* Sync notes, which exists both in Palm PDA and OrgMode files, depends on
	   modification time */
	return 0;
}
