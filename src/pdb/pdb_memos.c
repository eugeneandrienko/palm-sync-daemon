#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "helper.h"
#include "log.h"
#include "pdb_memos.h"


static PDBMemo * _pdb_memos_read_memo(int fd, PDBRecord * record);
static int _pdb_memos_write_memo(int fd, PDBRecord * record);


PDB * pdb_memos_read(char * path)
{
	PDB * pdb;
	int fd = pdb_read(path, 1, &pdb);
	if(fd == -1)
	{
		log_write(LOG_ERR, "Cannot open and read %s PDB file", path);
		return NULL;
	}

	PDBRecord * record;
	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		PDBMemo * memo;
		if((memo = _pdb_memos_read_memo(fd, record)) == NULL)
		{
			pdb_memos_free(pdb);
			pdb_free(fd, pdb);
			log_write(LOG_ERR, "Error when reading Memos from PDB file: %s", path);
			return NULL;
		}
	}
	if(close(fd) == -1)
	{
		pdb_memos_free(pdb);
		pdb_free(fd, pdb);
		log_write(LOG_ERR, "Failed to close %s file after reading Memos", path);
		return NULL;
	}

	return pdb;
}

int pdb_memos_write(char * path, PDB * pdb)
{
	int fd;
	if((fd = open(path, O_RDWR, 0644)) == -1)
	{
		log_write(LOG_ERR, "Cannot open %s PDB file with Memos: %s",
				  path, strerror(errno));
		goto pdb_memos_write_error;
	}

	if(pdb_write(fd, pdb))
	{
		log_write(LOG_ERR, "Cannot write header to PDB file: %s", path);
		goto pdb_memos_write_error;
	}

	PDBRecord * record;
	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		if(_pdb_memos_write_memo(fd, record))
		{
			log_write(LOG_ERR, "Failed to write memo with header \"%s\" to file: %s",
					  ((PDBMemo *)record->data)->header, path);
			goto pdb_memos_write_error;
		}
	}

	pdb_memos_free(pdb);
	pdb_free(fd, pdb);
	return 0;
pdb_memos_write_error:
	pdb_memos_free(pdb);
	pdb_free(fd, pdb);
	return -1;
}

void pdb_memos_free(PDB * pdb)
{
	if(pdb == NULL)
	{
		return;
	}
	struct PDBRecord * record1 = TAILQ_FIRST(&pdb->records);
	struct PDBRecord * record2;
	while(record1 != NULL)
	{
		record2 = TAILQ_NEXT(record1, pointers);
		free(((PDBMemo *)record1->data)->header);
		free(((PDBMemo *)record1->data)->text);
		free((PDBMemo *)record1->data);
		record1->data = NULL;
		record1 = record2;
	}
}

/* Function to operate with memos */

/**
   Element of array of records with memos.

   Used to sort memos by header and search for desired memo by it's header.
*/
struct SortedMemoRecords
{
	char * header;  /**< Pointer to desired memo header */
	PDBRecord * record; /**< Pointer to corresponding record */
};

int __compare_headers(const void * rec1, const void * rec2)
{
	return strcmp(
		((const struct SortedMemoRecords *)rec1)->header,
		((const struct SortedMemoRecords *)rec2)->header);
}

PDBMemo * pdb_memos_memo_get(PDB * pdb, char * header)
{
	int memosQty = pdb->recordsQty;
	struct SortedMemoRecords sortedMemos[memosQty];

	PDBRecord * record;
	unsigned short index = 0;

	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		sortedMemos[index].header = ((PDBMemo *)record->data)->header;
		sortedMemos[index].record = record;
		index++;
	}

	if(memosQty != index)
	{
		log_write(LOG_ERR, "Memos count in header: %d, real memos count: %d",
				  memosQty, index);
		return NULL;
	}

	qsort(&sortedMemos, memosQty, sizeof(struct SortedMemoRecords), __compare_headers);
	struct SortedMemoRecords searchFor = {header, NULL};
	struct SortedMemoRecords * searchResult = bsearch(
		&searchFor, &sortedMemos, memosQty, sizeof(struct SortedMemoRecords),
		__compare_headers);
	return searchResult != NULL ? (PDBMemo *)searchResult->record->data : NULL;
}

PDBMemo * pdb_memos_memo_add(PDB * pdb, char * header, char * text,
							 char * category)
{
	/* Search category ID for given category */
	int categoryId;
	if((categoryId = pdb_category_get_id(pdb, category)) == -1)
	{
		log_write(LOG_ERR, "Category with name \"%s\" not found in PDB header!",
				  category);
		return NULL;
	}

	/* Calculate offset for new memo */
	PDBRecord * record;
	if((record = TAILQ_LAST(&pdb->records, RecordQueue)) == NULL)
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
	if((newRecord = pdb_record_create(
			pdb, offset, PDB_RECORD_ATTR_EMPTY | (0x0f & categoryId))) == NULL)
	{
		log_write(LOG_ERR, "Cannot add new record for new memo");
		free(newMemo->text);
		free(newMemo->header);
		free(newMemo);
		return NULL;
	}

	/* Add new memo */
	strcpy(newMemo->header, header);
	strcpy(newMemo->text, text);
	newRecord->data = newMemo;

	/* Recalculate and update offsets for old memos due to
	   length of record list change */
	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		if(record == newRecord)
		{
			break;
		}
		record->offset += PDB_RECORD_ITEM_SIZE;
	}

	return newMemo;
}

int pdb_memos_memo_edit(PDB * pdb, PDBMemo * memo, char * header,
						char * text, char * category)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "Got NULL PDB structure [pdb_memos_memo_edit]");
		return -1;
	}
	if(memo == NULL)
	{
		log_write(LOG_ERR, "Cannot edit NULL memo");
		return -1;
	}

	PDBRecord * record = TAILQ_FIRST(&pdb->records);
	/* Search for record corresponding for memo in edit */
	while(record->data != memo)
	{
		record = TAILQ_NEXT(record, pointers);
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
	   (categoryId = pdb_category_get_id(pdb, category)) == -1)
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
		record->attributes &= 0xf0;
		record->attributes |= categoryId;
	}

	/* Should recalculate offsets for next memos */
	if(headerSizeDiff + textSizeDiff != 0)
	{
		/* Edit offset starting from edited record */
		while((record = TAILQ_NEXT(record, pointers)) != NULL)
		{
			record->offset += headerSizeDiff + textSizeDiff;
		}
	}

	return 0;
}

int pdb_memos_memo_delete(PDB * pdb, PDBMemo * memo)
{
	if(pdb == NULL)
	{
		log_write(LOG_ERR, "Got NULL PDB structure [pdb_memos_memo_delete]");
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

	/* Search for corresponding record */
	PDBRecord * record = TAILQ_FIRST(&pdb->records);
	while(record->data != memo)
	{
		record = TAILQ_NEXT(record, pointers);
	}

	/* Delete memo record */
	free(memo->header);
	free(memo->text);
	free(memo);
	if(pdb_record_delete(pdb, record))
	{
		log_write(LOG_ERR, "Cannot delete memo record from record list (offset: %x)",
				  record->offset);
		return -1;
	}

	/* Recalculate offsets for next memos */
	while((record = TAILQ_NEXT(record, pointers)) != NULL)
	{
		record->offset -= offset;
	}

	return 0;
}

/**
   Read memo, pointed by PDBRecord, from PDB file and write it to corresponding
   PDBRecord field.

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

	char buffer[CHUNK_SIZE] = "\0";
	ssize_t readedBytes = 0;

	/* Calculate header size */
	unsigned int headerSize = 0;
	while((readedBytes = read(fd, buffer, CHUNK_SIZE)) > 0)
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
	while((readedBytes = read(fd, buffer, CHUNK_SIZE)) > 0)
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
	if(read_chunks(fd, memo->header, headerSize))
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
	if(read_chunks(fd, memo->text, textSize))
	{
		log_write(LOG_ERR, "Cannot read memo text");
		free(memo->header);
		free(memo->text);
		free(memo);
		return NULL;
	}

	/* Calculate hash for memo header */
	record->hash = str_hash(memo->header, strlen(memo->header));
	record->data = (void *)memo;
	return memo;
}

/**
   Writes given memo to file.

   @param[in] fd File descriptor.
   @param[in] memo PDBRecord to write.
   @return 0 on success or non-zero value if error.
*/
static int _pdb_memos_write_memo(int fd, PDBRecord * record)
{
	uint32_t offset = record->offset;

	/* Go to right place */
	if(lseek(fd, offset, SEEK_SET) != offset)
	{
		log_write(LOG_ERR, "Failed to go to 0x%08x position in PDB file", offset);
		return -1;
	}

	/* Insert header */
	PDBMemo * memo = (PDBMemo *)record->data;
	if(write_chunks(fd, memo->header, strlen(memo->header)))
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
	if(write_chunks(fd, memo->text, strlen(memo->text)))
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
