/**
   @author Eugene Andrienko
   @brief Functions to read/write from/to Palm device.
   @file palm.h

   Functions from this module **must** be called sequentally.

   First call should be palm_open() to open connection to Palm PDA.

   If it is successfull — call palm_read() to fill PalmData structure with valid
   data from PDA and download PDB-files to temporary files on the computer.

   After change data in PDB-files — call palm_write() to write updated data to
   the Palm PDA.

   If successfull, call palm_close() to close connection with Palm and
   disconnect Palm from PC.

   After that call palm_free() to remove temporary files and free memory
   allocated by PalmData structure.
*/

/**
   @page palm Palm connectivity

   Low-level module to read/write PDB files from/to Palm PDA.

   Depends on libpisock library.

   Typical one iteration of synchronization with Palm PDA should looks like
   this. First, our daemon found connected Palm PDA in HotSync mode. This is
   done via processing return value of palm_open().

   If PDA is connected and we successfully opened it — we can read PDB file from
   Palm. Structure PalmData is initialized inside of this function and we should
   use this structure **only after** palm_read() successfull completion.

   After, we can change PDB file and write it back to Palm PDA via
   palm_write(). If successfull — we can close Palm PDA via palm_close(). Device
   must be closed after successfull sequental calls to palm_read() and
   palm_write(). If you don't do this — Palm PDA will stay in HotSync mode and
   in next iteration of sync process — it will be started again.

   To free PalmData structure and remove unnecessary temporary PDB-file — call
   palm_free() after palm_close().
*/

#ifndef _PALM_H_
#define _PALM_H_

/**
   Record with paths to **temporary** PDB files.
*/
struct PalmData {
	char * datebookDBPath; /**< Path to DatebookDB */
	char * memoDBPath;     /**< Path to MemoDB */
	char * todoDBPath;     /**< Path to ToDoDB */
};
typedef struct PalmData PalmData;

/**
   Open connection to Palm device.

   This function should be called first, before any read/write to Palm PDA.

   @param[in] device Path to symbolic device connected to Palm PDA on the system.
   @return Device descriptor or -1 if error happens.
*/
int palm_open(char * device);

/**
   Read Palm databases from Palm PDA.

   Read next databases: DatebookDB, MemoDB, ToDoDB, writes it's contents to
   temporary files and fill PalmData structure with paths to these files.

   @param[in] sd Palm device descriptor.
   @return Initialized PalmData structure or NULL on error.
*/
PalmData * palm_read(int sd);

/**
   Write Palm databases to Palm PDA.

   Write next databases: DatebookDB, MemoDB, ToDoDB to Palm PDA. Paths to
   corresponding PDB-file will be taken from given pointer to PalmData
   structure.

   @param[in] sd Palm device descriptor.
   @param[in] data PalmData structure with paths to PDB files.
   @return 0 if write successfull, otherwise -1.
*/
int palm_write(int sd, PalmData * data);

/**
   Close connection to Palm device.

   This function should be called after all necessary read/write in current
   iteration of synchronization cycle. Without it Palm PDA stays connected to
   the system via HotSync!

   Function will wait while symbolic device disconnectes from system when
   HotSync finishes synchronization from other side.

   @param[in] sd Palm device descriptor.
   @param[in] device Path to symbolic device connected to Palm PDA.
   @return 0 if successfull or -1 if failed to close connection or got timeout when waiting.
*/
int palm_close(int sd, char * device);

/**
   Clear PalmData structure.

   Should be called after palm_close() to free all associated structures and
   remove unnecessary temporary files.

   @param[in] data Initialized PalmData structure.
*/
void palm_free(PalmData * data);

#endif
