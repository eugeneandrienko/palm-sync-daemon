#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "pdb_memos.h"


#define PDB_MEMOS_CHUNK_SIZE 10 /* Buffer size for memo data */


static PDBMemo * _pdb_memos_read_memo(int fd, PDBRecord * record, PDBFile * pdbFile);
static int _pdb_memos_write_memo(int fd, uint32_t offset, PDBMemo * memo);

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
		if((memo = _pdb_memos_read_memo(fd, record, memos->header)) == NULL)
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
		PDBRecord * record;
		if((record = pdb_record_get(memos->header, recordNo)) == NULL)
		{
			log_write(LOG_ERR, "Failed to get record #%d from header", recordNo);
			pdb_close(fd);
			pdb_memos_free(memos);
			return -1;
		}

		if(_pdb_memos_write_memo(fd, record->offset, memo))
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

PDBMemo * pdb_memos_memo_get(PDBMemos * memos, char * header)
{
	return NULL;
}

int pdb_memos_memo_add(PDBMemos * memos, char * header, char * text,
				  char category[PDB_CATEGORY_LEN])
{
	return 0;
}

int pdb_memos_memo_edit(PDBMemo * memo, char * header, char * text,
				   char * category)
{
	return 0;
}

int pdb_memos_memo_delete(PDBMemos * memos, PDBMemo * memo)
{
	return 0;
}

/**
   Read memo, pointed by PDBRecord, from PDB file.

   @param[in] fd File descriptor.
   @param[in] record PDBRecord, which points to memo.
   @param[in] pdbFile Initialized PDBFile structure to read category info for memo.
   @return Filled PDBMemo or NULL if error.
*/
static PDBMemo * _pdb_memos_read_memo(int fd, PDBRecord * record, PDBFile * pdbFile)
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

	/* Filling category fields in memo */
	memo->categoryId = record->attributes & 0x0f;
	memo->categoryName = pdb_category_get(pdbFile, memo->categoryId);
	if(memo->categoryName == NULL)
	{
		log_write(LOG_ERR, "Cannot read category name for category ID = %d",
				  memo->categoryId);
		free(memo->header);
		free(memo->text);
		free(memo);
		return NULL;
	}

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

	ssize_t readedBytes;
	while(length > 0)
	{
		unsigned int bytesToRead = length < PDB_MEMOS_CHUNK_SIZE ? length : PDB_MEMOS_CHUNK_SIZE;
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
   @param[in] offset Where to write memo in file.
   @param[in] memo PDBMemo to write.
   @return 0 on success or non-zero value if error.
*/
static int _pdb_memos_write_memo(int fd, uint32_t offset, PDBMemo * memo)
{
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

	ssize_t writtenBytes;
	while(length > 0)
	{
		unsigned int bytesToWrite = length < PDB_MEMOS_CHUNK_SIZE ? length : PDB_MEMOS_CHUNK_SIZE;
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
