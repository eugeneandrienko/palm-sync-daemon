#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "helper.h"
#include "log.h"
#include "org_notes.h"
#include "orgmode_parser.h"
#include "parser.h"
#include "pdb/pdb.h"


OrgNotes * org_notes_parse(const char * path)
{
	OrgModeEntries * parseResult;
	if((parseResult = parse_orgmode_file(path)) == NULL)
	{
		return NULL;
	}

	OrgNotes * result;
	if((result = calloc(1, sizeof(OrgNotes))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for notes from OrgMode: %s",
				  strerror(errno));
		return NULL;
	}
	TAILQ_INIT(result);

	if(TAILQ_EMPTY(parseResult))
	{
		log_write(LOG_INFO, "No OrgMode entries in %s file", path);
		free_orgmode_parser(parseResult);
		return result;
	}

	OrgModeEntry * entry;
	TAILQ_FOREACH(entry, parseResult, pointers)
	{
		OrgNote * note;
		if((note = calloc(1, sizeof(OrgNote))) == NULL)
		{
			log_write(LOG_ERR, "Failed to allocate memory for note: \"%s\"",
					  entry->header);
			continue;
		}
		note->header = iconv_utf8_to_cp1251(entry->header);
		note->text = entry->text != NULL ? iconv_utf8_to_cp1251(entry->text) : NULL;
		note->category = entry->tag != NULL ? strdup(entry->tag) : NULL;
		note->header_hash = str_hash(note->header, strlen(note->header));

		if(TAILQ_EMPTY(result))
		{
			TAILQ_INSERT_HEAD(result, note, pointers);
		}
		else
		{
			TAILQ_INSERT_TAIL(result, note, pointers);
		}
	}

	free_orgmode_parser(parseResult);
	return result;
}

void org_notes_free(OrgNotes * notes)
{
	if(TAILQ_EMPTY(notes))
	{
		free(notes);
		return;
	}

	OrgNote * note;
	TAILQ_FOREACH(note, notes, pointers)
	{
		if(note->category != NULL)
		{
			free(note->category);
		}
		if(note->text != NULL)
		{
			free(note->text);
		}
		free(note->header);
		free(note);
	}
	free(notes);
	return;
}

int org_notes_open(const char * path)
{
	int fd;
	if((fd = open(path, O_WRONLY | O_APPEND)) == -1)
	{
		log_write(LOG_ERR, "Cannot open OrgMode file \"%s\" for writing: %s",
				  path, strerror(errno));
		return -1;
	}
	return fd;
}

int org_notes_write(int fd, char * header, char * text, char * category)
{
	char * conv_header = iconv_cp1251_to_utf8(header);
	char * conv_text = text != NULL ? iconv_cp1251_to_utf8(text) : NULL;
	if(category != NULL && strncmp(PDB_DEFAULT_CATEGORY, category,
								   strlen(PDB_DEFAULT_CATEGORY)) == 0)
	{
		category = NULL;
	}

	unsigned int noteLen = strlen(conv_header);
	noteLen += conv_text != NULL ? strlen(conv_text) : 0;
	noteLen += category != NULL ? strlen(category) : 0;
	noteLen += 3                       /* For "* " before header + '\n' after header */
		+ (category != NULL ? 4 : 0)   /* For "\t\t:" before tag + ':' after tag */
		+ (conv_text != NULL ? 1 : 0)  /* For '\n' after text */
		+ 1;                           /* For '\0' at the end of string */

	char * note;

	if((note = calloc(noteLen, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new OrgMode note: %s",
				  strerror(errno));
		free(conv_header);
		if(conv_text != NULL)
		{
			free(conv_text);
		}
		return -1;
	}

	if(conv_text != NULL && category != NULL)
	{
		snprintf(note, noteLen, "* %s\t\t:%s:\n%s\n", conv_header, category, conv_text);
	}
	else if(conv_text != NULL)
	{
		snprintf(note, noteLen, "* %s\n%s\n", conv_header, conv_text);
	}
	else if(category != NULL)
	{
		snprintf(note, noteLen, "* %s\t\t:%s:\n", conv_header, category);
	}
	else
	{
		snprintf(note, noteLen, "* %s\n", conv_header);
	}

	if(write_chunks(fd, note, strlen(note)))
	{
		log_write(LOG_ERR, "Failed to write note to OrgMode file.\nNote: \"%s\"",
				  note);
		free(conv_header);
		if(conv_text != NULL)
		{
			free(conv_text);
		}
		return -1;
	}
	return 0;
}

int org_notes_close(int fd)
{
	if(close(fd) == -1)
	{
		log_write(LOG_ERR, "Cannot close OrgMode file: %s",
				  strerror(errno));
		return -1;
	}
	return 0;
}
