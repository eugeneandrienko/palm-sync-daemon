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
   TODO

   @param[in] device Path to symbolic device, to which Palm PDA is connected.
   @param[in] notesOrgFile Path to OrgMode file with notes.
   @param[in] todoOrgFile Path to OrgMode file with TODOs and calendar events.
   @param[in] dryRun If non-zero - do not sync data, just simulate process.
   @return Zero on success, non-zero value when sync failed.
   Or NOT_CONNECTED if Palm device not connected to system.
*/
int sync_this(char * device, char * notesOrgFile, char * todoOrgFile, int dryRun);

#endif
