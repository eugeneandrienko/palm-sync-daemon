/**
   @author Eugene Andrienko
   @brief
   @file sync.h

   TODO
*/

/**
   @page sync Synchronization procedure

   TODO
*/

#ifndef _SYNC_H_
#define _SYNC_H_

/**
   Special return value for sync() - returns if Palm PDA
   not connected and sync impossible. In that case program
   should wait for the device to connect to the system.
*/
#define PALM_NOT_CONNECTED -2


/**
   Settings for synchronization.
*/
struct SyncSettings
{
	char * device;          /**< Path to symbolic device, to which Palm PDA is
							   connected. */
	char * notesOrgFile;    /**< Path to OrgMode file with notes. */
	char * todoOrgFile;     /**< Path to OrgMode file with TODOs and calendar
							   events. */
	int dryRun;             /**< If non-zero - do not sync data, just simulate
							   process. */
	char * dataDir;         /**< Path to directory with data from previous sync
							   iteration. */
	char * prevDatebookPDB; /**< Path to PDB file with Datebook from previous
							   iteration. */
	char * prevMemosPDB;    /**< Path to PDB file with Memos from previous
							   iteration. */
	char * prevTodoPDB;     /**< Path to PDB file with TODO from previous
							   iteration. */
	char * prevTasksPDB;    /**< Path to PDB file with TasksDB-PTod from
							   previous iteration. */
};
typedef struct SyncSettings SyncSettings;

/**
   Performs synchronization task.

   @param[in] syncSettings settings for synchronization.
   @return Zero on success, non-zero value when sync failed.
   Or NOT_CONNECTED if Palm device not connected to system.
*/
int sync_this(SyncSettings * syncSettings);

#endif
