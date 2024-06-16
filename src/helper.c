#include <errno.h>
#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "helper.h"
#include "log.h"
#include "umash.h"


char * iconv_utf8_to_cp1251(char * string)
{
	iconv_t iconvfd;
	if((iconvfd = iconv_open("CP1251", "UTF8")) == (iconv_t)-1)
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
	if((iconvfd = iconv_open("UTF8", "CP1251")) == (iconv_t)-1)
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
