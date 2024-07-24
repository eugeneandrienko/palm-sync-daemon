/**
   @author Eugene Andrienko
   @brief Module to operate with Memos database in PDB file.
   @file pdb_memos.h

   This module can be used to open PDB file from Memos application and read it's
   structure to PDB structure — use pdb_memos_read() function. It opens the file
   from specified path, reads it's contents and close the file (via pdb_free()
   function).

   This function and other functions in module doesn't operate with file
   descriptors. File will be immediately closed after read/write or error.

   Memory for PDB structure will be allocated inside pdb_memos_read() function
   by pdb_read() function.

   To write PDB file — use pdb_memos_write() function. Memory for PDBMemos
   structure will be freed inside this function (after call of pdb_free()
   function).

   To work with PDBMemos structure use pdb_memos_memo_get(),
   pdb_memos_memo_add(), pdb_memos_memo_edit() or pdb_memos_memo_delete()
   functions. These functions will not break the underlying data structures if
   they return some errors!
*/

/**
   @page pdb_memos Edit PDB data from Memos

   This module can operate with memos from PDB file with Memos database inside.

   There are two main functions:
   - pdb_memos_read() - reads memos from given file to PDB structure
   - pdb_memos_write() - writes memos from PDB structure to given file.

   Memory for memos will be allocated inside pdb_memos_read() and will be freed
   inside pdb_memos_write() functions (by the call of pdb_free() function).

   To operate with PDBMemos structure there are the next functions:
   - pdb_memos_memo_get()
   - pdb_memos_memo_add()
   - pdb_memos_memo_edit()
   - pdb_memos_memo_delete()
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
	char * header; /**< Header of memo */
	char * text;   /**< Memo text */
};
typedef struct PDBMemo PDBMemo;


/**
   Read data from MemosDB file to PDB->PDBRecord->data fields.

   Function will open file, and read data from file to data field in
   corresponding PDBRecord structure in PDB structure. After that file will be
   closed (as unnecessary).

   Every memo will be read by it's offset from record list (see PDBRecord).

   @param[in] path Path to file with MemosDB.
   @return Pointer to initialized PDB structure with filled
   application-specific data. Or NULL on error.
*/
PDB * pdb_memos_read(char * path);

/**
   Write data from PDB structure to MemosDB file.

   Function will open file, write data from PDB structure to file and free the
   structure. After that, file will be closed.

   @param[in] path Path to file for writing.
   @param[in] memos Pointer to PDBFile structure with data inside.
   @return 0 on success or non-zero if error.
*/
int pdb_memos_write(char * path, PDB * memos);

/**
   Emergency clear of filled PDB structure with all allocated memory. Call it
   only on irreversible errors, when writing PDB structure to PDB file with
   proper cleaning is impossible.

   @param[in] pdb PDB structure for emergency cleaning.
*/
void pdb_memos_free(PDB * pdb);

/**
   Returns memo from PDB structure, if exists.

   Will be search memo by it's header. If multiple memos with same header exists
   — first found memo will be returned. If no memo with given header found —
   NULL value will be returned.

   @param[in] pdb Initialized PDB structure.
   @param[in] header Will search memo with this header.
   @return Pointer to memo or NULL if no memo found or error occured.
*/
PDBMemo * pdb_memos_memo_get(PDB * pdb, char * header);

/**
   Add new memo to PDB structure.

   If there is no specified category and exists space for new category in header
   — it will be added to PDB header. If there are no memory for new category —
   function returns an error.

   New memo will be added to the end of existing memos list.

   If adding memo is failed — underlying structures will not be changed!

   @param[in] pdb Pointer to initialized PDB structure.
   @param[in] header Header of new memo.
   @param[in] text Text of new memo.
   @param[in] category Category name for memo.
   @return Pointer to new memo or NULL if error.
*/
PDBMemo * pdb_memos_memo_add(PDB * pdb, char * header, char * text,
							 char * category);

/**
   Edit existing memo inside PDB structure.

   If error is happened — no any underlying structures will be changed.

   @param[in] pdb Pointer to PDB structure to which new memo will be added.
   @param[in] memo Pointer to memo to edit.
   @param[in] header New header or NULL if we shouldn't change header.
   @param[in] text New text or NULL if we shouldn't change text.
   @param[in] category Category name or NULL if we shouldn't change category.
   @return 0 on success or non-zero value on error.
*/
int pdb_memos_memo_edit(PDB * pdb, PDBMemo * memo, char * header,
						char * text, char * category);

/**
   Delete existing memo.

   If error is happened — no any underlying structures will be changed.

   @param[in] pdb Pointer to PDB structure.
   @param[in] memo Pointer to memo to delete.
   @return 0 on success or if memo not found. Returns non-zero value on error.
*/
int pdb_memos_memo_delete(PDB * pdb, PDBMemo * memo);

#endif
