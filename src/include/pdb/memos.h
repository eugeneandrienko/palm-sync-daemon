/**
   @author Eugene Andrienko
   @brief Module to operate with Memos database from PDB file.
   @file memos.h

   This module can be used to open PDB file from Memos application and
   read it's structure to Memos structure — with memos_open() and
   memos_read() functions.

   To write changed Memos back to file — use memos_write()
   function. To close opened PDB file use memos_close(). To free
   memory, allocated inside Memos structure, use memos_free().

   To work with Memos structure use memos_memo_get(),
   memos_memo_add(), memos_memo_edit() or memos_memo_delete()
   functions.
*/

/**
   @page memos Operate with Memos from PDB file.

   This module can operate with memos from PDB file with Memos database inside.

   There are five main functions:
   - memos_open() - open PDB file with Memos inside.
   - memos_read() - reads memos from opened file to Memos structure.
   - memos_write() - writes memos from Memos structure to opened file.
   - memos_close() - close opened PDB file with Memos.
   - memos_free() - free memory, allocated inside Memos structure.

   To operate with PDBMemos structure there are the next functions:
   - memos_memo_get()
   - memos_memo_add()
   - memos_memo_edit()
   - memos_memo_delete()
*/


#ifndef _MEMOS_H_
#define _MEMOS_H_

#include <stdint.h>
#include <sys/queue.h>
#include "pdb/pdb.h"


/**
   One memo from Memos application.
*/
struct Memo
{
	char * header;              /**< Header of memo */
	char * text;                /**< Memo text */
	char * category;            /**< Memo category */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	TAILQ_ENTRY(Memo) pointers; /**< Connection between elements in tail
								   queue */
	PDBRecord * _record;         /**< PDB record for memo */
#endif
};
#ifndef DOXYGEN_SHOULD_SKIP_THIS
TAILQ_HEAD(MemosQueue, Memo);
#endif
typedef struct Memo Memo;
/**
   Double linked queue of Memo elements
*/
typedef struct MemosQueue MemosQueue;

/**
   Data from PDB file.
*/
struct Memos
{
	MemosQueue queue; /**< Memos queue */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	PDB * _pdb;       /**< PDB structure from file */
#endif
};
typedef struct Memos Memos;


/**
   \defgroup memos_ops Operate with Memos from corresponding PDB
   structure

   Set of functions to operate with PDB structure specific for Memos
   Palm application.

   @{
*/

/**
   Opens Memos PDB file and returns it's descriptor.

   @param[in] path Path to file in filesystem.
   @return File descriptor or -1 on error.
*/
int memos_open(const char * path);

/**
   Read memos from PDB file.

   Function will read data from file to Memos structure. Memory for
   Memos structure will be initialized inside this function and can be
   freed by memos_free().

   Every memo will be read by it's offset from record list (see PDBRecord).

   @param[in] fd File with memos descriptor.
   @return Memos structure on success, NULL on error.
*/
Memos * memos_read(int fd);

/**
   Write data from Memos structure to PDB file.

   Function will write data from Memos structure to file.

   @param[in] fd File with memos descriptor.
   @param[in] memos Pointer to Memos structure.
   @return 0 on success or non-zero if error.
*/
int memos_write(int fd, Memos * memos);

/**
   Close opened Memos file.

   @param[in] fd Opened Memos file descriptor.
*/
void memos_close(int fd);

/**
   Clear internal data structures of Memos structure.

   @param[in] memos Memos structure.
*/
void memos_free(Memos * memos);

/**
   @}
*/


/**
   \defgroup memo_ops Actions, specific to one memo entry

   Functions to operate with one memo from Memos application. This
   memo can be edited or deleted. Also a new memo can be created.

   @{
*/

/**
   Returns memo from Memos queue, if exists.

   Will be search memo by it's header. If multiple memos with same
   header exists — first found memo will be returned. If no memo with
   given header found — NULL value will be returned.

   @param[in] memos Memos structure.
   @param[in] header Will search memo with this header.
   @return Pointer to memo or NULL if no memo found or error occured.
*/
Memo * memos_memo_get(Memos * memos, char * header);

/**
   Add new memo.

   If there is no specified category and exists space for new category
   in header — it will be added as new category. If there are no
   memory for new category — function returns an error.

   If there are NULL category — default category will be used.

   New memo will be added to the end of existing memos list.

   @param[in] memos Memos structure.
   @param[in] header Header of new memo.
   @param[in] text Text of new memo. May be NULL if there are no text.
   @param[in] category Category name for memo. May be NULL — default Palm
   category will be used.
   @return Pointer to new memo or NULL if error.
*/
Memo * memos_memo_add(Memos * memos, char * header, char * text,
					  char * category);

/**
   Edit existing memo.

   @param[in] memos Memos structure.
   @param[in] memo Memo to edit.
   @param[in] header New header or NULL if we shouldn't change header.
   @param[in] text New text or NULL if we shouldn't change text.
   @param[in] category Category name or NULL if we shouldn't change
   category.
   @return 0 on success or non-zero value on error.
*/
int memos_memo_edit(Memos * memos, Memo * memo, char * header, char * text,
					char * category);

/**
   Delete existing memo.

   @param[in] memos Pointer to MemosQueue.
   @param[in] memo Pointer to memo to delete.
   @return 0 on success or returns non-zero value on error.
*/
int memos_memo_delete(Memos * memos, Memo * memo);

/**
   @}
*/

#endif
