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

int org_notes_write(int fd, const char * header, const char * text,
					const char * category)
{
	unsigned int noteLen = strlen(header);
	noteLen += text != NULL ? strlen(text) : 0;
	noteLen += category != NULL ? strlen(category) : 0;
	noteLen += 4                     /* For "* " before header + "\n" after header */
		+ (category != NULL ? 6 : 0) /* For "\t\t:" before tag + ":" after tag */
		+ (text != NULL ? 2 : 0);    /* For "\n" after text */

	char * note;

	if((note = calloc(noteLen, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new OrgMode note: %s",
				  strerror(errno));
		return -1;
	}

	if(text != NULL && category != NULL)
	{
		snprintf(note, noteLen, "* %s\t\t:%s:\n%s\n", header, category, text);
	}
	else if(text != NULL)
	{
		snprintf(note, noteLen, "* %s\n%s\n", header, text);
	}
	else if(category != NULL)
	{
		snprintf(note, noteLen, "* %s\t\t:%s:\n", header, category);
	}
	else
	{
		snprintf(note, noteLen, "* %s\n", header);
	}

	if(write_chunks(fd, note, strlen(note)))
	{
		log_write(LOG_ERR, "Failed to write note to OrgMode file.\nNote: \"%s\"",
				  note);
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
