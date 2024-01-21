/**
   @brief Process PDB standard structures
   @author Eugene Andrienko
*/

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "log.h"
#include "pdb.h"


#define PDB_RECORD_LIST_OFFSET         0x0048     /** Record list offset */
#define PDB_RECORD_SIZE                8          /** Record item size. 32 + 8 + 3 * 8 = 64 bits / 8 bytes */
#define PDB_MAC_UNIX_EPOCH_START_DIFF  2082844800 /** Seconds between start of Mac epoch and start of Unix epoch */
#define PDB_RECORD_LIST_HEADER_SIZE    6          /** Record list header size */


static int _read8_field(int fd, uint8_t * buf, char * description);
static int _read16_field(int fd, uint16_t * buf, char * description);
static int _read32_field(int fd, uint32_t * buf, char * description);
static int _read_record_list(int fd, int qty, struct RecordQueue * records);
static int _read_categories(int fd, PDBCategories ** categories);

static int _write8_field(int fd, uint8_t * buf, char * description);
static int _write16_field(int fd, uint16_t * buf, char * description);
static int _write32_field(int fd, uint32_t * buf, char * description);
static int _write_record_list(int fd, struct RecordQueue * records);
static int _write_categories(int fd, PDBCategories * categories);

static time_t _time_palm_to_unix(uint32_t time);
static uint32_t _time_unix_to_palm(time_t time);


/**
   Open PDB file.

   @param path Path to PDB file
   @return PDB file descriptor or -1 if error
*/
int pdb_open(const char * path)
{
	int fd = 0;
	if((fd = open(path, O_RDWR, 0644)) == -1)
	{
		log_write(LOG_ERR, "Cannot open %s PDB file: %s", path, strerror(errno));
		return -1;
	}
	return fd;
}

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
int pdb_read(int fd, PDBFile * pdbFile, int stdCatInfo)
{
	TAILQ_INIT(&pdbFile->records);

	if(read(fd, pdbFile->dbname, PDB_DBNAME_LEN) != PDB_DBNAME_LEN)
	{
		log_write(LOG_ERR, "Cannot read database name from PDB header: %s", strerror(errno));
		return -1;
	}

	int result = 0;
	result += _read16_field(fd, &pdbFile->attributes, "attributes");
	result += _read16_field(fd, &pdbFile->version, "version");
	result += _read32_field(fd, &pdbFile->ctime, "creation datetime");
	result += _read32_field(fd, &pdbFile->mtime, "modification datetime");
	result += _read32_field(fd, &pdbFile->btime, "last backup datetime");
	result += _read32_field(fd, &pdbFile->modificationNumber, "modification number");
	result += _read32_field(fd, &pdbFile->appInfoOffset, "application info offset");
	result += _read32_field(fd, &pdbFile->sortInfoOffset, "sort info offset");
	result += _read32_field(fd, &pdbFile->databaseTypeID, "database type ID");
	result += _read32_field(fd, &pdbFile->creatorID, "creator ID");
	result += _read32_field(fd, &pdbFile->seed, "unique ID seed");
	result += _read32_field(fd, &pdbFile->nextRecordListOffset, "next record list offset");
	result += _read16_field(fd, &pdbFile->recordsQty, "qty of records");

	if(result)
	{
		return -1;
	}

	if(pdbFile->nextRecordListOffset != 0)
	{
		log_write(LOG_ERR, "Malformed PDB file, next record list offset = %d",
				  pdbFile->nextRecordListOffset);
		return -1;
	}

	if(pdbFile->recordsQty > 0 &&
	   _read_record_list(fd, pdbFile->recordsQty, &pdbFile->records))
	{
		log_write(LOG_ERR, "Cannot read records list");
		return -1;
	}
	else if(pdbFile->recordsQty > 0)
	{
		pdbFile->recordListPadding = 0x0000;
	}

	if(pdbFile->appInfoOffset && stdCatInfo)
	{
		if(pdbFile->appInfoOffset != lseek(fd, pdbFile->appInfoOffset, SEEK_SET))
		{
			log_write(LOG_ERR, "Failed to reposition to application info in PDB file: %s",
					  strerror(errno));
			return -1;
		}
		if(_read_categories(fd, &pdbFile->categories))
		{
			log_write(LOG_ERR, "Cannot read categories from application info");
			return -1;
		}
	}

	/* Fix some fields */
	pdbFile->ctime = _time_palm_to_unix(pdbFile->ctime);
	pdbFile->mtime = _time_palm_to_unix(pdbFile->mtime);
	pdbFile->btime = _time_palm_to_unix(pdbFile->btime);

	return 0;
}

/**
   Write header and other standard info to PDB file.

   All multi-byte numbers will be converted to right endianess by libpisock. All
   timestamps will be converted to Mac timestamps.

   @param fd File descriptor
   @param pdbFile Pointer to filled PDBFile structure
   @return Zero if write successfull, otherwise non-zero
*/
int pdb_write(int fd, PDBFile * pdbFile)
{
	/* Check and fix some fields if necessary */
	pdbFile->ctime = _time_unix_to_palm(pdbFile->ctime);
	pdbFile->mtime = _time_unix_to_palm(pdbFile->mtime);
	pdbFile->btime = _time_unix_to_palm(pdbFile->btime);

	if(pdbFile->categories != NULL)
	{
		PDBRecord * record;
		uint32_t appInfoOffset = PDB_RECORD_LIST_OFFSET + PDB_RECORD_LIST_HEADER_SIZE;
		TAILQ_FOREACH(record, &pdbFile->records, pointers)
		{
			appInfoOffset += PDB_RECORD_SIZE;
		}
		appInfoOffset += sizeof(pdbFile->recordListPadding);
		if(appInfoOffset != pdbFile->appInfoOffset)
		{
			log_write(LOG_NOTICE, "Fix application info offset. Old: %lu, new: %lu",
					  pdbFile->appInfoOffset, appInfoOffset);
			pdbFile->appInfoOffset = appInfoOffset;
		}
	}

	/* Start file writing */
	if(lseek(fd, 0, SEEK_SET) != 0)
	{
		log_write(LOG_ERR, "Cannot go to start of the PDB file for writing: %s",
				  strerror(errno));
		return -1;
	}

	if(write(fd, pdbFile->dbname, PDB_DBNAME_LEN) != PDB_DBNAME_LEN)
	{
		log_write(LOG_ERR, "Cannot write database name to PDB header: %s", strerror(errno));
		return -1;
	}

	int result = 0;
	result += _write16_field(fd, &pdbFile->attributes, "attributes");
	result += _write16_field(fd, &pdbFile->version, "version");
	result += _write32_field(fd, &pdbFile->ctime, "creation datetime");
	result += _write32_field(fd, &pdbFile->mtime, "modification datetime");
	result += _write32_field(fd, &pdbFile->btime, "last backup datetime");
	result += _write32_field(fd, &pdbFile->modificationNumber, "modification number");
	result += _write32_field(fd, &pdbFile->appInfoOffset, "application info offset");
	result += _write32_field(fd, &pdbFile->sortInfoOffset, "sort info offset");
	result += _write32_field(fd, &pdbFile->databaseTypeID, "database type ID");
	result += _write32_field(fd, &pdbFile->creatorID, "creator ID");
	result += _write32_field(fd, &pdbFile->seed, "unique ID seed");
	result += _write32_field(fd, &pdbFile->nextRecordListOffset, "next record list offset");
	result += _write16_field(fd, &pdbFile->recordsQty, "qty of records");

	if(result)
	{
		return -1;
	}

	if(pdbFile->nextRecordListOffset != 0)
	{
		log_write(LOG_ERR, "Malformed PDB data, next record list offset = %d",
				  pdbFile->nextRecordListOffset);
		return -1;
	}

	if(pdbFile->recordsQty > 0 && _write_record_list(fd, &pdbFile->records))
	{
		log_write(LOG_ERR, "Cannot write records list");
		return -1;
	}
	else if(pdbFile->recordsQty > 0 &&
			_write16_field(fd, &pdbFile->recordListPadding, "record list padding bytes"))
	{
		log_write(LOG_ERR, "Cannot write padding bytes after record list");
		return -1;
	}

	if(pdbFile->categories != NULL)
	{
		if(pdbFile->appInfoOffset != lseek(fd, pdbFile->appInfoOffset, SEEK_SET))
		{
			log_write(LOG_ERR, "Failed to reposition to application info in PDB file: %s",
					  strerror(errno));
			return -1;
		}
		if(_write_categories(fd, pdbFile->categories))
		{
			log_write(LOG_ERR, "Cannot write categories to application info");
			return -1;
		}
	}

	return 0;
}

/**
   Close opened PDB file

   @param fd File descriptor
*/
void pdb_close(int fd)
{
	if(close(fd) == -1)
	{
		log_write(LOG_ERR, "Cannot close PDB file: %s", strerror(errno));
	}
}

/**
   Free allocated memory for standard PDB file structures.

   @param pdbFile PDBFile structure to free
*/
void pdb_free(PDBFile * pdbFile)
{
	if(pdbFile == NULL)
	{
		return;
	}
	struct PDBRecord * record1 = TAILQ_FIRST(&pdbFile->records);
	struct PDBRecord * record2;
	while(record1 != NULL)
	{
		record2 = TAILQ_NEXT(record1, pointers);
		free(record1);
		record1 = record2;
	}
	TAILQ_INIT(&pdbFile->records);

	if(pdbFile->categories != NULL)
	{
		free(pdbFile->categories);
	}
}

/**
   Add new record to the end of the list

   @param pdbFile Pointer to PDBFile structure
   @param record New record to insert
   @return 0 if success or non-zero value on error
*/
int pdb_record_add(PDBFile * pdbFile, PDBRecord record)
{
	PDBRecord * newRecord = calloc(1, sizeof(PDBRecord));

	if(pdbFile == NULL)
	{
		log_write(LOG_ERR, "NULL PDBFile structure (%s)", "pdb_record_add");
		return -1;
	}

	if(newRecord == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new PDB record: %s",
				  strerror(errno));
		return -1;
	}

	newRecord->offset = record.offset;
	newRecord->attributes = record.attributes;
	srand(time(NULL) + getpid());
	newRecord->id[0] = (uint8_t)(rand() & 0x000000ff);
	newRecord->id[1] = (uint8_t)(rand() & 0x000000ff);
	newRecord->id[2] = (uint8_t)(rand() & 0x000000ff);

	if(TAILQ_EMPTY(&pdbFile->records))
	{
		TAILQ_INSERT_HEAD(&pdbFile->records, newRecord, pointers);
	}
	else
	{
		TAILQ_INSERT_TAIL(&pdbFile->records, newRecord, pointers);
	}

	pdbFile->appInfoOffset += pdbFile->appInfoOffset != 0 ? PDB_RECORD_SIZE : 0;
	pdbFile->sortInfoOffset += pdbFile->sortInfoOffset != 0 ? PDB_RECORD_SIZE : 0;
	pdbFile->recordsQty++;
	return 0;
}

/**
   Returns pointer to PDBRecord from records list.

   Get Nth record from record list where N is index.

   @param pdbFile Pointer to PDBFile structure
   @param index Number of record to get. Starts from zero.
   @return Pointer to PDBRecord or NULL on error
*/
PDBRecord * pdb_record_get(PDBFile * pdbFile, uint16_t index)
{
	if(pdbFile == NULL)
	{
		log_write(LOG_ERR, "NULL PDBFile structure (%s)", "pdb_record_get");
		return NULL;
	}

	if(index > pdbFile->recordsQty)
	{
		log_write(LOG_ERR, "Wrong index of record %d! Size of record list: %d.",
				  index, pdbFile->recordsQty);
		return NULL;
	}

	PDBRecord * record = TAILQ_FIRST(&pdbFile->records);
	if(index == pdbFile->recordsQty - 1)
	{
		record = TAILQ_LAST(&pdbFile->records, RecordQueue);
	}
	else if(index > 0)
	{
		for(int i = 0; i < index; i++)
		{
			record = TAILQ_NEXT(record, pointers);
		}
	}
	return record;
}

/**
   Delete given record from the list

   @param pdbFile Pointer to PDBFile structure
   @param record Record to delete
   @return 0 if success or non-zero value on error
*/
int pdb_record_delete(PDBFile * pdbFile, PDBRecord * record)
{
	if(pdbFile == NULL)
	{
		log_write(LOG_ERR, "NULL PDBFile structure (%s)", "pdb_record_delete");
		return -1;
	}
	if(record == NULL)
	{
		log_write(LOG_ERR, "NULL record (%s)", "pdb_record_delete");
		return -1;
	}
	if(TAILQ_EMPTY(&pdbFile->records))
	{
		log_write(LOG_WARNING, "Empty queue, nothing to delete (%s)", "pdb_record_delete");
		return -1;
	}

	TAILQ_REMOVE(&pdbFile->records, record, pointers);

	pdbFile->appInfoOffset -= pdbFile->appInfoOffset != 0 ? PDB_RECORD_SIZE : 0;
	pdbFile->sortInfoOffset -= pdbFile->sortInfoOffset != 0 ? PDB_RECORD_SIZE : 0;
	pdbFile->recordsQty--;
	return 0;
}

/**
   Returns pointer to category name

   @param pdbFile Pointer to PDBFile structure
   @param id Category ID. Starts from zero.
   @return Pointer to category name or NULL on error
*/
char * pdb_category_get(PDBFile * pdbFile, uint8_t id)
{
	if(pdbFile == NULL)
	{
		log_write(LOG_ERR, "NULL PDBFile structure (%s)", "pdb_category_get");
		return NULL;
	}
	if(id >= PDB_CATEGORIES_STD_LEN)
	{
		log_write(LOG_ERR, "Wrong category id - cannot be greater than %d",
				  PDB_CATEGORIES_STD_LEN - 1);
		return NULL;
	}

	return pdbFile->categories->names[id];
}

/**
   Add new category

   @param pdbFile Pointer to PDBFile structure
   @param id New category ID. Starts from zero.
   @param name Name of new category
   @return Zero on successfull or non-zero on error
*/
int pdb_category_add(PDBFile * pdbFile, uint8_t id, char * name)
{
	if(pdbFile == NULL)
	{
		log_write(LOG_ERR, "NULL PDBFile structure (%s)", "pdb_category_add");
		return -1;
	}
	if(id >= PDB_CATEGORIES_STD_LEN)
	{
		log_write(LOG_ERR, "Wrong category id - cannot be greater than %d",
				  PDB_CATEGORIES_STD_LEN - 1);
		return -1;
	}
	if(name == NULL)
	{
		log_write(LOG_ERR, "NULL pointer to new name (%s)", "pdb_category_add");
		return -1;
	}
	if(strlen(name) == 0)
	{
		log_write(LOG_ERR, "Empty new name (%s)", "pdb_category_add");
		return -1;
	}

	size_t length = strlen(name);
	if(length > PDB_CATEGORY_LEN - 1)
	{
		memcpy(pdbFile->categories->names[id], name, PDB_CATEGORY_LEN - 1);
		pdbFile->categories->names[id][PDB_CATEGORY_LEN - 1] = '\0';
	}
	else
	{
		memcpy(pdbFile->categories->names[id], name, length);
		pdbFile->categories->names[id][length] = '\0';
	}
	pdbFile->categories->ids[id] = id;
	return 0;
}

/**
   Edit existsing category.

   @param category Pointer to category name
   @param newName New category name
   @param length Length of new category name
   @return Zero on successfull or non-zero on error
*/
int pdb_category_edit(char * category, char * newName, size_t length)
{
	if(category == NULL)
	{
		log_write(LOG_ERR, "Got NULL category - cannot edit");
		return -1;
	}
	if(newName == NULL)
	{
		log_write(LOG_ERR, "Got NULL new category - cannot edit");
		return -1;
	}

	explicit_bzero(category, PDB_CATEGORY_LEN);
	strncpy(category, newName, length);
	return 0;
}

/**
   Delete category

   @param pdbFile Pointer to PDBFile structure
   @param id Category ID to delete. Starts from zero.
   @return Zero on successfull or non-zero on error
*/
int pdb_category_delete(PDBFile * pdbFile, uint8_t id)
{
	if(pdbFile == NULL)
	{
		log_write(LOG_ERR, "NULL PDBFile structure (%s)", "pdb_category_delete");
		return -1;
	}
	if(id >= PDB_CATEGORIES_STD_LEN)
	{
		log_write(LOG_ERR, "Wrong category id - cannot be greater than %d",
				  PDB_CATEGORIES_STD_LEN - 1);
		return -1;
	}

	explicit_bzero(pdbFile->categories->names[id], sizeof(char) * PDB_CATEGORY_LEN);
	pdbFile->categories->ids[id] = 0;
	return 0;
}

/**
   Read 8-bit unsigned value from file

   @param fd File descriptor
   @param buf Buffer, 8-bit length
   @param description Field description for error string
   @return 0 on success and -1 on error
*/
static int _read8_field(int fd, uint8_t * buf, char * description)
{
	off_t offset = 0;
	if(log_is_debug())
	{
		offset = lseek(fd, 0, SEEK_CUR);
	}

	if(read(fd, buf, 1) != 1)
	{
		log_write(LOG_ERR, "Cannot read %s from PDB header: %s",
				  description == NULL ? "8 bit value" : description,
				  strerror(errno));
		return -1;
	}

	log_write(LOG_DEBUG, "Read %s 0x%02x from 0x%08x offset",
			  description, *buf, offset);
	return 0;
}

/**
   Read 16-bit unsigned value from file

   @param fd File descriptor
   @param buf Buffer, 16-bit length
   @param description Field description for error string
   @return 0 on success and -1 on error
*/
static int _read16_field(int fd, uint16_t * buf, char * description)
{
	off_t offset = 0;
	if(log_is_debug())
	{
		offset = lseek(fd, 0, SEEK_CUR);
	}

	if(read(fd, buf, 2) != 2)
	{
		log_write(LOG_ERR, "Cannot read %s from PDB header: %s",
				  description == NULL ? "16 bit value" : description,
				  strerror(errno));
		return -1;
	}
	*buf = be16toh(*buf);
	log_write(LOG_DEBUG, "Read %s 0x%04x from 0x%08x offset",
			  description, *buf, offset);
	return 0;
}

/**
   Read 32-bit unsigned value from file

   @param fd File descriptor
   @param buf Buffer, 32-bit length
   @param description Field description for error string
   @return 0 on success and -1 on error
*/
static int _read32_field(int fd, uint32_t * buf, char * description)
{
	off_t offset = 0;
	if(log_is_debug())
	{
		offset = lseek(fd, 0, SEEK_CUR);
	}

	if(read(fd, buf, 4) != 4)
	{
		log_write(LOG_ERR, "Cannot read %s from PDB header: %s",
				  description == NULL ? "32 bit value" : description,
				  strerror(errno));
		return -1;
	}
	*buf = be32toh(*buf);
	log_write(LOG_DEBUG, "Read %s 0x%08x from 0x%08x offset",
			  description, *buf, offset);
	return 0;
}

/**
   Read record list from PDB file

   @param fd File descriptor
   @param qty Count of records
   @param records Pointer to record queue
   @return 0 on success, -1 on error
*/
static int _read_record_list(int fd, int qty, struct RecordQueue * records)
{
	for(int i = 0; i < qty; i++)
	{
		int result = 0;
		PDBRecord * record = calloc(1, sizeof(PDBRecord));
		if(record == NULL)
		{
			log_write(LOG_ERR, "Cannot allocate memory for PDB record: %s",
					  strerror(errno));
			return -1;
		}

		result += _read32_field(fd, &record->offset, "record offset");
		result += _read8_field(fd, &record->attributes, "record attributes");
		result += _read8_field(fd, &(record->id[0]), "record ID 1st byte");
		result += _read8_field(fd, &(record->id[1]), "record ID 2nd byte");
		result += _read8_field(fd, &(record->id[2]), "record ID 3rd byte");
		if(result)
		{
			log_write(LOG_ERR, "Cannot read record item");
			free(record);
			return -1;
		}

		if(TAILQ_EMPTY(records))
		{
			TAILQ_INSERT_HEAD(records, record, pointers);
		}
		else
		{
			TAILQ_INSERT_TAIL(records, record, pointers);
		}
	}
	return 0;
}

/**
   Read standard Palm OS category information from application info block.

   @param fd File descriptor
   @param categories Pointer to pointer to categories structure
   @return 0 on success and non-zero on error
*/
static int _read_categories(int fd, PDBCategories ** categories)
{
	if((*categories = calloc(1, sizeof(PDBCategories))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for Palm OS categories: %s",
				  strerror(errno));
		return -1;
	}

	int result = 0;
	result += _read16_field(fd, &((*categories)->renamedCategories), "renamed categories");
	for(int i = 0; i < PDB_CATEGORIES_STD_LEN; i++)
	{
		if(read(fd, &((*categories)->names[i]), PDB_CATEGORY_LEN) != PDB_CATEGORY_LEN)
		{
			log_write(LOG_ERR, "Cannot read category #%d name: %s", i,
					  strerror(errno));
			return -1;
		}
	}
	for(int i = 0; i < PDB_CATEGORIES_STD_LEN; i++)
	{
		result += _read8_field(fd, &((*categories)->ids[i]), "category id");
	}
	result += _read8_field(fd, &((*categories)->lastUniqueId), "category last unique id");
	result += _read8_field(fd, &((*categories)->padding), "category padding");

	if(result)
	{
		log_write(LOG_ERR, "Failed to read categories from application info");
		return -1;
	}
	if((*categories)->lastUniqueId != 0x0f &&
	   (*categories)->padding != 0x00)
	{
		log_write(LOG_ERR, "Malformed Palm OS category information in application info block");
		return -1;
	}

	return 0;
}

/**
   Write 8-bit unsigned value to file

   @param fd File descriptor
   @param buf Buffer, 8-bit length
   @param description Field description for error string
   @return 0 on success and -1 on error
*/
static int _write8_field(int fd, uint8_t * buf, char * description)
{
	off_t offset = 0;
	if(log_is_debug())
	{
		offset = lseek(fd, 0, SEEK_CUR);
		log_write(LOG_DEBUG, "Writing %s 0x%02x to 0x%08x offset",
				  description, *buf, offset);
	}

	if(write(fd, buf, 1) != 1)
	{
		log_write(LOG_ERR, "Cannot write %s to PDB file: %s",
				  description == NULL ? "8 bit value" : description,
				  strerror(errno));
		return -1;
	}

	return 0;
}

/**
   Write 16-bit unsigned value to file

   @param fd File descriptor
   @param buf Buffer, 16-bit length
   @param description Field description for error string
   @return 0 on success and -1 on error
*/
static int _write16_field(int fd, uint16_t * buf, char * description)
{
	off_t offset = 0;
	if(log_is_debug())
	{
		offset = lseek(fd, 0, SEEK_CUR);
		log_write(LOG_DEBUG, "Writing %s 0x%04x to 0x%08x offset",
				  description, *buf, offset);
	}

	uint16_t htobe = htobe16(*buf);
	if(write(fd, &htobe, 2) != 2)
	{
		log_write(LOG_ERR, "Cannot write %s to PDB file: %s",
				  description == NULL ? "16 bit value" : description,
				  strerror(errno));
		return -1;
	}

	return 0;
}

/**
   Write 32-bit unsigned value to file

   @param fd File descriptor
   @param buf Buffer, 32-bit length
   @param description Field description for error string
   @return 0 on success and -1 on error
*/
static int _write32_field(int fd, uint32_t * buf, char * description)
{
	off_t offset = 0;
	if(log_is_debug())
	{
		offset = lseek(fd, 0, SEEK_CUR);
		log_write(LOG_DEBUG, "Writing %s 0x%08x to 0x%08x offset",
				  description, *buf, offset);
	}

	uint32_t htobe = htobe32(*buf);
	if(write(fd, &htobe, 4) != 4)
	{
		log_write(LOG_ERR, "Cannot write %s to PDB file: %s",
				  description == NULL ? "32 bit value" : description,
				  strerror(errno));
		return -1;
	}

	return 0;
}

/**
   Write record list to PDB file

   @param fd File descriptor
   @param records Queue of records
   @return 0 on success, -1 on error
*/
static int _write_record_list(int fd, struct RecordQueue * records)
{
	if(records == NULL)
	{
		log_write(LOG_ERR, "NULL records list: %s");
		return -1;
	}
	if(TAILQ_EMPTY(records))
	{
		log_write(LOG_NOTICE, "Nothing to write - record list is empty");
		return 0;
	}

	PDBRecord * record;
	TAILQ_FOREACH(record, records, pointers)
	{
		int result = 0;
		result += _write32_field(fd, &record->offset, "record offset");
		result += _write8_field(fd, &record->attributes, "record attributes");
		result += _write8_field(fd, &(record->id[0]), "record ID 1st byte");
		result += _write8_field(fd, &(record->id[1]), "record ID 2nd byte");
		result += _write8_field(fd, &(record->id[2]), "record ID 3rd byte");
		if(result)
		{
			log_write(LOG_ERR, "Cannot write record item");
			return -1;
		}
	}

	return 0;
}

/**
   Write standard Palm OS category information to application info block.

   @param fd File descriptor
   @param categories Pointer to category information
   @return 0 on success and non-zero on error
*/
static int _write_categories(int fd, PDBCategories * categories)
{
	if(categories == NULL)
	{
		log_write(LOG_ERR, "NULL categories");
		return -1;
	}

	int result = 0;
	result += _write16_field(fd, &categories->renamedCategories, "renamed categories");
	for(int i = 0; i < PDB_CATEGORIES_STD_LEN; i++)
	{
		if(write(fd, &(categories->names[i]), PDB_CATEGORY_LEN) != PDB_CATEGORY_LEN)
		{
			log_write(LOG_ERR, "Cannot write category #%d name: %s", i,
					  strerror(errno));
			return -1;
		}
	}
	for(int i = 0; i < PDB_CATEGORIES_STD_LEN; i++)
	{
		result += _write8_field(fd, &(categories->ids[i]), "category id");
	}

	categories->lastUniqueId = 0x0f;
	categories->padding = 0x00;

	result += _write8_field(fd, &categories->lastUniqueId, "category last unique id");
	result += _write8_field(fd, &categories->padding, "category padding");

	if(result)
	{
		log_write(LOG_ERR, "Failed to write categories to application info");
		return -1;
	}

	return 0;
}

/**
   Convert Palm time to Unix time.

   Palm time, like a Mac time, has start of the epoch at the
   01.01.1904 00:00:00 GMT. But Unix time started the epoch at
   the 01.01.1970 00:00:00 GMT.

   If Palm time is before start of Unix epoch - return time of Unix epoch start.

   @param time Palm time
   @return Unix time
*/
static inline time_t _time_palm_to_unix(uint32_t time)
{
	time_t result = time - PDB_MAC_UNIX_EPOCH_START_DIFF;
	return time < PDB_MAC_UNIX_EPOCH_START_DIFF ? 0 : result;
}

/**
   Convert Unix time to Palm time.

   Palm time, like a Mac time, has start of the epoch at the
   01.01.1904 00:00:00 GMT. But Unix time started the epoch at
   the 01.01.1970 00:00:00 GMT.

   @param time Unix time
   @return Palm time
*/
static inline uint32_t _time_unix_to_palm(time_t time)
{
	return time + PDB_MAC_UNIX_EPOCH_START_DIFF;
}
