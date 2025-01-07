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
		PDBRecord * record = memo1->_record;
		memo1->_record = NULL;
		TAILQ_REMOVE(&memos->queue, memo1, pointers);
		free(memo1);
		record->data = NULL;
		memo1 = memo2;
	}

	pdb_free(memos->_pdb);
	free(memos);
}


/* Functions to operate with memo */

/**
   Element of array with memos. Maps memo header, or header and text, or ID
   and pointer to memo.

   Used to sort memos and search for desired memo.
*/
struct __SortedMemos
{
	uint32_t id;   /**< ID of Memo */
	char * header; /**< Pointer to Memo header */
	char * text;   /**< Pointer to Memo text */
	Memo * memo;   /**< Pointer to Memo structure */
};

static int __compare_headers(const void * rec1, const void * rec2)
{
	return strcmp(((const struct __SortedMemos *)rec1)->header,
				  ((const struct __SortedMemos *)rec2)->header);
}

static int __compare_headers_and_text(const void * rec1, const void * rec2)
{
	char * text1 = ((const struct __SortedMemos *)rec1)->text;
	char * text2 = ((const struct __SortedMemos *)rec2)->text;
	int result = strcmp(((const struct __SortedMemos *)rec1)->header,
						((const struct __SortedMemos *)rec2)->header);
	if(result == 0 && text1 != NULL && text2 != NULL)
	{
		return strcmp(text1, text2);
	}
	else
	{
		return result;
	}
}

/**
   Recursive search of memos with given header/text.

   On first iteration function will search for desired memo. On second
   iteration it will search for the memo with the same header/text,
   excluding previously found memo.

   @param[in] memos Memos structure.
   @param[in] header Will search memo with this header.
   @param[in] text Optional. Will search memo with this text. May be NULL, then
   memo text will not be used in search.
   @param[out] id ID of found memo. Will be NULL on error.
   @param[in] prevId ID of memo, found on previous iteration or NULL on the
   first iteration.
   @return Zero on success, E_NOMEMO if no memo is found, E_MULTIPLE_MEMOS if
   found multiple memos.
*/
int _memos_memo_get_id(Memos * memos, char * header, char * text, uint32_t * id,
					   uint32_t * prevId)
{
	int memosQty = memos->_pdb->recordsQty;
	/* One less element on the 2nd iteration */
	struct __SortedMemos sortedMemos[prevId == NULL ? memosQty : memosQty - 1];

	Memo * memo;
	unsigned short index = 0;

	if(prevId != NULL)
	{
		log_write(LOG_DEBUG, "Second iteration of _memos_memo_get_id");
	}
	else
	{
		log_write(LOG_DEBUG, "First iteration of _memos_memo_get_id");
	}

	TAILQ_FOREACH(memo, &memos->queue, pointers)
	{
		if(prevId != NULL && *prevId == memo->id)
		{
			continue;
		}
		sortedMemos[index].header = memo->header;
		sortedMemos[index].text = memo->text;
		sortedMemos[index].memo = memo;
		index++;
	}

	if(prevId == NULL && memosQty != index)
	{
		log_write(LOG_ERR, "Memos count in PDB header: %d, real memos "
				  "count: %d", memosQty, index);
		return E_NOMEMO;
	}

	qsort(&sortedMemos, memosQty, sizeof(struct __SortedMemos),
		  text == NULL ? __compare_headers : __compare_headers_and_text);
	struct __SortedMemos searchFor = {0, header, text, NULL};
	struct __SortedMemos * searchResult = bsearch(
		&searchFor, &sortedMemos, memosQty, sizeof(struct __SortedMemos),
		text == NULL ? __compare_headers : __compare_headers_and_text);

	if(searchResult == NULL)
	{
		return E_NOMEMO;
	}

	if(prevId == NULL)
	{
		if(_memos_memo_get_id(memos, header, text, id,
								   &searchResult->memo->id) != E_NOMEMO)
		{
			return E_MULTIPLE_MEMOS;
		}
		else
		{
			*id = searchResult->memo->id;
			return 0;
		}
	}
	else
	{
		*id = searchResult->memo->id;
		log_write(LOG_DEBUG, "Found second memo with header = %s. ID = %d",
				  header, *id);
		return 0;
	}
}

int memos_memo_get_id(Memos * memos, char * header, char * text, uint32_t * id)
{
	return _memos_memo_get_id(memos, header, text, id, NULL);
}

static int __compare_ids(const void * rec1, const void * rec2)
{
	uint32_t id1 = ((const struct __SortedMemos *)rec1)->id;
	uint32_t id2 = ((const struct __SortedMemos *)rec2)->id;
	if(id1 < id2)
	{
		return -1;
	}
	else if(id1 == id2)
	{
		return 0;
	}
	else
	{
		return 1;
	};
}

Memo * memos_memo_get(Memos * memos, uint32_t id)
{
	int memosQty = memos->_pdb->recordsQty;
	struct __SortedMemos sortedMemos[memosQty];

	Memo * memo;
	unsigned short index = 0;

	TAILQ_FOREACH(memo, &memos->queue, pointers)
	{
		sortedMemos[index].id = memo->id;
		sortedMemos[index].memo = memo;
		index++;
	}

	if(memosQty != index)
	{
		log_write(LOG_ERR, "Memos count in PDB header: %d, real memos "
				  "count: %d", memosQty, index);
		return NULL;
	}

	qsort(&sortedMemos, memosQty, sizeof(struct __SortedMemos), __compare_ids);
	struct __SortedMemos searchFor = {id, NULL, NULL, NULL};
	struct __SortedMemos * searchResult = bsearch(
		&searchFor, &sortedMemos, memosQty, sizeof(struct __SortedMemos),
		__compare_ids);
	return searchResult == NULL ? NULL : searchResult->memo;
}

uint32_t memos_memo_add(Memos * memos, char * header, char * text,
						char * category)
{
	if(memos == NULL)
	{
		log_write(LOG_ERR, "No memos structure - nowhere to add new memo!");
		return 0;
	}
	if(header == NULL)
	{
		log_write(LOG_ERR, "Header of new memo is NULL! Cannot add new memo!");
		return 0;
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
			return 0;
		}
	}

	/* Calculate offset for new memo: */

	/* Get last memo */
	PDBRecord * record;
	Memo * memo;
	if((record = TAILQ_LAST(&memos->_pdb->records, RecordQueue)) == NULL)
	{
		log_write(LOG_ERR, "Cannot get last memo's record from PDB header");
		return 0;
	}
	if((memo = TAILQ_LAST(&memos->queue, MemosQueue)) == NULL)
	{
		log_write(LOG_ERR, "Cannot get last memo");
		return 0;
	}
	if(memo->_record != record)
	{
		log_write(LOG_ERR, "Latest memo and latest PDB record doesn't match");
		return 0;
	}
	uint32_t offset = record->offset;
	log_write(LOG_DEBUG, "Offset of the last record: 0x%08x", offset);

	/* Calculate size of last record and offset beyond it: */
	/* Header + '\n' */
	offset += memo->_header_cp1251_len + sizeof(char);
	/* Text (if exists) + '\0' */
	offset += (memo->text != NULL ? memo->_text_cp1251_len : 0) + sizeof(char);
	log_write(LOG_DEBUG, "Offset beyond the last record: 0x%08x", offset);
	/* New item in record list */
	offset += PDB_RECORD_ITEM_SIZE;
	log_write(LOG_DEBUG, "New offset for new memo: 0x%08x", offset);

	/* Prepare header for new memo */
	char * newHeader;
	/* '+ 1' for NULL-terminating character */
	if((newHeader = calloc(strlen(header) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo header: %s",
				  strerror(errno));
		return 0;
	}
	strcpy(newHeader, header);
	/* Calculate length of CP1251 encoded header */
	char * newHeaderCp1251 = iconv_utf8_to_cp1251(header);
	if(newHeaderCp1251 == NULL)
	{
		log_write(LOG_ERR, "Failed to convert new memo header \"%s\" "
				  "from UTF8 to CP1251", header);
		free(newHeader);
		return 0;
	}
	size_t newHeaderCp1251Len = strlen(newHeaderCp1251);
	free(newHeaderCp1251);
	log_write(LOG_DEBUG, "New memo header copied, length (CP1251): %d",
			  newHeaderCp1251Len);

	/* Prepare text for new memo */
	char * newText = NULL;
	size_t newTextCp1251Len = 0;
	if(text != NULL)
	{
		/* '+1' for NULL-terminated character */
		if((newText = calloc(strlen(text) + 1, sizeof(char))) == NULL)
		{
			log_write(LOG_ERR, "Cannot allocate memory for new memo text: %s",
					  strerror(errno));
			free(newHeader);
			return 0;
		}
		strcpy(newText, text);
		/* Calculate length of CP1251 encoded text */
		char * newTextCp1251 = iconv_utf8_to_cp1251(text);
		if(newTextCp1251 == NULL)
		{
			log_write(LOG_ERR, "Failed to convert new memo text \"%s\" "
					  "from UTF8 to CP1251", text);
			free(newHeader);
			free(newText);
			return 0;
		}
		newTextCp1251Len = strlen(newTextCp1251);
		free(newTextCp1251);
		log_write(LOG_DEBUG, "New memo text copied, length (CP1251): %d",
				  newTextCp1251Len);
	}

	/* Prepare category for new memo */
	char * newCategory;
	if((newCategory = calloc(strlen(category) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo category: %s",
				  strerror(errno));
		free(newHeader);
		free(newText);
		return 0;
	}
	strcpy(newCategory, category);
	log_write(LOG_DEBUG, "New memo category copied");

	/* Allocate memory for new memo */
	if((memo = calloc(1, sizeof(Memo))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo: %s",
				  strerror(errno));
		free(newHeader);
		free(newText);
		free(newCategory);
		return 0;
	}
	log_write(LOG_DEBUG, "Memory for new memo allocated");

	/* Add new record for new memo */
	if((record = pdb_record_create(
			memos->_pdb, offset,
			PDB_RECORD_ATTR_EMPTY | (0x0f & categoryId), memo)) == NULL)
	{
		log_write(LOG_ERR, "Cannot add new PDB record for new memo");
		free(newHeader);
		free(newText);
		free(newCategory);
		free(memo);
		return 0;
	}

	/* Read memo unique ID */
	uint32_t id = pdb_record_get_unique_id(record);
	log_write(LOG_DEBUG, "PDB record for new memo created. Record: ID: %d", id);

	/* Fill new memo with data and append it to PDB structure */
	memo->id = id;
	memo->header = newHeader;
	memo->text = newText;
	memo->category = newCategory;
	memo->_record = record;
	memo->_header_cp1251_len = newHeaderCp1251Len;
	memo->_text_cp1251_len = newTextCp1251Len;
	if(TAILQ_EMPTY(&memos->queue))
	{
		TAILQ_INSERT_HEAD(&memos->queue, memo, pointers);
		log_write(LOG_DEBUG, "New memo added as the first memo to the queue");
	}
	else
	{
		TAILQ_INSERT_TAIL(&memos->queue, memo, pointers);
		log_write(LOG_DEBUG, "New memo added as the last memo to the queue");
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

	return id;
}

int memos_memo_edit(Memos * memos, uint32_t id, char * header, char * text,
					char * category)
{
	if(memos == NULL)
	{
		log_write(LOG_ERR, "Got no memos to edit");
		return -1;
	}
	Memo * memo = memos_memo_get(memos, id);
	if(memo == NULL)
	{
		log_write(LOG_ERR, "Cannot get memo with ID = %d", id);
		return -1;
	}
	log_write(LOG_DEBUG, "Found memo with ID %d and header \"%s\" for edit",
			  id, memo->header);

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
	size_t strlenOldMemoText = memo->text != NULL ? strlen(text) : 0;
	if(text != NULL && strlen(text) > strlenOldMemoText &&
	   (newText = calloc(strlen(text), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for memo's new text: %s",
				  strerror(errno));
		free(newHeader);
		return -1;
	}

	/* Load category ID for new category, if necessary */
	uint8_t categoryId = 0;
	if(category != NULL &&
	   (categoryId = pdb_category_get_id(memos->_pdb, category)) == UINT8_MAX)
	{
		log_write(LOG_ERR, "Cannot find category ID for category \"%s\"",
				  category);
		free(newHeader);
		free(newText);
		return -1;
	}

	/* Calculate difference between new and old header/text strings in CP1251
	   encoding, while old data is exists */
	uint32_t headerSizeDiff = 0;
	if(header != NULL)
	{
		char * headerCp1251 = iconv_utf8_to_cp1251(header);
		headerSizeDiff = strlen(headerCp1251) - memo->_header_cp1251_len;
		free(headerCp1251);
	}
	uint32_t textSizeDiff = 0;
	if(text != NULL)
	{
		char * textCp1251 = iconv_utf8_to_cp1251(text);
		textSizeDiff = strlen(textCp1251) - memo->_text_cp1251_len;
		free(textCp1251);
	}
	log_write(LOG_DEBUG, "Calculate strings' size diffs, header: %d, text: %d",
			  headerSizeDiff, textSizeDiff);

	/* Set new header for memo */
	if(newHeader != NULL)
	{
		free(memo->header);
		memo->header = newHeader;
		strcpy(memo->header, header);
		log_write(LOG_DEBUG, "New header set");
	}
	else if(header != NULL)
	{
		explicit_bzero(memo->header, strlen(memo->header));
		strcpy(memo->header, header);
		log_write(LOG_DEBUG, "New header set");
	}

	/* Set new text for memo */
	if(newText != NULL)
	{
		/* New text size is larger than existing memo's text size */
		free(memo->text);
		/* Use newly allocated string */
		memo->text = newText;
		/* and copy new text string to it */
		strcpy(memo->text, text);
		log_write(LOG_DEBUG, "New text set");
	}
	else if(text != NULL)
	{
		/* New text size is smaller or equals to existing memo's text size */
		if(memo->text == NULL)
		{
			if((memo->text = calloc(strlen(text), sizeof(char))) == NULL)
			{
				log_write(LOG_ERR, "Failed to allocate memory for new text in "
						  "memo structure: %s", strerror(errno));
				free(newHeader);
				return -1;
			}
		}
		explicit_bzero(memo->text, strlen(memo->text));
		strcpy(memo->text, text);
		log_write(LOG_DEBUG, "New text set");
	}

	/* Set new category for category */
	if(category != NULL)
	{
		record->attributes &= 0xf0;
		record->attributes |= categoryId;
		log_write(LOG_DEBUG, "New category set");
	}

	/* Should recalculate PDB offsets for next memos */
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

int memos_memo_delete(Memos * memos, uint32_t id)
{
	if(memos == NULL)
	{
		log_write(LOG_ERR, "Got no memos, cannot delete memo");
		return -1;
	}
	Memo * memo = memos_memo_get(memos, id);
	if(memo == NULL)
	{
		log_write(LOG_ERR, "Memo with ID = %d not found. "
				  "Nothing to delete.", id);
		return -1;
	}

	uint32_t memoId = memo->id;
	log_write(LOG_DEBUG, "Deleting memo with ID: %d", memoId);

	uint32_t offset;
	/* header + '\n' */
	offset = strlen(memo->header) + sizeof(char);
	/* text   + '\0' */
	offset += (memo->text != NULL ? strlen(memo->text) : 0) + sizeof(char);

	/* Delete memo */
	PDBRecord * record = memo->_record;
	free(memo->header);
	free(memo->text);
	free(memo->category);
	TAILQ_REMOVE(&memos->queue, memo, pointers);

	/* Recalculate offsets for next memos */
	log_write(LOG_DEBUG, "Recalculate offsets for existing memos");
	while((record = TAILQ_NEXT(record, pointers)) != NULL)
	{
		log_write(LOG_DEBUG, "Existing memo: old offset=0x%08x, new offset="
				  "0x%08x", record->offset, record->offset - offset);
		record->offset -= offset;
	}
	/* Recalculate offsets due to record list size change */
	log_write(LOG_DEBUG, "Recalculate offsets due to record list size change");
	TAILQ_FOREACH(record, &memos->_pdb->records, pointers)
	{
		log_write(LOG_DEBUG, "Existing memo [2]: old offset=0x%08x, "
				  "new offset=0x%08x", record->offset,
				  record->offset - PDB_RECORD_ITEM_SIZE);
		record->offset -= PDB_RECORD_ITEM_SIZE;
	}

	/* Delete record */
	if(pdb_record_delete(memos->_pdb, memoId))
	{
		log_write(LOG_ERR, "Cannot delete memo record with ID=%d from record "
				  "list (offset: 0x%08x)", memoId, record->offset);
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
	log_write(LOG_DEBUG, "Header size: %d, text size: %d", headerSize, textSize);

	/* Rewinding to the start of memo */
	if(lseek(fd, record->offset, SEEK_SET) != record->offset)
	{
		log_write(LOG_ERR, "Cannot rewind to start of memo: %s",
				  strerror(errno));
		return NULL;
	}

	/* Read CP1251 encoded header */
	char * headerCp1251;
	if((headerCp1251 = calloc(headerSize, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate buffer for new memo header "
				  "(CP1251): %s", strerror(errno));
		return NULL;
	}
	if(read_chunks(fd, headerCp1251, headerSize))
	{
		log_write(LOG_ERR, "Cannot read memo header");
		return NULL;
	}

	/* Encode header to UTF8 */
	char * header = iconv_cp1251_to_utf8(headerCp1251);
	if(header == NULL)
	{
		log_write(LOG_ERR, "Failed to encode CP1251 header to UTF8");
		free(headerCp1251);
		return NULL;
	}
	free(headerCp1251);

	/* Skip '\n' from the header */
	if(lseek(fd, 1, SEEK_CUR) == (off_t)-1)
	{
		log_write(LOG_ERR, "Failed to skip newline symbol between memo header "
				  "and text: %s", strerror(errno));
		free(header);
		return NULL;
	}

	/* Read CP1251 encoded text */
	char * textCp1251;
	if((textCp1251 = calloc(textSize, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate buffer for new memo text "
				  "(CP1251): %s", strerror(errno));
		free(header);
		return NULL;
	}
	if(read_chunks(fd, textCp1251, textSize))
	{
		log_write(LOG_ERR, "Cannot read memo text");
		free(header);
		return NULL;
	}

	/* Encode text for UTF8 */
	char * text = iconv_cp1251_to_utf8(textCp1251);
	if(text == NULL)
	{
		log_write(LOG_ERR, "Failed to encode CP1251 text to UTF8");
		free(header);
		free(textCp1251);
		return NULL;
	}
	free(textCp1251);

	/* Get string with category name */
	char * categoryName = pdb_category_get_name(pdb, record->attributes & 0x0f);
	if(categoryName == NULL)
	{
		log_write(LOG_ERR, "Failed to read category name");
		free(header);
		free(text);
	    return NULL;
	}

	/* Allocate memory for memo */
	Memo * memo;
	if((memo = calloc(1, sizeof(Memo))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for memo at offset 0x%08x:"
				  " %s", record->offset, strerror(errno));
		free(header);
		free(text);
		free(categoryName);
		return NULL;
	}

	/* Allocate memory for new memo's category */
	if((memo->category = calloc(PDB_CATEGORY_LEN, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for memo category: %s",
				  strerror(errno));
		free(memo);
		free(header);
		free(text);
		free(categoryName);
		return NULL;
	}

	/* Set memo ID */
	uint32_t id = pdb_record_get_unique_id(record);
	if(id == 0)
	{
		log_write(LOG_ERR, "Failed to get ID of memo!");
		free(memo->category);
		free(memo);
		free(text);
		free(header);
		free(categoryName);
		return NULL;
	}

	memo->id = id;
	memo->header = header;
	memo->text = text;
	strncpy(memo->category, categoryName, PDB_CATEGORY_LEN);
	memo->_record = record;
	memo->_header_cp1251_len = headerSize;
	memo->_text_cp1251_len = textSize;
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

	/* Encode header to CP1251 */
	char * headerCp1251 = iconv_utf8_to_cp1251(memo->header);
	if(headerCp1251 == NULL)
	{
		log_write(LOG_ERR, "Failed to encode memo's header \"%s\" to CP1251. "
				  "Memo ID = %d", memo->header, memo->id);
		return -1;
	}

	/* Insert header */
	if(write_chunks(fd, headerCp1251, memo->_header_cp1251_len))
	{
		log_write(LOG_ERR, "Failed to write memo header!");
		free(headerCp1251);
		return -1;
	}
	log_write(LOG_DEBUG, "Write header (len=%d) [%s] for memo",
			  strlen(memo->header), memo->header);
	free(headerCp1251);

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
		/* Encode text to CP1251 */
		char * textCp1251 = iconv_utf8_to_cp1251(memo->text);
		if(textCp1251 == NULL)
		{
			log_write(LOG_ERR, "Failed to encode memo's text \"%s\" to CP1251. "
					  "Memo ID = %d", memo->text, memo->id);
			return -1;
		}

		if(write_chunks(fd, textCp1251, memo->_text_cp1251_len))
		{
			log_write(LOG_ERR, "Failed to write memo text!");
			free(textCp1251);
			return -1;
		}
		log_write(LOG_DEBUG, "Write text (len=%d) [%s] for memo",
				  strlen(memo->text), memo->text);
		free(textCp1251);
	}

	/* Insert '\0' at the end of memo */
	if(write(fd, "\0", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\0\" as divider between memos");
		return -1;
	}
	return 0;
}
