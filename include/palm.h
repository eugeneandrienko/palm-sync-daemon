#ifndef _PALM_H_
#define _PALM_H_

/**
   Temporary PDB files from Palm device
*/
struct palm_data {
	char * datebookDBPath; /** Path to DatebookDB */
	char * memoDBPath;     /** Path to MemoDB */
	char * todoDBPath;     /** Path to ToDoDB */
};
typedef struct palm_data PalmData;

/**
   Open connection to Palm device

   @param device Path to symbolic device on the system
   @return Device descriptor or -1 if error happens
*/
int palm_open(char * device);

/**
   Read Palm databases from Palm PDA.

   Read next databases: DatebookDB, MemoDB, ToDoDB.
   Fill initialized PalmData structure with paths to PDB files.

   @param sd Palm device descriptor
   @param data Initialized palm_data structure
   @return 0 if read successfull, otherwise -1.
*/
int palm_read(int sd, PalmData * data);

/**
   Write Palm databases to Palm PDA.

   Write next databases: DatebookDB, MemoDB, ToDoDB.
   Paths to PDB files taken from initialized PalmData structure.

   @param sd Palm device descriptor
   @param data Initialized and filled PalmData structure
   @return 0 if write successfull, otherwise -1.
*/
int palm_write(int sd, PalmData * data);

/**
   Close connection to Palm device.

   @param sd Device descriptor
   @param device Path to file with Palm device
   @return 0 if successfull. Return -1 if failed to close connection - got timeout.
*/
int palm_close(int sd, char * device);

/**
   Clear PalmData structure.

   @param data Initialized PalmData structure
*/
void palm_free(PalmData * data);

#endif
