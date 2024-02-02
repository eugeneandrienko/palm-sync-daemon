#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "pdb_memos.h"


#define PDB_MEMOS_CHUNK_SIZE 10 /* Buffer size for memo data */


static PDBMemo * _pdb_memos_read_memo(int fd, PDBRecord * record);
static int _pdb_memos_write_memo(int fd, PDBMemo * memo);

static int _pdb_memos_read_chunks(int fd, char * buf, unsigned int length);
static int _pdb_memos_write_chunks(int fd, char * buf, unsigned int length);


PDBMemos * pdb_memos_read(char * path)
{
	int fd = pdb_open(path);
	if(fd == -1)
	{
		log_write(LOG_ERR, "Cannot open %s PDB file", path);
		return NULL;
	}

	PDBMemos * memos;
	if((memos = calloc(1, sizeof(PDBMemos))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for PDBMemos structure: %s",
				  strerror(errno));
		pdb_close(fd);
		return NULL;
	}

	if((memos->header = pdb_read(fd, 1)) == NULL)
	{
		log_write(LOG_ERR, "Cannot read PDB header from %s file", path);
		free(memos);
		pdb_close(fd);
		return NULL;
	}

	TAILQ_INIT(&memos->memos);
	if(TAILQ_EMPTY(&memos->header->records))
	{
		log_write(LOG_WARNING, "Empty memos list in %s file", path);
	}

	PDBRecord * record;
	TAILQ_FOREACH(record, &memos->header->records, pointers)
	{
		PDBMemo * memo;
		if((memo = _pdb_memos_read_memo(fd, record)) == NULL)
		{
			pdb_memos_free(memos);
			return NULL;
		}
		if(TAILQ_EMPTY(&memos->memos))
		{
			TAILQ_INSERT_HEAD(&memos->memos, memo, pointers);
		}
		else
		{
			TAILQ_INSERT_TAIL(&memos->memos, memo, pointers);
		}
	}

	return memos;
}

int pdb_memos_write(char * path, PDBMemos * memos)
{
	int fd = pdb_open(path);
	if(fd == -1)
	{
		log_write(LOG_ERR, "Cannot open %s PDB file", path);
		return -1;
	}

	if(pdb_write(fd, memos->header))
	{
		log_write(LOG_ERR, "Cannot write header to PDB file: %s", path);
		return -1;
	}

	PDBMemo * memo;
	unsigned short recordNo = 0;
	TAILQ_FOREACH(memo, &memos->memos, pointers)
	{
		if(_pdb_memos_write_memo(fd, memo))
		{
			log_write(LOG_ERR, "Failed to write memo with header \"%s\" to file: %s",
					  memo->header, path);
			pdb_close(fd);
			pdb_memos_free(memos);
			return -1;
		}

		recordNo++;
	}

	pdb_close(fd);
	pdb_memos_free(memos);
	return 0;
}

void pdb_memos_free(PDBMemos * memos)
{
	if(memos == NULL)
	{
		return;
	}
	struct PDBMemo * memo1 = TAILQ_FIRST(&memos->memos);
	struct PDBMemo * memo2;
	while(memo1 != NULL)
	{
		memo2 = TAILQ_NEXT(memo1, pointers);
		free(memo1->header);
		free(memo1->text);
		free(memo1);
		memo1 = memo2;
	}
	TAILQ_INIT(&memos->memos);
	pdb_free(memos->header);
	free(memos);
}

/**
   Entry for array of memos.

   Used to sort memos by header and search for desired memo by header.
*/
struct SortedMemo
{
	char * header;  /**< Pointer to memo header */
	PDBMemo * memo; /**< Pointer to memo */
};

int __compare_headers(const void * memo1, const void * memo2)
{
	return strcmp(
		((const struct SortedMemo *)memo1)->header,
		((const struct SortedMemo *)memo2)->header);
}

PDBMemo * pdb_memos_memo_get(PDBMemos * memos, char * header)
{
	int memosQty = memos->header->recordsQty;
	struct SortedMemo sortedMemos[memosQty];

	PDBMemo * memo;
	unsigned short index = 0;

	TAILQ_FOREACH(memo, &memos->memos, pointers)
	{
		sortedMemos[index].header = memo->header;
		sortedMemos[index].memo = memo;
		index++;
	}

	if(memosQty != index)
	{
		log_write(LOG_ERR, "Memos count in header: %d, real memos count: %d",
				  memosQty, index);
		return NULL;
	}

	qsort(&sortedMemos, memosQty, sizeof(struct SortedMemo), __compare_headers);
	struct SortedMemo searchFor = {header, NULL};
	struct SortedMemo * searchResult = bsearch(
		&searchFor, &sortedMemos, memosQty, sizeof(struct SortedMemo), __compare_headers);
	return searchResult != NULL ? searchResult->memo : NULL;
}

PDBMemo * pdb_memos_memo_add(PDBMemos * memos, char * header, char * text,
							 char * category)
{
	/* Search category ID for given category */
	int categoryId;
	if((categoryId = pdb_category_get_id(memos->header, category)) == -1)
	{
		log_write(LOG_ERR, "Category with name \"%s\" not found in PDB header!",
				  category);
		return NULL;
	}

	/* Calculate offset for new memo */
	PDBRecord * record;
	if((record = TAILQ_LAST(&memos->header->records, RecordQueue)) == NULL)
	{
		log_write(LOG_ERR, "Cannot get last record from PDB header");
		return NULL;
	}
	uint32_t offset = record->offset;
	offset += PDB_RECORD_ITEM_SIZE;          /* New item in record list */
	offset += strlen(header) + sizeof(char); /* header + '\n' */
	offset += strlen(text) + sizeof(char);   /* text   + '\0' */
	log_write(LOG_DEBUG, "New offset for new memo: 0x%08x", offset);

	/* Allocate memory for new memo */
	PDBMemo * newMemo;
	if((newMemo = calloc(1, sizeof(PDBMemo))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo: %s",
				  strerror(errno));
		return NULL;
	}
	if((newMemo->header = calloc(strlen(header), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo header: %s",
				  strerror(errno));
		free(newMemo);
		return NULL;
	}
	if((newMemo->text = calloc(strlen(text), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new memo text: %s",
				  strerror(errno));
		free(newMemo->header);
		free(newMemo);
		return NULL;
	}

	/* Add new record for new memo */
	PDBRecord * newRecord;
	if((newRecord = pdb_record_add(
			memos->header, offset,
			PDB_RECORD_ATTR_DIRTY | (0x0f & categoryId))) == NULL)
	{
		log_write(LOG_ERR, "Cannot add new record for new memo");
		free(newMemo->text);
		free(newMemo->header);
		free(newMemo);
		return NULL;
	}

	/* Add new memo */
	newMemo->record = newRecord;
	strcpy(newMemo->header, header);
	strcpy(newMemo->text, text);

	/* Recalculate and update offsets for old memos due to
	   length of record list change */
	TAILQ_FOREACH(record, &memos->header->records, pointers)
	{
		if(record == newRecord)
		{
			break;
		}
		record->offset += PDB_RECORD_ITEM_SIZE;
	}

	/* Insert new memo */
	TAILQ_INSERT_TAIL(&memos->memos, newMemo, pointers);

	return newMemo;
}

int pdb_memos_memo_edit(PDBMemos * memos, PDBMemo * memo, char * header,
						char * text, char * category)
{
	if(memos == NULL)
	{
		log_write(LOG_ERR, "Got NULL memos structure");
		return -1;
	}
	if(memo == NULL)
	{
		log_write(LOG_ERR, "Cannot edit NULL memo");
		return -1;
	}

	/* Allocate memory for new header and text, if necessary */
	char * newHeader = NULL;
	char * newText = NULL;
	if(header != NULL && strlen(header) > strlen(memo->header) &&
	   (newHeader = calloc(strlen(header), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for new header for memo: %s",
				  strerror(errno));
		return -1;
	}
	if(text != NULL && strlen(text) > strlen(memo->text) &&
	   (newText = calloc(strlen(text), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for new text for memo: %s",
				  strerror(errno));
		if(newHeader != NULL)
		{
			free(newHeader);
		}
		return -1;
	}

	char categoryId = 0;
	if(category != NULL &&
	   (categoryId = pdb_category_get_id(memos->header, category)) == -1)
	{
		log_write(LOG_ERR, "Cannot find category ID for category \"%s\"", category);
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
		free(memo->text);
		memo->text = newText;
		strcpy(memo->text, text);
	}
	else if(text != NULL)
	{
		explicit_bzero(memo->text, strlen(memo->text));
		strcpy(memo->text, text);
	}

	if(category != NULL)
	{
		memo->record->attributes &= 0xf0;
		memo->record->attributes |= categoryId;
	}

	/* Should recalculate offsets for next memos */
	if(headerSizeDiff + textSizeDiff != 0)
	{
		PDBMemo * next;
		while((next = TAILQ_NEXT(memo, pointers)) != NULL)
		{
			next->record->offset += headerSizeDiff + textSizeDiff;
			memo = next;
		}
	}

	return 0;
}

int pdb_memos_memo_delete(PDBMemos * memos, PDBMemo * memo)
{
	if(memos == NULL)
	{
		log_write(LOG_ERR, "Got NULL PDBMemos structure");
		return -1;
	}
	if(memo == NULL)
	{
		log_write(LOG_ERR, "Got NULL memo to delete");
		return -1;
	}

	uint32_t offset;
	offset = strlen(memo->header) + sizeof(char); /* header + '\n' */
	offset += strlen(memo->text) + sizeof(char);  /* text   + '\0' */

	/* Delete memo record */
	if(pdb_record_delete(memos->header, memo->record))
	{
		log_write(LOG_ERR, "Cannot delete memo record from record list, memo header: %s",
				  memo->header);
		return -1;
	}

	/* Recalculate offsets for next memos */
	PDBMemo * next;
	while((next = TAILQ_NEXT(memo, pointers)) != NULL)
	{
		next->record->offset -= offset;
	}
	log_write(LOG_DEBUG, "recalc offsets");

	/* Delete memo itself */
	free(memo->header);
	free(memo->text);
	TAILQ_REMOVE(&memos->memos, memo, pointers);

	return 0;
}

/**
   Read memo, pointed by PDBRecord, from PDB file.

   @param[in] fd File descriptor.
   @param[in] record PDBRecord, which points to memo.
   @return Filled PDBMemo or NULL if error.
*/
static PDBMemo * _pdb_memos_read_memo(int fd, PDBRecord * record)
{
	/* Go to memo */
	if(lseek(fd, record->offset, SEEK_SET) != record->offset)
	{
		log_write(LOG_ERR, "Cannot go to 0x%08x offset in PDB file to read memo: %s",
				  record->offset, strerror(errno));
		return NULL;
	}

	char buffer[PDB_MEMOS_CHUNK_SIZE] = "\0";
	ssize_t readedBytes = 0;

	/* Calculate header size */
	unsigned int headerSize = 0;
	while((readedBytes = read(fd, buffer, PDB_MEMOS_CHUNK_SIZE)) > 0)
	{
		char * headerEnd = memchr(buffer, '\n', readedBytes);
		if(headerEnd != NULL)
		{
			headerSize += headerEnd - buffer;
			/* "+ 1" to skip '\n' at the end of memo header */
			if(lseek(fd, -(readedBytes - (headerEnd - buffer)) + 1, SEEK_CUR) == (off_t)-1)
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
	while((readedBytes = read(fd, buffer, PDB_MEMOS_CHUNK_SIZE)) > 0)
	{
		char * textEnd = memchr(buffer, '\0', readedBytes);
		if(textEnd != NULL)
		{
			textSize += textEnd - buffer;
			if(lseek(fd, -(readedBytes - (textEnd - buffer)), SEEK_CUR) == (off_t)-1)
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
		log_write(LOG_ERR, "Cannot rewind to start of memo: %s", strerror(errno));
		return NULL;
	}

	/* Allocate memory for memo */
	PDBMemo * memo;
	if((memo = calloc(1, sizeof(PDBMemo))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for memo at offset 0x%08x: %s",
				  record->offset, strerror(errno));
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

	log_write(LOG_DEBUG, "Header size: %d, text size: %d", headerSize, textSize);

	/* Reading header */
	if(_pdb_memos_read_chunks(fd, memo->header, headerSize) < 0)
	{
		log_write(LOG_ERR, "Cannot read memo header");
		free(memo->header);
		free(memo->text);
		free(memo);
		return NULL;
	}

	/* Skip '\n' from the header */
	if(lseek(fd, 1, SEEK_CUR) == (off_t)-1)
	{
		log_write(LOG_ERR, "Failed to skip newline symbol between memo header and text: %s",
				  strerror(errno));
		free(memo->header);
		free(memo->text);
		free(memo);
		return NULL;
	}

	/* Reading text */
	if(_pdb_memos_read_chunks(fd, memo->text, textSize))
	{
		log_write(LOG_ERR, "Cannot read memo text");
		free(memo->header);
		free(memo->text);
		free(memo);
		return NULL;
	}

	memo->record = record;

	return memo;
}

/**
   Read data from file by chunks.

   @param[in] fd File descriptor.
   @param[in] buf Buffer for readed data.
   @param[in] length Length of buffer.
   @return 0 on successfull read or non-zero value if error.
*/
static int _pdb_memos_read_chunks(int fd, char * buf, unsigned int length)
{
	if(buf == NULL)
	{
		log_write(LOG_ERR, "Buffer is null (%s)", "_pdb_memos_read_chunks");
		return -1;
	}

	while(length > 0)
	{
		unsigned int bytesToRead = length < PDB_MEMOS_CHUNK_SIZE ? length : PDB_MEMOS_CHUNK_SIZE;
		ssize_t readedBytes;
		if((readedBytes = read(fd, buf, bytesToRead)) < 0)
		{
			log_write(LOG_ERR, "Cannot read to buffer: %s", strerror(errno));
			return -1;
		}
		else if(readedBytes == 0)
		{
			log_write(LOG_ERR, "Suddenly reach EOF while reading PDB file!");
			return -1;
		}
		buf += readedBytes;
		length -= readedBytes;
	}

	return 0;
}

/**
   Writes given memo to file.

   @param[in] fd File descriptor.
   @param[in] memo PDBMemo to write.
   @return 0 on success or non-zero value if error.
*/
static int _pdb_memos_write_memo(int fd, PDBMemo * memo)
{
	uint32_t offset = memo->record->offset;

	/* Go to right place */
	if(lseek(fd, offset, SEEK_SET) != offset)
	{
		log_write(LOG_ERR, "Failed to go to 0x%08x position in PDB file", offset);
		return -1;
	}

	/* Insert header */
	if(_pdb_memos_write_chunks(fd, memo->header, strlen(memo->header)))
	{
		log_write(LOG_ERR, "Failed to write memo header!");
		return -1;
	}
	log_write(LOG_DEBUG, "Write header (len=%d) for memo", strlen(memo->header));

    /* Insert '\n' as divider */
	if(write(fd, "\n", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\n\" as divider between header and text");
		return -1;
	}

	/* Insert text */
	if(_pdb_memos_write_chunks(fd, memo->text, strlen(memo->text)))
	{
		log_write(LOG_ERR, "Failed to write memo text!");
		return -1;
	}
	log_write(LOG_DEBUG, "Write text (len=%d) for memo", strlen(memo->text));

	/* Insert '\0' at the end of memo */
	if(write(fd, "\0", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\0\" as divider between memos");
		return -1;
	}
	return 0;
}

/**
   Write data from file by chunks.

   @param[in] fd File descriptor.
   @param[in] buf Buffer for data to write.
   @param[in] length Length of buffer.
   @return 0 on successfull write or non-zero value if error.
*/
static int _pdb_memos_write_chunks(int fd, char * buf, unsigned int length)
{
	if(buf == NULL)
	{
		log_write(LOG_ERR, "Buffer is null (%s)", "_pdb_memos_write_chunks");
		return -1;
	}

	while(length > 0)
	{
		unsigned int bytesToWrite = length < PDB_MEMOS_CHUNK_SIZE ? length : PDB_MEMOS_CHUNK_SIZE;
		ssize_t writtenBytes;
		if((writtenBytes = write(fd, buf, bytesToWrite)) <= 0)
		{
			log_write(LOG_ERR, "Cannot write to buffer: %s", strerror(errno));
			return -1;
		}
		buf += writtenBytes;
		length -= writtenBytes;
	}

	return 0;
}
