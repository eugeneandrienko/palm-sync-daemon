/**
   @author Eugene Andrienko
   @brief Module to operate with header of PDB file
   @file pdb.h

   This module open PDB file and parse it's header. File descriptor from
   pdb_open() can be used in other modules to read remaining application data in
   other modules.

   To open PDB file use pdb_open() function. Then, pass file descriptor to
   pdb_read() which allocate memory for PDBFile structure and fills it with data
   from header.

   All mutli-byte numbers (offsets, timestamps, etc) will be saved to PDBFile in
   host system endianess!

   After modification of in-memory data, it can be written back to
   PDB file. Call pdb_write() for this.

   All multi-byte numbers (offsets, timestamps, etc) will be written to PDB file
   on disk as big-endian!

   After that you can call pdb_close() to close opened PDB file. File descriptor
   mustn't be used after that call. Also, call pdb_free() to free allocated
   memory for PDBFile structure.

   There are some support functions to operate with records and categories —
   add, delete and edit it. Each function will change application info offset
   and records qty fields in PDBFile structure to remain consistency of this
   structure.
*/

/**
   @page pdb Work with PDB header

   This module provides set of functions to operate with PDB file header.

   To read/write header to PDB file to/from PDBFile structure, there are next
   set of functions:
   - pdb_open()
   - pdb_read()
   - pdb_write()
   - pdb_close()

   To free memory, allocated for PDBFile structure, use pdb_free() structure.

   To edit record list, when we add/edit/delete some application data in other
   module:
   - pdb_record_get()
   - pdb_record_add()
   - pdb_record_delete()

   To operate with standard Palm OS categories:
   - pdb_category_get()
   - pdb_category_add()
   - pdb_category_edit()
   - pdb_category_delete()
*/


#ifndef _PDB_H_
#define _PDB_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>


/**
   Secret record.
*/
#define PDB_RECORD_ATTR_SECRET  0x10
/**
   Record locked by PalmOS.
*/
#define PDB_RECORD_ATTR_LOCKED  0x20
/**
   Record modified since it's creation.
*/
#define PDB_RECORD_ATTR_DIRTY   0x40
/**
   Deleted on Palm OS, should be deleted on next sync.
*/
#define PDB_RECORD_ATTR_DELETED 0x80


/**
   Database name length, including null-termination byte.
*/
#define PDB_DBNAME_LEN         32
/**
   Max count of standardized Palm OS categories.
*/
#define PDB_CATEGORIES_STD_LEN 16
/**
   Length of category string, including null-termination byte.
*/
#define PDB_CATEGORY_LEN       16


/**
   One record from record queue.
*/
struct PDBRecord
{
	uint32_t offset;                 /**< Offset to record data */
	uint8_t attributes;              /**< Record attributes */
	uint8_t id[3];                   /**< Record unique ID */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	TAILQ_ENTRY(PDBRecord) pointers; /**< Connection between elements in tail queue */
#endif
};
#ifndef DOXYGEN_SHOULD_SKIP_THIS
TAILQ_HEAD(RecordQueue, PDBRecord);
#endif
typedef struct PDBRecord PDBRecord;

/**
   Application info with standard Palm OS categories.
*/
struct PDBCategories
{
	uint16_t renamedCategories;                           /**< Renamed categories (WTF?) */
	char names[PDB_CATEGORIES_STD_LEN][PDB_CATEGORY_LEN]; /**< Array with categories names */
	uint8_t ids[PDB_CATEGORIES_STD_LEN];                  /**< Array with categories IDs */
	uint8_t lastUniqueId;                                 /**< Last unique category ID (usually 0x0f) */
	uint8_t padding;                                      /**< Zero byte for padding */
};
typedef struct PDBCategories PDBCategories;

/**
   Standardized header of PDB file.
*/
struct PDBFile
{
	char dbname[PDB_DBNAME_LEN];   /**< Null-terminated string with database name */
	uint16_t attributes;           /**< Attributes */
	uint16_t version;              /**< Version */
	uint32_t ctime;                /**< Creation datetime */
	uint32_t mtime;                /**< Modification datetime */
	uint32_t btime;                /**< Datetime of last backup */
	uint32_t modificationNumber;   /**< Modification database number */
	uint32_t appInfoOffset;        /**< Offset to application info */
	uint32_t sortInfoOffset;       /**< Offset to sort info */
	uint32_t databaseTypeID;       /**< Database type */
	uint32_t creatorID;            /**< Creator ID */
	uint32_t seed;                 /**< Unique seed */
	uint32_t nextRecordListOffset; /**< Offset to next record list. Should be 0x00000000 */
	uint16_t recordsQty;           /**< Qty of records */
	struct RecordQueue records;    /**< Record list */
	uint16_t recordListPadding;    /**< Padding bytes after record list */
	PDBCategories * categories;    /**< Categories from PDB file. May be NULL if not applicable */
};
typedef struct PDBFile PDBFile;


/**
   Open PDB file.

   Call it before any read/write from/to PDB file.

   @param[in] path Path to PDB file.
   @return PDB file descriptor or -1 if error.
*/
int pdb_open(const char * path);

/**
   Read header and other standard info from PDB file to PDBFile structure.

   Reads PDB file with given descriptor. Then, allocate memory for header and
   records to fill it with actual data. Categories may by NULL if we do not use
   standard Palm OS categories in given PDB file.

   Allocated memory should be freed outside of this function.

   All multi-byte numbers will be converted to host system endianess. All
   timestamps will be converted to Unix timestamps.

   @param[in] fd PDB file descriptor.
   @param[in] stdCatInfo Set to non-zero if there is a standard Palm OS category information.
   @return Pointer to initialized PDBFile structure or NULL if error.
*/
PDBFile * pdb_read(int fd, int stdCatInfo);

/**
   Write header and other standard information to PDB file.

   All multi-byte numbers will be converted to big-endian. All timestamps will
   be converted to Mac timestamps.

   @param[in] fd File descriptor.
   @param[in] pdbFile Pointer to the PDBFile structure with data.
   @return Zero if write successfull, otherwise non-zero
*/
int pdb_write(int fd, PDBFile * pdbFile);

/**
   Close PDB file.

   @param[in] fd File descriptor.
*/
void pdb_close(int fd);

/**
   Free memory allocated for PDBFile.

   @param[in] pdbFile Pointer to PDBFile structure.
*/
void pdb_free(PDBFile * pdbFile);

/**
   Add new record to the record list end.

   @param[in] pdbFile Pointer to PDBFile structure.
   @param[in] record New PDBRecord to insert.
   @return Zero if success or non-zero value on error.
*/
int pdb_record_add(PDBFile * pdbFile, PDBRecord record);

/**
   Returns pointer to PDBRecord from record list.

   Get Nth record from record queue where N is index. N starts from 0 to
   PDBFile.recordsQty - 1

   @param[in] pdbFile Pointer to PDBFile structure.
   @param[in] index Number of record to get. Starts from zero.
   @return Pointer to PDBRecord or NULL if not found or on error.
*/
PDBRecord * pdb_record_get(PDBFile * pdbFile, uint16_t index);

/**
   Delete given record from the records list.

   @param[in] pdbFile Pointer to PDBFile structure.
   @param[in] record Record to delete.
   @return Zero if success or non-zero value on error.
*/
int pdb_record_delete(PDBFile * pdbFile, PDBRecord * record);

/**
   Returns pointer to category name.

   Category ID starts from zero to PDB_CATEGORIES_STD_LEN - 1.

   @param[in] pdbFile Pointer to PDBFile structure.
   @param[in] id Category ID. Starts from zero.
   @return Pointer to category name or NULL on error.
*/
char * pdb_category_get(PDBFile * pdbFile, uint8_t id);

/**
   Add new category.

   Category ID starts from zero to PDB_CATEGORIES_STD_LEN - 1.

   @param[in] pdbFile Pointer to PDBFile structure.
   @param[in] id New category ID. Starts from zero.
   @param[in] name Name of new category.
   @return Zero on successfull or non-zero on error.
*/
int pdb_category_add(PDBFile * pdbFile, uint8_t id, char * name);

/**
   Edit existsing category.

   @param[in] category Pointer to category name.
   @param[in] newName New category name.
   @param[in] length Length of new category name. Cannot be greater than PDB_CATEGORY_LEN.
   @return Zero on successfull or non-zero on error.
*/
int pdb_category_edit(char * category, char * newName, size_t length);

/**
   Delete existing category.

   @param[in] pdbFile Pointer to PDBFile structure.
   @param[in] id Category ID to delete. Starts from zero to PDB_CATEGORIES_STD_LEN - 1.
   @return Zero on successfull or non-zero on error.
*/
int pdb_category_delete(PDBFile * pdbFile, uint8_t id);

#endif
