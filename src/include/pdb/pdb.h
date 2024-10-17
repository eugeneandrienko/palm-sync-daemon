/**
   @author Eugene Andrienko
   @brief Module with low-level operations for PDB files.
   @file pdb.h

   This module open PDB file and parse it's header.

   To open PDB file use pdb_open() function.

   To parse PDB file use pdb_read() function. This function will open
   PDB file and allocate memory for PDB structure and fills it with
   data from header. All application specific data should be read in
   application specific modules.

   File descriptor from pdb_read() can be used in other modules to
   read remaining application data in other modules.

   All mutli-byte numbers (offsets, timestamps, etc) will be saved to
   PDB in host system endianess!

   After modification of in-memory data, it can be written back to PDB
   file. Call pdb_write() for this. This function will write data from
   PDB structure to file. If writing error occurs, function will close
   opened PDB-file and free memory, allocated for PDB structure; file
   descriptor and PDB structure mustn't be used after error condition.

   All multi-byte numbers (offsets, timestamps, etc) will be written
   to PDB file on disk as big-endian!

   If PDB structure and opened file are not necessary anymore -
   pdb_close() function can be used to free resources.

   There are some support functions to operate with records and
   categories — add, delete and edit it. Each function will change
   application info offset and records qty fields in PDB structure to
   remain consistency of this structure.
*/

/**
   @page pdb Work with PDB file low-level

   This module provides set of low-level functions to operate with PDB files.

   To read/write PDB file to/from PDB structure, there are next set of
   functions:
   - pdb_open()
   - pdb_read()
   - pdb_write()
   - pdb_close()

   To edit record list, when we add/edit/delete some application data in other
   module:
   - pdb_record_create()
   - pdb_record_delete()

   To operate with standard Palm OS categories:
   - pdb_category_get_id()
   - pdb_category_get_name()
   - pdb_category_add()
   - pdb_category_delete()
*/


#ifndef _PDB_H_
#define _PDB_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>


/**
   No attributes for record.
*/
#define PDB_RECORD_ATTR_EMPTY   0x00
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
#define PDB_CATEGORIES_STD_QTY 16
/**
   Length of category string, including null-termination byte.
*/
#define PDB_CATEGORY_LEN       16
/**
   Default category on Palm PDA
*/
#define PDB_DEFAULT_CATEGORY   "Unfiled"
/** Record item size.

	- Offset to data: 32 bits
	- Attributes: 8 bits
	- Record unique ID: 24 bits

	Overall size: 64 bits = 8 bytes.
*/
#define PDB_RECORD_ITEM_SIZE   8


/**
   One record from record queue.
*/
struct PDBRecord
{
	uint32_t offset;                 /**< Offset to record data */
	uint8_t attributes;              /**< Record attributes */
	uint8_t id[3];                   /**< Record unique ID */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	TAILQ_ENTRY(PDBRecord) pointers; /**< Connection between elements in tail
										queue */
#endif
	void * data;                     /**< Application specific data */
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
	uint16_t renamedCategories;                           /**< Renamed categories */
	char names[PDB_CATEGORIES_STD_QTY][PDB_CATEGORY_LEN]; /**< Array with categories names */
	uint8_t ids[PDB_CATEGORIES_STD_QTY];                  /**< Array with categories IDs */
	uint8_t lastUniqueId;                                 /**< Last unique category ID (usually 0x0f) */
	uint8_t padding;                                      /**< Zero byte for padding */
};
typedef struct PDBCategories PDBCategories;

/**
   Standardized data from PDB file.
*/
struct PDB
{
	char dbname[PDB_DBNAME_LEN];   /**< Null-terminated string with database
									  name */
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
	uint32_t nextRecordListOffset; /**< Offset to next record list.
									  Should be 0x00000000 */
	uint16_t recordsQty;           /**< Qty of records */
	struct RecordQueue records;    /**< Record list */
	uint16_t recordListPadding;    /**< Padding bytes after record list */
	PDBCategories * categories;    /**< Categories from PDB file. May be NULL
									  if not applicable */
};
typedef struct PDB PDB;


/**
   \defgroup pdb_file_ops Operations with PDB files

   There are a set of functions to operate with *.pdb files and to
   read/store it in PDB structure.

   @{
*/

/**
   Opens given PDB file and returns it's descriptor.

   @param[in] path Path to file in filesystem.
   @return File descriptor or -1 on error.
*/
int pdb_open(const char * path);

/**
   Read header and other standard info from PDB file to PDB structure.

   Reads PDB file and allocate memory for header and records to fill
   it with actual data. Categories in PDB structure may be NULL if we
   do not use Palm OS categories in given PDB file.

   Allocated memory will be freed outside of this function, by
   pdb_close().

   All multi-byte numbers will be converted to host system endianess. All
   timestamps will be converted to Unix timestamps.

   @param[in] fd PDB file descriptor.
   @param[in] stdCatInfo Set to true if there is a standard Palm OS category
   information.
   @return Initialized PDB structure or NULL on error.
*/
PDB * pdb_read(const int fd, bool stdCatInfo);

/**
   Write header and other standard information to PDB file.

   All multi-byte numbers will be converted to big-endian. All
   timestamps will be converted to Mac timestamps. If there are a
   records in PDB structure with dirty flag set — this flag will be
   cleared.

   @param[in] fd File descriptor, already opened by pdb_open().
   @param[in] pdb Pointer to the PDB structure with data.
   @return Zero if write successfull, otherwise non-zero.
*/
int pdb_write(int fd, PDB * pdb);

/**
   Close opened PDB file.

   @param[in] fd Opened PDB-file descriptor.
*/
void pdb_close(int fd);

/**
   Free internals of PDB structure allocated in pdb_read().

   @param[in] pdb PDB structure to free.
*/
void pdb_free(PDB * pdb);

/**
   @}
*/


/**
   \defgroup pdb_record_ops Operate with records from PDB structure

   There are set of functions to create or delete record in PDB
   structure.

   @{
*/

/**
   Add new record to the end of record list.

   Record unique ID will be generated inside this function.

   @param[in] pdb Pointer to PDB structure.
   @param[in] offset Offset to record's data.
   @param[in] attributes Attributes field for new record.
   @param[in] data Application specific data.
   @return Created record or NULL on error.
*/
PDBRecord * pdb_record_create(PDB * pdb, uint32_t offset, uint8_t attributes,
							  void * data);

/**
   Delete given record from the records list.

   @param[in] pdb Pointer to PDB structure.
   @param[in] uniqueRecordId Unique record ID.
   @return Zero if success or non-zero value on error.
*/
int pdb_record_delete(PDB * pdb, long uniqueRecordId);

/**
   Get unique record ID.

   @param[in] record PDB record.
   @return Unique record ID or zero on error.
*/
long pdb_record_get_unique_id(PDBRecord * record);


/**
   @}
*/


/**
   \defgroup pdb_categories_ops Operate with categories in PDB structure

   Set of functions to retrieving category data or create/delete
   categories.

   @{
*/

/**
   Returns pointer to category name by category ID.

   Category ID starts from zero to PDB_CATEGORIES_STD_LEN - 1.

   @param[in] pdb Pointer to PDB structure.
   @param[in] id Category ID. Starts from zero.
   @return Pointer to category name or NULL on error.
*/
char * pdb_category_get_name(PDB * pdb, uint8_t id);

/**
   Returns category ID by category name.

   @param[in] pdb Pointer to PDB structure.
   @param[in] name Category name.
   @return Category ID. Returns UINT8_MAX if category not found.
*/
uint8_t pdb_category_get_id(PDB * pdb, char * name);

/**
   Add new category.

   Resulting category ID starts from zero to PDB_CATEGORIES_STD_LEN - 1.
   Function will take first non-using category ID.

   If category name is > PDB_CATEGORY_LEN, then category name will be
   truncated.

   @param[in] pdb Pointer to PDB structure.
   @param[in] name Name of new category.
   @return Category ID or UINT8_MAX on error.
*/
uint8_t pdb_category_add(PDB * pdb, const char * name);

/**
   Delete existing category.

   @param[in] pdb Pointer to PDB structure.
   @param[in] id Category ID to delete. Starts from zero to
   PDB_CATEGORIES_STD_LEN - 1.
   @return Zero on successfull or non-zero on error.
*/
int pdb_category_delete(PDB * pdb, uint8_t id);

/**
   @}
*/

#endif
