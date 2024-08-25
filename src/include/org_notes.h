/**
   @author Eugene Andrienko
   @brief Operates with OrgMode file with notes on disk
   @file org_notes.h

   Reading and parsing OrgMode file use flex/bison parser functions. Writing to
   OrgMode file realized separately, via read_chunks() and write_chunks()
   functions.
*/

/**
   @page org_notes Process notes in OrgMode file

   To read and parse notes from OrgMode file — use org_notes_parse(), which
   returns initialized queue of OrgNote entries with parsed notes. When this
   structure is no longer necessary — free it via org_notes_free().

   To write new note to OrgMode file — use set of functions:
   - org_notes_open() — to open file for writing
   - org_notes_write() — to write new note to file
   - org_notes_close() — to close previously opened file.
*/

#ifndef _ORG_NOTES_H_
#define _ORG_NOTES_H_

#include <stdint.h>
#include <sys/queue.h>


/**
   Note for Emacs OrgMode file.
*/
struct OrgNote
{
	char * header;        /**< Note header */
	char * text;          /**< Note text. May be NULL of no text in note. */
	char * category;      /**< String with note category. May be NULL if
							 category not specified. */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	TAILQ_ENTRY(OrgNote) pointers;
#endif
	uint64_t header_hash; /**< Note header hash */
};
#ifndef DOXYGEN_SHOULD_SKIP_THIS
TAILQ_HEAD(NotesQueue, OrgNote);
#endif
typedef struct OrgNote OrgNote;
/**
   Queue of OrgNote elements.
*/
typedef struct NotesQueue OrgNotes;

/**
   Parse given OrgMode file with notes inside.

   @param[in] path Path to OrgMode file to parse.
   @return Pointer to queue with parsed notes or NULL if error.
*/
OrgNotes * org_notes_parse(const char * path);

/**
   Free inner structures and queue with parsed notes.

   @param[in] notes Pointer to queue to free.
*/
void org_notes_free(OrgNotes * notes);

/**
   Open OrgMode file to write.

   @param[in] path Path to OrgMode file.
   @return File descriptor on success or -1 on errror.
*/
int org_notes_open(const char * path);

/**
   Write given note to file.

   If category is NULL — write note without category. If text is NULL — write
   only header without additional text.

   @param[in] fd File descriptor of OrgMode file.
   @param[in] header Header for OrgMode record.
   @param[in] text Text for OrgMode record. May be NULL if there is no text for
   note.
   @param[in] category Text name of category. May be NULL if there is no
   category.
   @return 0 on success or non-zero value on error.
*/
int org_notes_write(int fd, char * header, char * text, char * category);

/**
   Close OrgMode file, opened for writing.

   @param[in] fd File descriptor to close.
   @return 0 on success or non-zero value on error.
*/
int org_notes_close(int fd);

#endif
