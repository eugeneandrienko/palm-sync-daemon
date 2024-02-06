#include <stddef.h>
#include "log.h"
#include "org_notes.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 1);

	OrgNotes * notes;
	if((notes = org_notes_parse(argv[1])) == NULL)
	{
		log_write(LOG_ERR, "Cannot open %s file", argv[1]);
		return 1;
	}

	OrgNote * note;
	TAILQ_FOREACH(note, notes, pointers)
	{
		log_write(LOG_INFO, "Header: %s", note->header);
		if(note->text != NULL)
		{
			log_write(LOG_INFO, "Text: %s", note->text);
		}
		if(note->category != NULL)
		{
			log_write(LOG_INFO, "Category: %s", note->category);
		}
	}

	org_notes_free(notes);
	log_close();
	return 0;
}
