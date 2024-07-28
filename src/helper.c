#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "helper.h"
#include "log.h"
#include "umash.h"


#if defined (__FreeBSD__)
#define UTF8 "UTF-8"
#else
#define UTF8 "UTF8"
#endif
#define CP1251 "CP1251"


char * iconv_utf8_to_cp1251(char * string)
{
	iconv_t iconvfd;
	if((iconvfd = iconv_open(CP1251, UTF8)) == (iconv_t)-1)
	{
		log_write(LOG_ERR, "Cannot initialize UTF8->CP1251 convertor: %s", strerror(errno));
		return NULL;
	}

	char * inString = string;
	size_t inStringLen = strlen(string);
	size_t outStringLen = inStringLen * 2; /* 1 cyrillic symbol == 2 bytes */
	char * outString;
	if((outString = calloc(outStringLen, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for converted string: %s",
				  strerror(errno));
		if(iconv_close(iconvfd) == -1)
		{
			log_write(LOG_ERR, "Cannot close iconv: %s", strerror(errno));
		}
		return NULL;
	}

	char * result = outString;
	if(iconv(iconvfd, &inString, &inStringLen, &outString, &outStringLen) == (size_t)-1)
	{
		log_write(LOG_ERR, "Failed to convert UTF8 string \"%s\" to CP1251: %s",
				  string, strerror(errno));
		if(iconv_close(iconvfd) == -1)
		{
			log_write(LOG_ERR, "Cannot close iconv: %s", strerror(errno));
		}
		free(outString);
		return NULL;
	}

	if(iconv_close(iconvfd) == -1)
	{
		log_write(LOG_ERR, "Cannot close iconv: %s", strerror(errno));
	}
	return result;
}

char * iconv_cp1251_to_utf8(char * string)
{
	iconv_t iconvfd;
	if((iconvfd = iconv_open(UTF8, CP1251)) == (iconv_t)-1)
	{
		log_write(LOG_ERR, "Cannot initialize CP1251->UTF8 convertor: %s", strerror(errno));
		return NULL;
	}

	char * inString = string;
	size_t inStringLen = strlen(string);
	size_t outStringLen = inStringLen * 2;
	char * outString;
	if((outString = calloc(outStringLen, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for converted string: %s",
				  strerror(errno));
		if(iconv_close(iconvfd) == -1)
		{
			log_write(LOG_ERR, "Cannot close iconv: %s", strerror(errno));
		}
		return NULL;
	}

	char * result = outString;
	if(iconv(iconvfd, &inString, &inStringLen, &outString, &outStringLen) == (size_t)-1)
	{
		log_write(LOG_ERR, "Failed to convert CP1251 string \"%s\" to UTF8: %s",
				  string, strerror(errno));
		if(iconv_close(iconvfd) == -1)
		{
			log_write(LOG_ERR, "Cannot close iconv: %s", strerror(errno));
		}
		free(outString);
		return NULL;
	}

	if(iconv_close(iconvfd) == -1)
	{
		log_write(LOG_ERR, "Cannot close iconv: %s", strerror(errno));
	}
	return result;
}

int read_chunks(int fd, char * buf, unsigned int length)
{
	if(buf == NULL)
	{
		log_write(LOG_ERR, "Buffer is null (%s)", "_pdb_memos_read_chunks");
		return -1;
	}

	while(length > 0)
	{
		unsigned int bytesToRead = length < CHUNK_SIZE ? length : CHUNK_SIZE;
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

int write_chunks(int fd, char * buf, unsigned int length)
{
	if(buf == NULL)
	{
		log_write(LOG_ERR, "Buffer is null (%s)", "_pdb_memos_write_chunks");
		return -1;
	}

	while(length > 0)
	{
		unsigned int bytesToWrite = length < CHUNK_SIZE ? length : CHUNK_SIZE;
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

uint64_t str_hash(char * buf, size_t length)
{
	/* Prepare configuration for UMASH hash-function */
	static uint64_t seed1 = 0xc328ec6a247b1455;
	static uint64_t seed2 = 0x18af24e667bbd865;
	struct umash_params umashp;
	umash_params_derive(&umashp, seed1, NULL);
	return umash_full(&umashp, seed2, 0, buf, length);
}


/**
   Filename of Datebook PDB file from previous iteration.
*/
#define PREV_DATEBOOK_PDB "previousDatebook.pdb"
/**
   Filename of Memos PDB file from previous iteration.
*/
#define PREV_MEMOS_PDB "previousMemos.pdb"
/**
   Filename of TODO PDB file from previous iteration.
*/
#define PREV_TODO_PDB "previousTodo.pdb"

/**
   Max length of PDB filepath for file from previous iteration.
*/
#define MAX_PATH_LEN 300

/**
   Copy buffer length
*/
#define COPY_BUFFER_LENGTH 4096


static int _check_previous_pdb(char * dataDir, char * pdbFileName, char ** result);
static int _save_as_previous_pdb(char ** pathToPrevPDB, char * pathToCurrentPDB,
								 char * dataDir, char * prevPdbFname);
static int _cp(char * from, char * to);


int check_previous_pdbs(SyncSettings * syncSettings)
{
	/* Check for Datebook PDB file from previous iteration */
	if(_check_previous_pdb(syncSettings->dataDir, PREV_DATEBOOK_PDB,
						   &syncSettings->prevDatebookPDB))
	{
		return -1;
	}
	/* Check for Memos PDB file from previous iteration */
	if(_check_previous_pdb(syncSettings->dataDir, PREV_MEMOS_PDB,
						   &syncSettings->prevMemosPDB))
	{
		return -1;
	}
	/* Check for TODO PDB file from previous iteration */
	if(_check_previous_pdb(syncSettings->dataDir, PREV_TODO_PDB,
						   &syncSettings->prevTodoPDB))
	{
		return -1;
	}
	return 0;
}

/**
   Function check one of the PDB file from previous synchronization iteration.

   Function checks existence of file and copy corresponding filepath
   to result variable if file exists and accessible. Or if file not exists NULL
   will be set as result.

   If file exists and not accessible, the error will be returned.

   @param[in] dataDir path to data directory.
   @param[in] pdbFileName name of PDB file to check
   @param[out] result Pointer to string with resulting path.
   @return Zero value on success, otherwise non-zero value will be returned.
*/
static int _check_previous_pdb(char * dataDir, char * pdbFileName,	char ** result)
{
	char prevPDBPath[MAX_PATH_LEN] = "\0";

	strncpy(prevPDBPath, dataDir, strlen(dataDir));
	strcat(prevPDBPath, pdbFileName);

	if(access(prevPDBPath, F_OK))
	{
		log_write(LOG_DEBUG, "PDB file %s from previous sync cycle not found",
				  prevPDBPath);
		*result = NULL;
		return 0;
	}
	if(access(prevPDBPath, R_OK | W_OK))
	{
		log_write(LOG_WARNING, "No access to PDB file from previous iteration: %s",
				  prevPDBPath);
		return -1;
	}

	if((*result = calloc(strlen(prevPDBPath) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for %s file path",
				  pdbFileName);
		return -1;
	}
	strncpy(*result, prevPDBPath, strlen(prevPDBPath));
	log_write(LOG_DEBUG, "Found PDB file %s from previous sync cycle",
			  *result);
	return 0;
}

int save_as_previous_pdbs(SyncSettings * syncSettings, PalmData * palmData)
{
	if(_save_as_previous_pdb(&syncSettings->prevDatebookPDB,
							 palmData->datebookDBPath,
							 syncSettings->dataDir,
							 PREV_DATEBOOK_PDB))
	{
		log_write(LOG_ERR, "Failed to copy %s as old Datebook PDB file",
				  palmData->datebookDBPath);
		return -1;
	}
	if(_save_as_previous_pdb(&syncSettings->prevMemosPDB,
							 palmData->memoDBPath,
							 syncSettings->dataDir,
							 PREV_MEMOS_PDB))
	{
		log_write(LOG_ERR, "Failed to copy %s as old Memos PDB file",
				  palmData->memoDBPath);
		return -1;
	}
	if(_save_as_previous_pdb(&syncSettings->prevTodoPDB,
							 palmData->todoDBPath,
							 syncSettings->dataDir,
							 PREV_TODO_PDB))
	{
		log_write(LOG_ERR, "Failed to copy %s as old TODO PDB file",
				  palmData->todoDBPath);
		return -1;
	}
	return 0;
}

/**
   Copy PDB file to given path.

   Function check given path to previous PDB file for existence. If it
   is NULL, then it will be constructed as concatenation of dataDir
   and prevPdbFname strings.

   Then a new hardlink will be created at given path. Hardlink will
   point to pathtoCurrentPDB file.

   @param[in] pathToPrevPDB path to copy previous PDB file to.
   @param[in] pathToCurrentPDB path to PDB file, downloaded from Palm handheld
   in this synchronization cycle.
   @param[in] dataDir path to directory, where PDB files from previous
   synchronization is stored.
   @param[in] prevPdbFName designated filename for PDB file from previous
   synchronization cycle.
   @return Zero on success, otherwise non-zero value will be returned.
*/
static int _save_as_previous_pdb(char ** pathToPrevPDB, char * pathToCurrentPDB,
						  char * dataDir, char * prevPdbFname)
{
	if(*pathToPrevPDB == NULL)
	{
		if((*pathToPrevPDB = calloc(
				strlen(dataDir) + strlen(prevPdbFname) + 1,
				sizeof(char))) == NULL)
		{
			log_write(LOG_ERR, "Failed to allocate memory for %s filepath",
					  prevPdbFname);
			return -1;
		}
		strncpy(*pathToPrevPDB, dataDir, strlen(dataDir));
		strcat(*pathToPrevPDB, prevPdbFname);
		log_write(LOG_DEBUG, "Constructed next file path: %s - to store PDB file as from prev sync",
				  *pathToPrevPDB);
	}

	if(_cp(pathToCurrentPDB, *pathToPrevPDB))
	{
		log_write(LOG_ERR, "Failed copy %s to %s: %s", pathToCurrentPDB,
				  *pathToPrevPDB, strerror(errno));
		return -1;
	}
	log_write(LOG_DEBUG, "Copy %s to %s", pathToCurrentPDB, *pathToPrevPDB);
	return 0;
}

/**
   Simple realization of cp here.

   @param[in] from Path for copy source.
   @param[in] to Path to copy target.
   @return Zero on success, non-zero value on error.
*/
static int _cp(char * from, char * to)
{
	int fromFd = -1;
	int toFd = -1;

	if((fromFd = open(from, O_RDONLY)) < 0)
	{
		log_write(LOG_ERR, "Cannot open %s to copy", from);
		return -1;
	}
	if((toFd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
					S_IRUSR | S_IWUSR | S_IRGRP)) < 0)
	{
		log_write(LOG_ERR, "Cannot open %s as copy target", to);
		goto cp_error;
	}

	uint8_t buffer[COPY_BUFFER_LENGTH];
	ssize_t readed;
	while(readed = read(fromFd, buffer, COPY_BUFFER_LENGTH), readed > 0)
	{
		uint8_t * outBuffer = buffer;
		ssize_t written;

		do
		{
			written = write(toFd, outBuffer, readed);
			if(written >= 0)
			{
				readed -= written;
				outBuffer += written;
			}
			else if(errno != EINTR)
			{
				goto cp_error;
			}
		}
		while(readed > 0);
	}

	if(readed == 0)
	{
		if(close(toFd))
		{
			log_write(LOG_ERR, "Cannot close %s file", to);
			toFd = -1;
			goto cp_error;
		}
		if(close(fromFd))
		{
			log_write(LOG_ERR, "Cannot close %s file", from);
			return -1;
		}
		return 0;
	}

cp_error:
	if(toFd != -1 && close(toFd))
	{
		log_write(LOG_ERR, "Cannot close %s file", to);
	}
	if(close(fromFd))
	{
		log_write(LOG_ERR, "Cannot close %s file", from);
	}
	return -1;
}
