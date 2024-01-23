/**
   @author Eugene Andrienko
   @brief Module to operate with PDB from Memos
   @file pdb_memos.h

   TODO
*/

/**
   @page pdb_memos Edit PDB data from Memos

   TODO
*/


#ifndef _PDB_MEMOS_H_
#define _PDB_MEMOS_H_

#include <stdint.h>
#include "pdb.h"


/**
   One memo from Memos application.
*/
struct PDBMemo
{
	char * header;                       /**< Header of memo */
	char * text;                         /**< Memo text */
	int categoryId;                      /**< ID of category (taken from header). Used only for simplicity — to do not work with header every time */
	char * categoryName;                 /**< Category name (points to category name from PDBRecord structure). */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	TAILQ_ENTRY(PDBMemo) pointers;       /**< Connection between elements in queue */
#endif
};
#ifndef DOXYGEN_SHOULD_SKIP_THIS
TAILQ_HEAD(MemosQueue, PDBMemo);
#endif
typedef struct PDBMemo PDBMemo;

/**
   Structure of parsed MemosDB.pdb file.
*/
struct PDBMemos
{
	PDBFile * header;        /**< PDB file header */
	struct MemosQueue memos; /**< Queue of memos from Memos application */
};
typedef struct PDBMemos PDBMemos;


/**
   Read data from MemosDB file to PDBMemos structure.

   Function will open file, allocate memory for PDBMemos structure and write
   data from file to this structure. After that the file will be closed.

   Every memo will be read by it's offset from record list (see PDBRecord).

   @param[in] path Path to file with MemosDB.
   @return Pointer to initialized PDBMemos structure or NULL on error.
*/
PDBMemos * pdb_memos_read(char * path);

/**
   Write data from PDBMemos structure to MemosDB file.

   Function will open file, write data from PDBMemos structure and free
   it. After that, file will be closed.

   @param[in] path Path to file for writing.
   @param[in] memos Pointer to PDBMemos structure with data inside.
   @return 0 on success or non-zero if error.
*/
int pdb_memos_write(char * path, PDBMemos * memos);

/**
   Free PDBMemos structure.

   This function should not be called if we read and then write PDBMemos
   structure to file. In that case PDBMemos structure will be free inside
   pdb_memos_write() function.

   This function should be called only when we call pdbm_memos_read() and do not
   want to call pdb_memos_write().

   @param[in] memos Pointer to PDBMemos structure to free.
*/
void pdb_memos_free(PDBMemos * memos);

/**
   Returns memo from PDBMemos structure, if exists.

   Will be search memo by it's header. If multiple memos with same header exists
   — first found memo will be returned. If no memo with given header found —
   NULL value will be returned.

   @param[in] memos Initialized PDBMemos structure.
   @param[in] header Will search memo with this header.
   @return Pointer to memo or NULL if no memo found or error occured.
*/
PDBMemo * pdb_memos_memo_get(PDBMemos * memos, char * header);

/**
   Add new memo to PDBMemos structure.

   If there is no specified category and exists space for new category in header
   — it will be added to PDB header. If there are no memory for new category —
   function returns an error.

   @param[in] memos Pointer to initialized PDBMemos structure.
   @param[in] header Header of new memo.
   @param[in] text Text of new memo.
   @param[in] category Category name for memo.
   @return 0 on success or non-zero value on error.
*/
int pdb_memos_memo_add(PDBMemos * memos, char * header, char * text,
				  char category[PDB_CATEGORY_LEN]);

/**
   Edit existing memo inside PDBMemos structure.

   @param[in] memo Pointer to memo to edit.
   @param[in] header New header or NULL if we shouldn't change header.
   @param[in] text New text or NULL if we shouldn't change text.
   @param[in] category Category name or NULL if we shouldn't change category.
   @return 0 on success or non-zero value on error.
*/
int pdb_memos_memo_edit(PDBMemo * memo, char * header, char * text,
				   char * category);

/**
   Delete existing memo.

   @param[in] memos Pointer to PDBMemos structure.
   @param[in] memo Pointer to memo to delete.
   @return 0 on success or if memo not found. Returns non-zero value on error.
*/
int pdb_memos_memo_delete(PDBMemos * memos, PDBMemo * memo);

#endif
