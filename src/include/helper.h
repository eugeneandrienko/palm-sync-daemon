/**
   @author Eugene Andrienko
   @brief Different helper functions, reused in other modules
   @file helper.h
*/

/**
   @page helper Helper functions

   - iconv_utf8_to_cp1251() - convert given string from UTF8 to CP1251
   - iconv_cp1251_to_utf8() - convert given string from CP1251 to UTF8
   - read_chunks() - read bytes from file by chunks
   - write_chunks() - write bytes to file by chunks
   - str_hash() - compute hash for given string
   - check_previous_pdbs() - check PDBs from previous synchronization
   cycle for existence
   - save_as_previous_pdbs() - save current set of PDBs as PDBs from previous
   synchronization cycle.
*/

#ifndef _HELPER_H_
#define _HELPER_H_

#include <stddef.h>
#include <stdint.h>
#include "palm.h"
#include "sync.h"


/**
   Buffer size for one chunk. Used in read_chunks() and write_chunks().
*/
#define CHUNK_SIZE 10


/**
   \defgroup iconv Character conversion

   Set of functions to perform character conversion for character
   arrays. Conversion can be performed from CP1251 to UTF8 and
   backwards, from UTF8 to CP1251.

   @{
*/

/**
   Convert given string from UTF8 to CP1251 encoding.

   Function will allocate (2 * symbols in UTF8 string) bytes for CP1251
   string. Memory for this string should be freed outside of this function.

   @param[in] string Characters in UTF8 encoding.
   @return Characters in CP1251 encoding. Memory for string allocated inside
   this functions. If error - NULL is returned.
*/
char * iconv_utf8_to_cp1251(char * string);

/**
   Convert given string from CP1251 to UTF8 encoding.

   Function will allocate (2 * symbols in CP1251 string) bytes for UTF8
   string. Memory for this string should be freed outside of this function.

   @param[in] string Characters in CP1251 encoding.
   @return Characters in UTF8 encoding. Memory for string allocated inside
   this functions. If error - NULL is returned.
*/
char * iconv_cp1251_to_utf8(char * string);

/**
   @}
*/


/**
   \defgroup chunk_rdwr Read/write chunks from file

   Functions to read/write chunks of specified size from given file
   descriptor.

   @{
*/

/**
   Read bytes from file by chunks.

   @param[in] fd File descriptor.
   @param[in] buf Buffer for readed data.
   @param[in] length Length of buffer.
   @return 0 on successfull read or non-zero value if error.
*/
int read_chunks(int fd, char * buf, unsigned int length);

/**
   Write bytes to file by chunks.

   @param[in] fd File descriptor.
   @param[in] buf Buffer for data to write.
   @param[in] length Length of buffer.
   @return 0 on successfull write or non-zero value if error.
*/
int write_chunks(int fd, char * buf, unsigned int length);

/**
   @}
*/

/**
   Compute hash for given string.

   @param[in] buf String to compute hash to.
   @param[in] length Length of string.
   @return Resulting hash.
*/
uint64_t str_hash(char * buf, size_t length);


/**
   \defgroup previous_pdbs Processing PDB files from previous synchronization
   cycle

   This set of functions intended to check and process PDB files,
   which are/will be files from previous synchronization cycle.

   @{
*/

/**
   Check for PDB files from previous synchronization iteration.

   Function check for PDB files existence and write paths to it in syncSettings
   structure. Or NULL if file not found.

   @param[in] syncSettings Synchronization settings structure with path to data
   directory inside.
   @return Zero on success, non-zero value if some of system calls are failed.
*/
int check_previous_pdbs(SyncSettings * syncSettings);

/**
   Save current PDB file as old PDB file.

   Save PDB file currently downloaded from Palm handheld as PDB file
   from previous synchronization iteration. File will be stored to
   daemon data directory.

   @param[in] syncSettings synchronization settings.
   @param[in] palmData structure with paths to PDB files, downloaded from
   handheld.
   @return Zero on success, otherwise non-zero value will be returned.
*/
int save_as_previous_pdbs(SyncSettings * syncSettings, PalmData * palmData);

/**
   @}
*/

#endif
