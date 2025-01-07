#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#include <unistd.h>
#include "log.h"
#include "pdb/pdb.h"


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


int pdb_open(const char * path)
{
	int fd = -1;
	static bool rng_initialized = false;

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

	/* Initialize random number generator.
	   It is necessary for pdb_record_create(). */
	if(!rng_initialized)
	{
#if defined(__linux__)
		unsigned int seed;
		if(getrandom(&seed, sizeof(seed), 0) == -1)
		{
			log_write(LOG_CRIT, "Could not get random from /dev/urandom: %s",
					  strerror(errno));
			return -1;
		}
		srandom(seed);
#elif defined(__FreeBSD__)
		srandomdev();
#endif
		rng_initialized = true;
	}

	return fd;
}

PDB * pdb_read(const int fd, bool stdCatInfo)
{
	PDB * pdb;
	if((pdb = calloc(1, sizeof(PDB))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for PDB structure: %s",
				  strerror(errno));
		return NULL;
	}

	if((lseek(fd, 0, SEEK_CUR) != 0) && (lseek(fd, 0, SEEK_SET) != 0))
	{
		log_write(LOG_ERR, "Cannot rewind to the start of file: %s",
				  strerror(errno));
		return NULL;
	}

	TAILQ_INIT(&pdb->records);
	pdb->categories = NULL;

	if(read(fd, pdb->dbname, PDB_DBNAME_LEN) != PDB_DBNAME_LEN)
	{
		log_write(LOG_ERR, "Cannot read database name from PDB header: %s",
				  strerror(errno));
	    free(pdb);
		return NULL;
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
	result += _read32_field(fd, &pdb->nextRecordListOffset, "next record list "
							"offset");
	result += _read16_field(fd, &pdb->recordsQty, "qty of records");

	if(result)
	{
		free(pdb);
		return NULL;
	}

	if(pdb->nextRecordListOffset != 0)
	{
		log_write(LOG_ERR, "Malformed PDB file, next record list offset = %d",
				  pdb->nextRecordListOffset);
		free(pdb);
		return NULL;
	}

	if(pdb->recordsQty > 0 &&
	   _read_record_list(fd, pdb->recordsQty, &pdb->records))
	{
		log_write(LOG_ERR, "Cannot read records list");
		free(pdb);
		return NULL;
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
			log_write(LOG_ERR, "Failed to reposition to application info in "
					  "given PDB file: %s", strerror(errno));
			free(pdb);
			return NULL;
		}
		if(_read_categories(fd, &pdb->categories))
		{
			log_write(LOG_ERR, "Cannot read categories from application info!");
			free(pdb);
			return NULL;
		}
	}

	/* Use Unix time for these fields */
	pdb->ctime = _time_palm_to_unix(pdb->ctime);
	pdb->mtime = _time_palm_to_unix(pdb->mtime);
	pdb->btime = _time_palm_to_unix(pdb->btime);

	return pdb;
}

int pdb_write(int fd, PDB * pdb)
{
	if((lseek(fd, 0, SEEK_CUR) != 0) && (lseek(fd, 0, SEEK_SET) != 0))
	{
		log_write(LOG_ERR, "Cannot rewind to the start of file: %s",
				  strerror(errno));
		return -1;
	}

	/* Use Mac time for these fields */
	pdb->ctime = _time_unix_to_palm(pdb->ctime);
	pdb->mtime = _time_unix_to_palm(pdb->mtime);
	pdb->btime = _time_unix_to_palm(pdb->btime);

	if(pdb->nextRecordListOffset != 0)
	{
		log_write(LOG_ERR, "Malformed PDB data, next record list offset = %d",
				  pdb->nextRecordListOffset);
		return -1;
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
			log_write(LOG_NOTICE, "Fix application info offset. Old: %lu, "
					  "new: %lu", pdb->appInfoOffset, appInfoOffset);
			pdb->appInfoOffset = appInfoOffset;
		}
	}

	/* Start file writing */
	if(lseek(fd, 0, SEEK_SET) != 0)
	{
		log_write(LOG_ERR, "Cannot go to start of the PDB file for writing: %s",
				  strerror(errno));
		return -1;
	}

	if(write(fd, pdb->dbname, PDB_DBNAME_LEN) != PDB_DBNAME_LEN)
	{
		log_write(LOG_ERR, "Cannot write database name to PDB header: %s",
				  strerror(errno));
		return -1;
	}

	int result = 0;
	result += _write16_field(fd, &pdb->attributes, "attributes");
	result += _write16_field(fd, &pdb->version, "version");
	result += _write32_field(fd, &pdb->ctime, "creation datetime");
	result += _write32_field(fd, &pdb->mtime, "modification datetime");
	result += _write32_field(fd, &pdb->btime, "last backup datetime");
	result += _write32_field(fd, &pdb->modificationNumber, "modification "
							 "number");
	result += _write32_field(fd, &pdb->appInfoOffset, "application info offset");
	result += _write32_field(fd, &pdb->sortInfoOffset, "sort info offset");
	result += _write32_field(fd, &pdb->databaseTypeID, "database type ID");
	result += _write32_field(fd, &pdb->creatorID, "creator ID");
	result += _write32_field(fd, &pdb->seed, "unique ID seed");
	result += _write32_field(fd, &pdb->nextRecordListOffset, "next record list "
							 "offset");
	result += _write16_field(fd, &pdb->recordsQty, "qty of records");
	if(result)
	{
		return -1;
	}

    /* Write records list if it is not empty */
	if(pdb->recordsQty > 0 && _write_record_list(fd, &pdb->records))
	{
		log_write(LOG_ERR, "Cannot write records list");
		return -1;
	}
	if(_write16_field(fd, &pdb->recordListPadding, "record list padding bytes"))
	{
		log_write(LOG_ERR, "Cannot write padding bytes after record list");
		return -1;
	}

	if(pdb->categories != NULL)
	{
		if(pdb->appInfoOffset != lseek(fd, pdb->appInfoOffset, SEEK_SET))
		{
			log_write(LOG_ERR, "Failed to reposition to application info in "
					  "PDB file: %s", strerror(errno));
			return -1;
		}
		if(_write_categories(fd, pdb->categories))
		{
			log_write(LOG_ERR, "Cannot write categories to application info");
			return -1;
		}
	}

	return 0;
}

void pdb_close(int fd)
{
	if(close(fd) == -1)
	{
		log_write(LOG_DEBUG, "Failed to close file descriptor: %s",
				  strerror(errno));
	}
}

void pdb_free(PDB * pdb)
{
	if(pdb == NULL)
	{
		log_write(LOG_WARNING, "Got empty PDB structure - nothing to do");
		return;
	}
	PDBRecord * record1 = TAILQ_FIRST(&pdb->records);
	PDBRecord * record2;
	while(record1 != NULL)
	{
		record2 = TAILQ_NEXT(record1, pointers);
		free(record1->data);
		TAILQ_REMOVE(&pdb->records, record1, pointers);
		free(record1);
		record1 = record2;
	}

	free(pdb->categories);
	free(pdb);
}


/* Operations with records */

PDBRecord * pdb_record_create(PDB * pdb, uint32_t offset, uint8_t attributes,
							  void * data)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure in pdb_record_create");
		return NULL;
	}

	long randomId = random();
	uint8_t id[3] = {
		(uint8_t)(randomId & 0x000000ff),
		(uint8_t)((randomId & 0x0000ff00) >> 8),
		(uint8_t)((randomId & 0x00ff0000) >> 16)
	};
	return pdb_record_create_with_id(pdb, offset, attributes, id, data);
}

PDBRecord * pdb_record_create_with_id(PDB * pdb, uint32_t offset,
									  uint8_t attributes, uint8_t id[3],
									  void * data)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure in pdb_record_create");
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
	newRecord->data = data;

	newRecord->id[0] = id[0];
	newRecord->id[1] = id[1];
	newRecord->id[2] = id[2];

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

int pdb_record_delete(PDB * pdb, long uniqueRecordId)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDBFile structure in pdb_record_delete");
		return -1;
	}

	if(TAILQ_EMPTY(&pdb->records))
	{
		log_write(LOG_WARNING, "Empty queue, cannot delete record with ID=%d",
				  uniqueRecordId);
		return -1;
	}

	uint8_t recordId[3] = {0, 0, 0};
	recordId[0] = (uint8_t)(uniqueRecordId & 0x000000ff);
	recordId[1] = (uint8_t)((uniqueRecordId & 0x0000ff00) >> 8);
	recordId[2] = (uint8_t)((uniqueRecordId & 0x00ff0000) >> 16);

	PDBRecord * record = TAILQ_FIRST(&pdb->records);
	while(record != NULL)
	{
		if(record->id[0] == recordId[0] && record->id[1] == recordId[1] &&
		   record->id[2] == recordId[2])
		{
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
		else
		{
			record = TAILQ_NEXT(record, pointers);
		}
	}

	if(record == NULL)
	{
		log_write(LOG_WARNING, "Record with ID=%d not found in record list",
				  uniqueRecordId);
		return -1;
	}
	else
	{
		log_write(LOG_ERR, "Unexpected error: record with ID=%d found but not deleted",
				  uniqueRecordId);
		return 0;
	}
}

uint32_t pdb_record_get_unique_id(PDBRecord * record)
{
	return ((0x000000ff & (long)record->id[2]) << 16) |
		((0x000000ff & (long)record->id[1]) << 8) |
		(0x000000ff & (long)record->id[0]);
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
struct __SortedCategory
{
	uint8_t * id; /**< Pointer to category ID */
	char * name;  /**< Pointer to category name */
};

int __compare_categories(const void * cat1, const void * cat2)
{
	return strcmp(
		((const struct __SortedCategory *)cat1)->name,
		((const struct __SortedCategory *)cat2)->name);
}

uint8_t pdb_category_get_id(PDB * pdb, char * name)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure in pdb_category_get_id");
		return UINT8_MAX;
	}
	if(name == NULL)
	{
		log_write(LOG_ERR, "NULL category name in pdb_category_get_id");
		return UINT8_MAX;
	}

	struct __SortedCategory sortedCategories[PDB_CATEGORIES_STD_QTY];

	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
	{
		sortedCategories[i].id = &pdb->categories->ids[i];
		sortedCategories[i].name = pdb->categories->names[i];
	}

	qsort(&sortedCategories, PDB_CATEGORIES_STD_QTY,
		  sizeof(struct __SortedCategory), __compare_categories);
	struct __SortedCategory searchFor = {NULL, name};
	struct __SortedCategory * searchResult = bsearch(
		&searchFor, &sortedCategories, PDB_CATEGORIES_STD_QTY,
		sizeof(struct __SortedCategory), __compare_categories);
	return searchResult != NULL ? *searchResult->id : UINT8_MAX;
}

uint8_t pdb_category_add(PDB * pdb, const char * name)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure in pdb_category_add");
		return UINT8_MAX;
	}
	if(name == NULL)
	{
		log_write(LOG_ERR, "NULL pointer to new name in pdb_category_add");
		return UINT8_MAX;
	}

	size_t length = strlen(name);
	char * _name = (char *)name;
	if(length == 0)
	{
		log_write(LOG_ERR, "Empty new name in pdb_category_add");
		return UINT8_MAX;
	}
	else if(length > PDB_CATEGORY_LEN - 1)
	{
		log_write(LOG_WARNING, "New name is too long: it has %d symbols", length);
		log_write(LOG_WARNING, "But in PalmOS allowed only %d symbols",
				  PDB_CATEGORY_LEN);
		if((_name = calloc(PDB_CATEGORY_LEN, sizeof(char))) == NULL)
		{
			log_write(LOG_ERR, "Cannot allocate memory for truncated category. "
					  "Original category: %s", name);
			return UINT8_MAX;
		}
		strncpy(_name, name, PDB_CATEGORY_LEN - 1);
		length = PDB_CATEGORY_LEN - 1;
		log_write(LOG_WARNING, "Category was truncated to: %s", _name);
	}

	unsigned int freeId = 0;
	while(freeId < PDB_CATEGORIES_STD_QTY &&
		  pdb->categories->names[freeId][0] != '\0')
	{
		freeId++;
	}
	if(freeId >= PDB_CATEGORIES_STD_QTY)
	{
		log_write(LOG_ERR, "No space to add new category - all IDs "
				  "is in use");
		return UINT8_MAX;
	}

	memcpy(pdb->categories->names[freeId], _name, length);

	pdb->categories->names[freeId][length] = '\0';
	pdb->categories->ids[freeId] = freeId;

	if(_name != name)
	{
		free(_name);
	}
	return freeId;
}

int pdb_category_delete(PDB * pdb, uint8_t id)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "NULL PDB structure in pdb_category_delete");
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


/* Internal functions */

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

   Also, check for remaing bytes from deleted categories and prune it.

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
	result += _read16_field(fd, &((*categories)->renamedCategories),
							"renamed categories");
	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
	{
		if(read(fd, &((*categories)->names[i]),
				PDB_CATEGORY_LEN) != PDB_CATEGORY_LEN)
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
	result += _read8_field(fd, &((*categories)->lastUniqueId), "category last "
						   "unique id");
	result += _read8_field(fd, &((*categories)->padding), "category padding");

	if(result)
	{
		log_write(LOG_ERR, "Failed to read categories from application info");
		return -1;
	}
	if((*categories)->padding != 0x00)
	{
		log_write(LOG_ERR, "Malformed Palm OS category information in "
				  "application info block");
		return -1;
	}

	// Check for "garbage" categories:
	for(int i = PDB_CATEGORIES_STD_QTY - 1; i >= 0; i--)
	{
		if((*categories)->ids[i] != i && strlen((*categories)->names[i]) > 0)
		{
			log_write(LOG_WARNING, "Found garbage in categories list: id=%d, "
					  "name=%s. Removing it.", (*categories)->ids[i],
					  (*categories)->names[i]);
			(*categories)->ids[i] = 0;
			explicit_bzero((*categories)->names[i],
						   strlen((*categories)->names[i]));
		}
		else
		{
			(*categories)->lastUniqueId = i;
			break;
		}
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
	uint16_t htobe = htobe16(*buf);
	if(log_is_debug())
	{
		off_t offset = lseek(fd, 0, SEEK_CUR);
		log_write(LOG_DEBUG, "Writing %s 0x%04x (htobe: 0x%04x) to 0x%08x "
				  "offset", description, *buf, htobe, offset);
	}
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
	uint32_t htobe = htobe32(*buf);
	if(log_is_debug())
	{
		off_t offset = lseek(fd, 0, SEEK_CUR);
		log_write(LOG_DEBUG, "Writing %s 0x%08x (htobe: 0x%08x) to 0x%08x "
				  "offset", description, *buf, htobe, offset);
	}
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
   Write record list to PDB file.

   Drop changed flag. It should be set only inside Palm handheld.

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
		/* Drop "changed" flag */
		record->attributes &= ~PDB_RECORD_ATTR_DIRTY;

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
	result += _write16_field(fd, &categories->renamedCategories,
							 "renamed categories");
	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
	{
		if(write(fd, &(categories->names[i]),
				 PDB_CATEGORY_LEN) != PDB_CATEGORY_LEN)
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

	result += _write8_field(fd, &categories->lastUniqueId,
							"category last unique id");
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
