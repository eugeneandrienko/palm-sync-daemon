#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "helper.h"
#include "log.h"
#include "pdb/memos.h"

/**
   Six-byte gap between application info and memos itself.
   Should be filled by zeroes.
*/
#define SIX_BYTE_GAP 0x06


static Memo * _memos_read_memo(int fd, PDBRecord * record, PDB * pdb);
static int _memos_write_memo(int fd, Memo * memo);


int memos_open(const char * path)
{
	int fd = pdb_open(path);
	if(fd == -1)
	{
		log_write(LOG_ERR, "Cannot open %s PDB file", path);
		return -1;
	}
	return fd;
}

Memos * memos_read(int fd)
{
	Memos * memos;
	if((memos = calloc(1, sizeof(Memos))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for Memos: %s",
				  strerror(errno));
		return NULL;
	}

	if((lseek(fd, 0, SEEK_CUR) != 0) && (lseek(fd, 0, SEEK_SET) != 0))
	{
		log_write(LOG_ERR, "Cannot rewind to the start of memos file: %s",
				  strerror(errno));
		return NULL;
	}

	if((memos->_pdb = pdb_read(fd, true)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read PDB header from memos file");
		return NULL;
	}
	TAILQ_INIT(&memos->queue);

	PDBRecord * record;
	TAILQ_FOREACH(record, &memos->_pdb->records, pointers)
	{
		Memo * memo;
		if((memo = _memos_read_memo(fd, record, memos->_pdb)) == NULL)
		{
			log_write(LOG_ERR, "Error when reading Memos from file. "
					  "Offset: %x", record->offset);;
			return NULL;
		}
		if(TAILQ_EMPTY(&memos->queue))
		{
			TAILQ_INSERT_HEAD(&memos->queue, memo, pointers);
		}
		else
		{
			TAILQ_INSERT_TAIL(&memos->queue, memo, pointers);
		}
	}
	return memos;
}

int memos_write(int fd, Memos * memos)
{
	if(lseek(fd, 0, SEEK_CUR) != 0 && lseek(fd, 0, SEEK_SET) != 0)
	{
		log_write(LOG_ERR, "Cannot rewind to the start of memos file: %s",
				  strerror(errno));
		return -1;
	}

	if(pdb_write(fd, memos->_pdb))
	{
		log_write(LOG_ERR, "Cannot write header to PDB file with memos");
		return -1;
	}

	/* Fill gap between application info section and notes data (6 bytes)
	   with zeroes.
	*/
	PDBRecord * record = TAILQ_FIRST(&memos->_pdb->records);
	if(record == NULL)
	{
		log_write(LOG_ERR, "Failed to read first record from PDB structure");
		return -1;
	}
	int currentOffset = lseek(fd, 0, SEEK_CUR);
	if(currentOffset == -1 || (record->offset - currentOffset != SIX_BYTE_GAP))
	{
		if(currentOffset == -1)
		{
			log_write(LOG_ERR, "Cannot get current file position: %s",
					  strerror(errno));
		}
		log_write(LOG_ERR, "Cannot fill 6 byte gap with zeroes. "
				  "First record offset: 0x%08x. "
				  "Current offset: 0x%08x. "
				  "Diff: %d",
				  record->offset, currentOffset, record->offset - currentOffset);
		return -1;
	}
	const char sixByteGap[SIX_BYTE_GAP] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	if(write(fd, sixByteGap, SIX_BYTE_GAP) != SIX_BYTE_GAP)
	{
		log_write(LOG_ERR, "Failed to write six byte gap: %s", strerror(errno));
		return -1;
	}

	/* Writing memos to PDB file */
	Memo * memo;
	TAILQ_FOREACH(memo, &memos->queue, pointers)
	{
		if(_memos_write_memo(fd, memo))
		{
			log_write(LOG_ERR, "Failed to write memo with header \"%s\" to "
					  "file!", memo->header);
			return -1;
		}
	}

	return 0;
}

void memos_close(int fd)
{
	pdb_close(fd);
}

void memos_free(Memos * memos)
{
	if(memos == NULL)
	{
		return;
	}
	Memo * memo1 = TAILQ_FIRST(&memos->queue);
	Memo * memo2;
	while(memo1 != NULL)
	{
		memo2 = TAILQ_NEXT(memo1, pointers);
		free(memo1->header);
		free(memo1->text);
		free(memo1->category);
		memo1->_record = NULL;
		TAILQ_REMOVE(&memos->queue, memo1, pointers);
		free(memo1);
		memo1 = memo2;
	}

	pdb_free(memos->_pdb);
	free(memos);
}


/* Functions to operate with memo */

/**
   Element of array with memos. Maps memo header and pointer of memo.

   Used to sort memos by header and search for desired memo by it's header.
*/
struct __SortedMemos
{
	char * header; /**< Pointer to Memo header */
	Memo * memo;   /**< Pointer to Memo structure */
};

static int __compare_headers(const void * rec1, const void * rec2)
{
	return strcmp(
		((const struct __SortedMemos *)rec1)->header,
		((const struct __SortedMemos *)rec2)->header);
}

Memo * memos_memo_get(Memos * memos, char * header)
{
	int memosQty = memos->_pdb->recordsQty;
	struct __SortedMemos sortedMemos[memosQty];

	Memo * memo;
	unsigned short index = 0;

	TAILQ_FOREACH(memo, &memos->queue, pointers)
	{
		sortedMemos[index].header = memo->header;
		sortedMemos[index].memo = memo;
		index++;
	}

	if(memosQty != index)
	{
		log_write(LOG_ERR, "Memos count in PDB header: %d, real memos "
				  "count: %d", memosQty, index);
		return NULL;
	}

	qsort(&sortedMemos, memosQty, sizeof(struct __SortedMemos),
		  __compare_headers);
	struct __SortedMemos searchFor = {header, NULL};
	struct __SortedMemos * searchResult = bsearch(
		&searchFor, &sortedMemos, memosQty, sizeof(struct __SortedMemos),
		__compare_headers);
	return searchResult != NULL ? searchResult->memo : NULL;
}

Memo * memos_memo_add(Memos * memos, char * header, char * text,
					  char * category)
{
	if(memos == NULL)
	{
		log_write(LOG_ERR, "No memos structure - nowhere to add new memo!");
		return NULL;
	}
	if(header == NULL)
	{
		log_write(LOG_ERR, "Header of new memo is NULL! Cannot add new memo!");
		return NULL;
	}

	/* Search category ID for given category */
	if(category == NULL)
	{
		category = PDB_DEFAULT_CATEGORY;
	}
	int categoryId = pdb_category_get_id(memos->_pdb, category);
	if(categoryId == UINT8_MAX)
	{
		log_write(LOG_DEBUG, "Category with name \"%s\" not found in Memos "
				  "file!", category);
		if((categoryId = pdb_category_add(memos->_pdb, category)) == UINT8_MAX)
		{
			log_write(LOG_ERR, "Cannot add new category with name \"%s\" "
					  "to Memos file!", category);
			return NULL;
		}
	}

	/* Calculate offset for new memo: */

	/* Get last memo */
	PDBRecord * record;
	Memo * memo;
	if((record = TAILQ_LAST(&memos->_pdb->records, RecordQueue)) == NULL)
	{
		log_write(LOG_ERR, "Cannot get last memo's record from PDB header");
		return NULL;
	}
	if((memo = TAILQ_LAST(&memos->queue, MemosQueue)) == NULL)
	{
		log_write(LOG_ERR, "Cannot get last memo");
		return NULL;
	}
	if(memo->_record != record)
	{
		log_write(LOG_ERR, "Latest memo and latest PDB record doesn't match");
		return NULL;
	}
	uint32_t offset = record->offset;
	log_write(LOG_DEBUG, "Offset of the last record: 0x%08x", offset);

	/* Calculate size of last record and offset beyond it: */
	/* Header + '\n' */
	offset += strlen(memo->header) + sizeof(char);
	/* Text (if exists) + '\0' */
	offset += (memo->text != NULL ? strlen(memo->text) : 0) + sizeof(char);
	log_write(LOG_DEBUG, "Offset beyond the last record: 0x%08x", offset);
	/* New item in record list */
	offset += PDB_RECORD_ITEM_SIZE;
	log_write(LOG_DEBUG, "New offset for new memo: 0x%08x", offset);

	/* Allocate memory for new memo */
	if((memo = calloc(1, sizeof(Memo))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo: %s",
				  strerror(errno));
		return NULL;
	}
	/* '+ 1' for NULL-terminating character */
	if((memo->header = calloc(strlen(header) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo header: %s",
				  strerror(errno));
		free(memo);
		return NULL;
	}
	if(text != NULL &&
	   (memo->text = calloc(strlen(text) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo text: %s",
				  strerror(errno));
		free(memo->header);
		free(memo);
		return NULL;
	}
	if((memo->category = calloc(strlen(category) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo category: %s",
				  strerror(errno));
		if(text != NULL)
		{
			free(memo->text);
		}
		free(memo->header);
		free(memo);
		return NULL;
	}

	/* Add new record for new memo */
	if((record = pdb_record_create(
			memos->_pdb, offset,
			PDB_RECORD_ATTR_EMPTY | (0x0f & categoryId), memo)) == NULL)
	{
		log_write(LOG_ERR, "Cannot add new record for new memo");
		free(memo->category);
		if(text != NULL)
		{
			free(memo->text);
		}
		free(memo->header);
		free(memo);
		return NULL;
	}

	/* Fill new memo with data and append it to PDB structure */
	strcpy(memo->header, header);
	memo->text = text != NULL ? strcpy(memo->text, text) : NULL;
	strcpy(memo->category, category);
	memo->_record = record;
	if(TAILQ_EMPTY(&memos->queue))
	{
		TAILQ_INSERT_HEAD(&memos->queue, memo, pointers);
	}
	else
	{
		TAILQ_INSERT_TAIL(&memos->queue, memo, pointers);
	}

	/* Recalculate and update offsets for old memos due to
	   length of record list change */
	log_write(LOG_DEBUG, "Changing offsets for old memos due to record list "
			  "size change");
	PDBRecord * old_record = TAILQ_PREV(record, RecordQueue, pointers);
	while(old_record != NULL)
	{
		log_write(LOG_DEBUG, "For existing record: old offset=0x%08x, "
				  "new offset=0x%08x", old_record->offset,
				  old_record->offset + PDB_RECORD_ITEM_SIZE);
		old_record->offset += PDB_RECORD_ITEM_SIZE;
		old_record = TAILQ_PREV(old_record, RecordQueue, pointers);
	}

	return memo;
}

int memos_memo_edit(Memos * memos, Memo * memo, char * header, char * text,
					char * category)
{
	if(memos == NULL)
	{
		log_write(LOG_ERR, "Got no memos to edit");
		return -1;
	}
	if(memo == NULL)
	{
		log_write(LOG_ERR, "Cannot edit completely empty memo");
		return -1;
	}

	PDBRecord * record = memo->_record;
	/* Allocate memory for new header and text, if necessary */
	char * newHeader = NULL;
	char * newText = NULL;
	if(header != NULL && strlen(header) > strlen(memo->header) &&
	   (newHeader = calloc(strlen(header), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for new memo's header: %s",
				  strerror(errno));
		return -1;
	}
	int strlenMemoText = memo->text != NULL ? strlen(text) : 0;
	if(text != NULL && strlen(text) > strlenMemoText &&
	   (newText = calloc(strlen(text), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for memo's new text: %s",
				  strerror(errno));
		if(newHeader != NULL)
		{
			free(newHeader);
		}
		return -1;
	}

	uint8_t categoryId = 0;
	if(category != NULL &&
	   (categoryId = pdb_category_get_id(memos->_pdb, category)) == UINT8_MAX)
	{
		log_write(LOG_ERR, "Cannot find category ID for category \"%s\"",
				  category);
		if(newHeader != NULL)
		{
			free(newHeader);
		}
		if(newText != NULL)
		{
			free(newText);
		}
		return -1;
	}

	uint32_t headerSizeDiff =  header != NULL ?
		strlen(header) - strlen(memo->header) :
		0;
	uint32_t textSizeDiff = text != NULL ?
		strlen(text) - strlen(memo->text) :
		0;

	if(newHeader != NULL)
	{
		free(memo->header);
		memo->header = newHeader;
		strcpy(memo->header, header);
	}
	else if(header != NULL)
	{
		explicit_bzero(memo->header, strlen(memo->header));
		strcpy(memo->header, header);
	}

	if(newText != NULL)
	{
		if(memo->text != NULL)
		{
			free(memo->text);
		}
		memo->text = newText;
		strcpy(memo->text, text);
	}
	else if(text != NULL)
	{
		if(memo->text == NULL)
		{
			if((memo->text = calloc(strlen(text), sizeof(char))) == NULL)
			{
				log_write(LOG_ERR, "Failed to allocate memory for new text in "
						  "memo structure: %s", strerror(errno));
				if(newHeader != NULL)
				{
					free(newHeader);
				}
				return -1;
			}
		}
		explicit_bzero(memo->text, strlen(memo->text));
		strcpy(memo->text, text);
	}

	if(category != NULL)
	{
		record->attributes &= 0xf0;
		record->attributes |= categoryId;
	}

	/* Should recalculate offsets for next memos */
	log_write(LOG_DEBUG, "Recalculate offsets for next memos");
	if(headerSizeDiff + textSizeDiff != 0)
	{
		/* Edit offset starting from edited record */
		while((record = TAILQ_NEXT(record, pointers)) != NULL)
		{
			log_write(LOG_DEBUG, "Next memo: old offset=0x%08x, "
					  "new offset=0x%08x", record->offset,
					  record->offset + headerSizeDiff + textSizeDiff);
			record->offset += headerSizeDiff + textSizeDiff;
		}
	}

	return 0;
}

int memos_memo_delete(Memos * memos, Memo * memo)
{
	if(memos == NULL)
	{
		log_write(LOG_ERR, "Got no memos, cannot delete memo");
		return -1;
	}
	if(memo == NULL)
	{
		log_write(LOG_ERR, "Got completely empty memo to delete. "
				  "Nothing to delete.");
		return -1;
	}

	uint32_t offset;
	offset = strlen(memo->header) + sizeof(char); /* header + '\n' */
	offset += (memo->text != NULL ? strlen(memo->text) : 0) + sizeof(char); /* text   + '\0' */

	/* Delete memo */
	PDBRecord * record = memo->_record;
	free(memo->header);
	if(memo->text != NULL)
	{
		free(memo->text);
	}
	free(memo->category);
	TAILQ_REMOVE(&memos->queue, memo, pointers);

	/* Recalculate offsets for next memos */
	PDBRecord * record2 = record;
	log_write(LOG_DEBUG, "Recalculate offsets for existing memos");
	while((record2 = TAILQ_NEXT(record2, pointers)) != NULL)
	{
		log_write(LOG_DEBUG, "Existing memo: old offset=0x%08x, new offset="
				  "0x%08x", record2->offset, record2->offset - offset);
		record2->offset -= offset;
	}
	/* Recalculate offsets due to record list size change */
	log_write(LOG_DEBUG, "Recalculate offsets due to record list size change");
	TAILQ_FOREACH(record2, &memos->_pdb->records, pointers)
	{
		log_write(LOG_DEBUG, "Existing memo [2]: old offset=0x%08x, "
				  "new offset=0x%08x", record2->offset,
				  record2->offset - PDB_RECORD_ITEM_SIZE);
		record2->offset -= PDB_RECORD_ITEM_SIZE;
	}

	/* Delete record */
	long uniqueRecordId = pdb_record_get_unique_id(record);
	if(pdb_record_delete(memos->_pdb, uniqueRecordId))
	{
		log_write(LOG_ERR, "Cannot delete memo record from record list "
				  "(offset: 0x%08x)", record->offset);
		return -1;
	}

	return 0;
}

/**
   Read memo, pointed by PDBRecord, from file with given descriptor.

   @param[in] fd File descriptor.
   @param[in] record PDBRecord, which points to memo.
   @param[in] pdb PDB structure with data from file.
   @return Memo or NULL if error.
*/
static Memo * _memos_read_memo(int fd, PDBRecord * record, PDB * pdb)
{
	/* Go to memo */
	if(lseek(fd, record->offset, SEEK_SET) != record->offset)
	{
		log_write(LOG_ERR, "Cannot go to 0x%08x offset in PDB file to read "
				  "memo: %s", record->offset, strerror(errno));
		return NULL;
	}

	char buffer[CHUNK_SIZE] = "\0";
	ssize_t readedBytes = 0;

	/* Calculate header size */
	unsigned int headerSize = 0;
	while((readedBytes = read(fd, buffer, CHUNK_SIZE)) > 0)
	{
		const char * headerEnd = memchr(buffer, '\n', readedBytes);
		if(headerEnd != NULL)
		{
			headerSize += headerEnd - buffer;
			/* "+ 1" to skip '\n' at the end of memo header */
			if(lseek(fd, -(readedBytes - (headerEnd - buffer)) + 1,
					 SEEK_CUR) == (off_t)-1)
			{
				log_write(LOG_ERR, "Cannot rewind to start of memo text: %s",
						  strerror(errno));
				return NULL;
			}
			break;
		}
		headerSize += readedBytes;
	}
	if(readedBytes < 0)
	{
		log_write(LOG_ERR, "Failed to locate memo header: %s", strerror(errno));
		return NULL;
	}

	/* Calculate text size */
	unsigned int textSize = 0;
	while((readedBytes = read(fd, buffer, CHUNK_SIZE)) > 0)
	{
		const char * textEnd = memchr(buffer, '\0', readedBytes);
		if(textEnd != NULL)
		{
			textSize += textEnd - buffer;
			if(lseek(fd, -(readedBytes - (textEnd - buffer)),
					 SEEK_CUR) == (off_t)-1)
			{
				log_write(LOG_ERR, "Cannot rewind to end of memo text: %s",
						  strerror(errno));
				return NULL;
			}
			break;
		}
		textSize += readedBytes;
	}
	if(readedBytes < 0)
	{
		log_write(LOG_ERR, "Failed to locate memo text: %s", strerror(errno));
		return NULL;
	}

	/* Rewinding to the start of memo */
	if(lseek(fd, record->offset, SEEK_SET) != record->offset)
	{
		log_write(LOG_ERR, "Cannot rewind to start of memo: %s",
				  strerror(errno));
		return NULL;
	}

	/* Allocate memory for memo */
	Memo * memo;
	if((memo = calloc(1, sizeof(Memo))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for memo at offset 0x%08x:"
				  " %s", record->offset, strerror(errno));
		return NULL;
	}
	/* "+ 1" for two next callocs for null-termination bytes */
	if((memo->header = calloc(headerSize + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for memo header: %s",
				  strerror(errno));
		free(memo);
		return NULL;
	}
	if((memo->text = calloc(textSize + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for memo text: %s",
				  strerror(errno));
		free(memo->header);
		free(memo);
		return NULL;
	}
	if((memo->category = calloc(PDB_CATEGORY_LEN, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for memo category: %s",
				  strerror(errno));
		free(memo->text);
		free(memo->header);
		free(memo);
		return NULL;
	}

	log_write(LOG_DEBUG, "Header size: %d, text size: %d", headerSize, textSize);

	/* Reading header */
	if(read_chunks(fd, memo->header, headerSize))
	{
		log_write(LOG_ERR, "Cannot read memo header");
		free(memo->header);
		free(memo->text);
		free(memo->category);
		free(memo);
		return NULL;
	}

	/* Skip '\n' from the header */
	if(lseek(fd, 1, SEEK_CUR) == (off_t)-1)
	{
		log_write(LOG_ERR, "Failed to skip newline symbol between memo header "
				  "and text: %s", strerror(errno));
		free(memo->header);
		free(memo->text);
		free(memo->category);
		free(memo);
		return NULL;
	}

	/* Reading text */
	if(read_chunks(fd, memo->text, textSize))
	{
		log_write(LOG_ERR, "Cannot read memo text");
		free(memo->header);
		free(memo->text);
		free(memo->category);
		free(memo);
		return NULL;
	}

	/* Reading category */
	char * categoryName = pdb_category_get_name(pdb, record->attributes & 0x0f);
	strncpy(memo->category, categoryName, PDB_CATEGORY_LEN);

	memo->_record = record;
	return memo;
}

/**
   Writes given memo to file.

   @param[in] fd File descriptor.
   @param[in] memo Memo to write.
   @return 0 on success or non-zero value if error.
*/
static int _memos_write_memo(int fd, Memo * memo)
{
	if(memo == NULL)
	{
		log_write(LOG_ERR, "Got NULL memo to write!");
		return -1;
	}

	PDBRecord * record = memo->_record;
	uint32_t offset = record->offset;

	/* Go to right place */
	if(lseek(fd, offset, SEEK_SET) != offset)
	{
		log_write(LOG_ERR, "Failed to go to 0x%08x position in PDB file",
				  offset);
		return -1;
	}

	/* Insert header */
	if(write_chunks(fd, memo->header, strlen(memo->header)))
	{
		log_write(LOG_ERR, "Failed to write memo header!");
		return -1;
	}
	log_write(LOG_DEBUG, "Write header (len=%d) [%s] for memo",
			  strlen(memo->header), iconv_cp1251_to_utf8(memo->header));

    /* Insert '\n' as divider */
	if(write(fd, "\n", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\n\" as divider between header "
				  "and text");
		return -1;
	}

	/* Insert text */
	if(memo->text != NULL)
	{
		if(write_chunks(fd, memo->text, strlen(memo->text)))
		{
			log_write(LOG_ERR, "Failed to write memo text!");
			return -1;
		}
		log_write(LOG_DEBUG, "Write text (len=%d) for memo",
				  strlen(memo->text));
	}

	/* Insert '\0' at the end of memo */
	if(write(fd, "\0", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\0\" as divider between memos");
		return -1;
	}
	return 0;
}
