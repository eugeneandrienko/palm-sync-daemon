#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "log.h"
#include "pdb.h"


/* Record list offset */
#define PDB_RECORD_LIST_OFFSET         0x0048
/* Seconds between start of Mac epoch and start of Unix epoch */
#define PDB_MAC_UNIX_EPOCH_START_DIFF  2082844800
/* Record list header size */
#define PDB_RECORD_LIST_HEADER_SIZE    6


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


int pdb_read(const char * path, int stdCatInfo, PDB ** ppdb)
{
	int fd = -1;

	if(path == NULL)
	{
		log_write(LOG_ERR, "Got NULL path to PDB file");
		return -1;
	}
	if((fd = open(path, O_RDWR, 0644)) == -1)
	{
		log_write(LOG_ERR, "Cannot open %s PDB file: %s", path, strerror(errno));
		return -1;
	}

	if((*ppdb = calloc(1, sizeof(PDB))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for PDB structure: %s",
				  strerror(errno));
		/* Мне похуй; я так чувствую */
		goto pdb_read_error;
	}

	PDB * pdb = *ppdb;
	TAILQ_INIT(&pdb->records);
	pdb->categories = NULL;

	if(read(fd, pdb->dbname, PDB_DBNAME_LEN) != PDB_DBNAME_LEN)
	{
		log_write(LOG_ERR, "Cannot read database name from PDB header: %s", strerror(errno));
		free(pdb);
		goto pdb_read_error;
	}

	int result = 0;
	result += _read16_field(fd, &pdb->attributes, "attributes");
	result += _read16_field(fd, &pdb->version, "version");
	result += _read32_field(fd, &pdb->ctime, "creation datetime");
	result += _read32_field(fd, &pdb->mtime, "modification datetime");
	result += _read32_field(fd, &pdb->btime, "last backup datetime");
	result += _read32_field(fd, &pdb->modificationNumber, "modification number");
	result += _read32_field(fd, &pdb->appInfoOffset, "application info offset");
	result += _read32_field(fd, &pdb->sortInfoOffset, "sort info offset");
	result += _read32_field(fd, &pdb->databaseTypeID, "database type ID");
	result += _read32_field(fd, &pdb->creatorID, "creator ID");
	result += _read32_field(fd, &pdb->seed, "unique ID seed");
	result += _read32_field(fd, &pdb->nextRecordListOffset, "next record list offset");
	result += _read16_field(fd, &pdb->recordsQty, "qty of records");

	if(result)
	{
		free(pdb);
		goto pdb_read_error;
	}

	if(pdb->nextRecordListOffset != 0)
	{
		log_write(LOG_ERR, "Malformed PDB file, next record list offset = %d",
				  pdb->nextRecordListOffset);
		free(pdb);
		goto pdb_read_error;
	}

	if(pdb->recordsQty > 0 &&
	   _read_record_list(fd, pdb->recordsQty, &pdb->records))
	{
		log_write(LOG_ERR, "Cannot read records list");
		pdb_free(fd, pdb);
		goto pdb_read_error;
	}
	else if(pdb->recordsQty > 0)
	{
		pdb->recordListPadding = 0x0000;
	}

	/* Read standard Palm OS categories info if necessary */
	if(pdb->appInfoOffset && stdCatInfo)
	{
		if(pdb->appInfoOffset != lseek(fd, pdb->appInfoOffset, SEEK_SET))
		{
			log_write(LOG_ERR, "Failed to reposition to application info in PDB file %s: %s",
					  path, strerror(errno));
			pdb_free(fd, pdb);
			goto pdb_read_error;
		}
		if(_read_categories(fd, &pdb->categories))
		{
			log_write(LOG_ERR, "Cannot read categories from application info (PDB file: %s)",
					  path);;
			pdb_free(fd, pdb);
			goto pdb_read_error;
		}
	}

	/* Fix some fields */
	pdb->ctime = _time_palm_to_unix(pdb->ctime);
	pdb->mtime = _time_palm_to_unix(pdb->mtime);
	pdb->btime = _time_palm_to_unix(pdb->btime);

	return fd;
pdb_read_error:
	if(close(fd) == -1 && errno != EBADF)
	{
		log_write(LOG_ERR, "Cannot close PDB file %s, error message: %s",
				  path, strerror(errno));
	}
	return -1;
}

int pdb_write(int fd, PDB * pdb)
{
	/* Check and fix some fields if necessary */
	pdb->ctime = _time_unix_to_palm(pdb->ctime);
	pdb->mtime = _time_unix_to_palm(pdb->mtime);
	pdb->btime = _time_unix_to_palm(pdb->btime);

	if(pdb->nextRecordListOffset != 0)
	{
		log_write(LOG_ERR, "Malformed PDB data, next record list offset = %d",
				  pdb->nextRecordListOffset);
		goto pdb_write_error;
	}

	/* Check records qty */
	uint16_t recordsQty = 0;
	if(!TAILQ_EMPTY(&pdb->records))
	{
		PDBRecord * record;
		TAILQ_FOREACH(record, &pdb->records, pointers)
		{
			recordsQty++;
		}
	}
	if(recordsQty != pdb->recordsQty)
	{
		log_write(LOG_NOTICE, "Fix records qty. Old: %d, new: %d",
				  pdb->recordsQty, recordsQty);
		pdb->recordsQty = recordsQty;
	}

	/* Check offset to application info */
	if(pdb->categories != NULL)
	{
		uint32_t appInfoOffset = PDB_RECORD_LIST_OFFSET +
			PDB_RECORD_LIST_HEADER_SIZE +
			pdb->recordsQty * PDB_RECORD_ITEM_SIZE +
			sizeof(pdb->recordListPadding);
		if(appInfoOffset != pdb->appInfoOffset)
		{
			log_write(LOG_NOTICE, "Fix application info offset. Old: %lu, new: %lu",
					  pdb->appInfoOffset, appInfoOffset);
			pdb->appInfoOffset = appInfoOffset;
		}
	}

	/* Start file writing */
	if(lseek(fd, 0, SEEK_SET) != 0)
	{
		log_write(LOG_ERR, "Cannot go to start of the PDB file for writing: %s",
				  strerror(errno));
		goto pdb_write_error;
	}

	if(write(fd, pdb->dbname, PDB_DBNAME_LEN) != PDB_DBNAME_LEN)
	{
		log_write(LOG_ERR, "Cannot write database name to PDB header: %s", strerror(errno));
		goto pdb_write_error;
	}

	int result = 0;
	result += _write16_field(fd, &pdb->attributes, "attributes");
	result += _write16_field(fd, &pdb->version, "version");
	result += _write32_field(fd, &pdb->ctime, "creation datetime");
	result += _write32_field(fd, &pdb->mtime, "modification datetime");
	result += _write32_field(fd, &pdb->btime, "last backup datetime");
	result += _write32_field(fd, &pdb->modificationNumber, "modification number");
	result += _write32_field(fd, &pdb->appInfoOffset, "application info offset");
	result += _write32_field(fd, &pdb->sortInfoOffset, "sort info offset");
	result += _write32_field(fd, &pdb->databaseTypeID, "database type ID");
	result += _write32_field(fd, &pdb->creatorID, "creator ID");
	result += _write32_field(fd, &pdb->seed, "unique ID seed");
	result += _write32_field(fd, &pdb->nextRecordListOffset, "next record list offset");
	result += _write16_field(fd, &pdb->recordsQty, "qty of records");
	if(result)
	{
		goto pdb_write_error;
	}

    /* Write records list if it is not empty */
	if(pdb->recordsQty > 0 && _write_record_list(fd, &pdb->records))
	{
		log_write(LOG_ERR, "Cannot write records list");
		goto pdb_write_error;
	}
	if(_write16_field(fd, &pdb->recordListPadding, "record list padding bytes"))
	{
		log_write(LOG_ERR, "Cannot write padding bytes after record list");
		goto pdb_write_error;
	}

	if(pdb->categories != NULL)
	{
		if(pdb->appInfoOffset != lseek(fd, pdb->appInfoOffset, SEEK_SET))
		{
			log_write(LOG_ERR, "Failed to reposition to application info in PDB file: %s",
					  strerror(errno));
			goto pdb_write_error;
		}
		if(_write_categories(fd, pdb->categories))
		{
			log_write(LOG_ERR, "Cannot write categories to application info");
			goto pdb_write_error;
		}
	}

	return 0;
pdb_write_error:
	pdb_free(fd, pdb);
	return -1;
}

void pdb_free(int fd, PDB * pdb)
{
	if(close(fd) == -1)
	{
		log_write(LOG_DEBUG, "Failed to close file descriptor: %s",
				  strerror(errno));
	}

	if(pdb == NULL)
	{
		return;
	}
	struct PDBRecord * record1 = TAILQ_FIRST(&pdb->records);
	struct PDBRecord * record2;
	while(record1 != NULL)
	{
		record2 = TAILQ_NEXT(record1, pointers);
		if(record1->data != NULL)
		{
			free(record1->data);
		}
		free(record1);
		record1 = record2;
	}
	TAILQ_INIT(&pdb->records);

	if(pdb->categories != NULL)
	{
		free(pdb->categories);
	}
	free(pdb);
}

/* Operations with records */

PDBRecord * pdb_record_create(PDB * pdb, uint32_t offset, uint8_t attributes)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure (%s)", "pdb_record_add");
		return NULL;
	}

	PDBRecord * newRecord;
	if((newRecord = calloc(1, sizeof(PDBRecord))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new PDB record: %s",
				  strerror(errno));
		return NULL;
	}

	newRecord->offset = offset;
	newRecord->attributes = attributes;
	newRecord->hash = 0;
	newRecord->data = NULL;
	srand(time(NULL) + getpid());
	newRecord->id[0] = (uint8_t)(rand() & 0x000000ff);
	newRecord->id[1] = (uint8_t)(rand() & 0x000000ff);
	newRecord->id[2] = (uint8_t)(rand() & 0x000000ff);

	if(TAILQ_EMPTY(&pdb->records))
	{
		TAILQ_INSERT_HEAD(&pdb->records, newRecord, pointers);
	}
	else
	{
		TAILQ_INSERT_TAIL(&pdb->records, newRecord, pointers);
	}

	pdb->appInfoOffset += pdb->appInfoOffset != 0 ? PDB_RECORD_ITEM_SIZE : 0;
	pdb->sortInfoOffset += pdb->sortInfoOffset != 0 ? PDB_RECORD_ITEM_SIZE : 0;
	pdb->recordsQty++;
	return newRecord;
}

int pdb_record_delete(PDB * pdb, PDBRecord * record)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDBFile structure (%s)", "pdb_record_delete");
		return -1;
	}
	if(record == NULL)
	{
		log_write(LOG_ERR, "NULL record (%s)", "pdb_record_delete");
		return -1;
	}

	if(TAILQ_EMPTY(&pdb->records))
	{
		log_write(LOG_WARNING, "Empty queue, nothing to delete (%s)", "pdb_record_delete");
		return -1;
	}
	if(record->data != NULL)
	{
		free(record->data);
	}
	TAILQ_REMOVE(&pdb->records, record, pointers);
	free(record);

	pdb->appInfoOffset -= pdb->appInfoOffset != 0 ? PDB_RECORD_ITEM_SIZE : 0;
	pdb->sortInfoOffset -= pdb->sortInfoOffset != 0 ? PDB_RECORD_ITEM_SIZE : 0;
	pdb->recordsQty--;
	return 0;
}

/* Operations with categories */

char * pdb_category_get_name(PDB * pdb, uint8_t id)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure (%s)", "pdb_category_get");
		return NULL;
	}
	if(id >= PDB_CATEGORIES_STD_QTY)
	{
		log_write(LOG_ERR, "Wrong category id - cannot be greater than %d",
				  PDB_CATEGORIES_STD_QTY - 1);
		return NULL;
	}

	return pdb->categories->names[id];
}

/**
   Entry for array of categories.

   Used to sort categories by name and search for desired category.
*/
struct SortedCategory
{
	uint8_t * id; /**< Pointer to category ID */
	char * name;  /**< Pointer to category name */
};

int __compare_categories(const void * cat1, const void * cat2)
{
	return strcmp(
		((const struct SortedCategory *)cat1)->name,
		((const struct SortedCategory *)cat2)->name);
}

char pdb_category_get_id(PDB * pdb, char * name)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure (%s)", "pdb_category_get_id");
		return -1;
	}
	if(name == NULL)
	{
		log_write(LOG_ERR, "NULL category name to search (%s)", "pdb_category_get_id");
		return -1;
	}

	struct SortedCategory sortedCategories[PDB_CATEGORIES_STD_QTY];

	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
	{
		sortedCategories[i].id = &pdb->categories->ids[i];
		sortedCategories[i].name = pdb->categories->names[i];
	}

	qsort(&sortedCategories, PDB_CATEGORIES_STD_QTY, sizeof(struct SortedCategory),
		  __compare_categories);
	struct SortedCategory searchFor = {NULL, name};
	struct SortedCategory * searchResult = bsearch(
		&searchFor, &sortedCategories, PDB_CATEGORIES_STD_QTY,
		sizeof(struct SortedCategory), __compare_categories);
	return searchResult != NULL ? *searchResult->id : -1;
}

int pdb_category_add(PDB * pdb, const char * name)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure (%s)", "pdb_category_add");
		return -1;
	}
	if(name == NULL)
	{
		log_write(LOG_ERR, "NULL pointer to new name (%s)", "pdb_category_add");
		return -1;
	}
	size_t length = strlen(name);
	if(length == 0)
	{
		log_write(LOG_ERR, "Empty new name (%s)", "pdb_category_add");
		return -1;
	}
	else if(length > PDB_CATEGORY_LEN - 1)
	{
		log_write(LOG_ERR, "New name is too long: it has %d symbols", length);
		log_write(LOG_ERR, "But in PalmOS allowed only %d symbols", PDB_CATEGORY_LEN);
		return -1;
	}

	unsigned int freeId = 0;
	while(freeId < PDB_CATEGORY_LEN && pdb->categories->names[freeId][0] != '\0')
	{
		freeId++;
	}
	if(freeId >= PDB_CATEGORY_LEN)
	{
		log_write(LOG_WARNING, "No space to add new category - all IDs is in use");
		return -1;
	}

	memcpy(pdb->categories->names[freeId], name, length);

	pdb->categories->names[freeId][length] = '\0';
	pdb->categories->ids[freeId] = freeId;
	return 0;
}

int pdb_category_delete(PDB * pdb, uint8_t id)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure (%s)", "pdb_category_delete");
		return -1;
	}
	if(id >= PDB_CATEGORIES_STD_QTY)
	{
		log_write(LOG_ERR, "Wrong category id - cannot be greater than %d",
				  PDB_CATEGORIES_STD_QTY - 1);
		return -1;
	}

	explicit_bzero(pdb->categories->names[id], sizeof(char) * PDB_CATEGORY_LEN);
	pdb->categories->ids[id] = 0;
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
		PDBRecord * record;
		if((record = calloc(1, sizeof(PDBRecord))) == NULL)
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
		record->hash = 0;
		record->data = NULL;

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
	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
	{
		if(read(fd, &((*categories)->names[i]), PDB_CATEGORY_LEN) != PDB_CATEGORY_LEN)
		{
			log_write(LOG_ERR, "Cannot read category #%d name: %s", i,
					  strerror(errno));
			return -1;
		}
	}
	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
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
	if(log_is_debug())
	{
		off_t offset = lseek(fd, 0, SEEK_CUR);
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
	if(log_is_debug())
	{
		off_t offset = lseek(fd, 0, SEEK_CUR);
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
	if(log_is_debug())
	{
		off_t offset = lseek(fd, 0, SEEK_CUR);
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
	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
	{
		if(write(fd, &(categories->names[i]), PDB_CATEGORY_LEN) != PDB_CATEGORY_LEN)
		{
			log_write(LOG_ERR, "Cannot write category #%d name: %s", i,
					  strerror(errno));
			return -1;
		}
	}
	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
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
