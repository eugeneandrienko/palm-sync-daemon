#ifndef _PDB_H_
#define _PDB_H_

/* ------------------------------------------------------------- */
/* All multi-byte data will be written in host system endianess! */
/* ------------------------------------------------------------- */

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

#define PDB_RECORD_ATTR_SECRET  0x10 /** Secret record */
#define PDB_RECORD_ATTR_LOCKED  0x20 /** Record locked by PalmOS */
#define PDB_RECORD_ATTR_DIRTY   0x40 /** Record modified since it's creation */
#define PDB_RECORD_ATTR_DELETED 0x80 /** Deleted on Palm OS, should be deleted on next sync */

#define PDB_DBNAME_LEN         32 /** Database name length including null-termination byte */
#define PDB_CATEGORIES_STD_LEN 16 /** Max count of standardized Palm OS categories */
#define PDB_CATEGORY_LEN       16 /** Length of category string */

/**
   One record from record queue
*/
struct PDBRecord
{
	uint32_t offset;                 /** Offset to record data */
	uint8_t attributes;              /** Record attributes */
	uint8_t id[3];                   /** Record unique ID */
	TAILQ_ENTRY(PDBRecord) pointers; /** Connection between elements in tail queue */
};
TAILQ_HEAD(RecordQueue, PDBRecord);
typedef struct PDBRecord PDBRecord;

/**
   Application info with standard Palm OS categories
*/
struct PDBCategories
{
	uint16_t renamedCategories;                           /** Renamed categories (WTF?) */
	char names[PDB_CATEGORIES_STD_LEN][PDB_CATEGORY_LEN]; /** Array with categories names */
	uint8_t ids[PDB_CATEGORIES_STD_LEN];                  /** Array with categories IDs */
	uint8_t lastUniqueId;                                 /** Last unique category ID */
	uint8_t padding;                                      /** Zero byte for padding */
};
typedef struct PDBCategories PDBCategories;

/**
   Standardized part of PDB file
*/
struct PDBFile
{
	char dbname[PDB_DBNAME_LEN];   /** Null-terminated string with database name */
	uint16_t attributes;           /** Attributes */
	uint16_t version;              /** Version */
	uint32_t ctime;                /** Creation datetime */
	uint32_t mtime;                /** Modification datetime */
	uint32_t btime;                /** Datetime of last backup */
	uint32_t modificationNumber;   /** Modification database number */
	uint32_t appInfoOffset;        /** Offset to application info */
	uint32_t sortInfoOffset;       /** Offset to sort info */
	uint32_t databaseTypeID;       /** Database type */
	uint32_t creatorID;            /** Creator ID */
	uint32_t seed;                 /** Unique seed */
	uint32_t nextRecordListOffset; /** Offset to next record list. Should be 0x00000000 */
	uint16_t recordsQty;           /** Qty of records */
	struct RecordQueue records;    /** Record list */
	uint16_t recordListPadding;    /** Padding bytes after record list */
	PDBCategories * categories;    /** Categories from PDB file. May be NULL if not applicable */
};
typedef struct PDBFile PDBFile;

/**
   Open PDB file.

   @param path Path to PDB file
   @return PDB file descriptor or -1 if error
*/
int pdb_open(const char * path);

/**
   Read header and other standard info from PDB file.

   Reads PDB file from given descriptor. Then allocate memory for header and
   records to fill it with actual data. Categories may by NULL if we do not use
   standard Palm OS categories in given PDB file.

   Allocated memory should be freed outside of this function.

   All multi-byte numbers already converted to right endianess by libpisock. All
   timestamps will be converted to Unix timestamps.

   @param fd PDB file descriptor
   @param PDBFile Pointer to PDBFile structure. It will be filled inside this function.
   @param stdCatInfo Set to non-zero if there is standard Palm OS category information.
   @return Zero if read successfull, otherwise non-zero value will be returned
*/
int pdb_read(int fd, PDBFile * pdbFile, int stdCatInfo);

/**
   Write header and other standard info to PDB file.

   All multi-byte numbers will be converted to right endianess by libpisock. All
   timestamps will be converted to Mac timestamps.

   @param fd File descriptor
   @param pdbFile Pointer to filled PDBFile structure
   @return Zero if write successfull, otherwise non-zero
*/
int pdb_write(int fd, PDBFile * pdbFile);

/**
   Close opened PDB file

   @param fd File descriptor
*/
void pdb_close(int fd);

/**
   Free allocated memory for standard PDB file structures.

   @param pdbFile PDBFile structure to free
*/
void pdb_free(PDBFile * pdbFile);

/**
   Add new record to the end of the queue.

   @param pdbFile Pointer to PDBFile structure
   @param record New record to insert
   @return 0 if success or non-zero value on error
*/
int pdb_record_add(PDBFile * pdbFile, PDBRecord record);

/**
   Returns pointer to PDBRecord from records queue.

   Get Nth record from record queue where N is index.

   @param pdbFile Pointer to PDBFile structure
   @param index Number of record to get. Starts from zero.
   @return Pointer to PDBRecord or NULL on error
*/
PDBRecord * pdb_record_get(PDBFile * pdbFile, uint16_t index);

/**
   Delete given record from the queue.

   @param pdbFile Pointer to PDBFile structure
   @param record Record to delete
   @return 0 if success or non-zero value on error
*/
int pdb_record_delete(PDBFile * pdbFile, PDBRecord * record);

/**
   Returns pointer to category name

   @param pdbFile Pointer to PDBFile structure
   @param id Category ID. Starts from zero.
   @return Pointer to category name or NULL on error
*/
char * pdb_category_get(PDBFile * pdbFile, uint8_t id);

/**
   Add new category

   @param pdbFile Pointer to PDBFile structure
   @param id New category ID. Starts from zero.
   @param name Name of new category
   @return Zero on successfull or non-zero on error
*/
int pdb_category_add(PDBFile * pdbFile, uint8_t id, char * name);

/**
   Edit existsing category.

   @param category Pointer to category name
   @param newName New category name
   @param length Length of new category name
   @return Zero on successfull or non-zero on error
*/
int pdb_category_edit(char * category, char * newName, size_t length);

/**
   Delete category

   @param pdbFile Pointer to PDBFile structure
   @param id Category ID to delete. Starts from zero.
   @return Zero on successfull or non-zero on error
*/
int pdb_category_delete(PDBFile * pdbFile, uint8_t id);

#endif
