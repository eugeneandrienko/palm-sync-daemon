/**
   @author Eugene Andrienko
   @brief Different helper functions, reused in other modules
   @file helper.h
*/

/**
   @page helper Helper functions

   - iconv_utf8_to_cp1251() - convert given string from UTF8 to CP1251
   - iconv_cp1251_to_utf8() - convert given string from CP1251 to UTF8.
*/

#ifndef _HELPER_H_
#define _HELPER_H_


/**
   Buffer size for one chunk. Used in read_chunks() and write_chunks().
*/
#define CHUNK_SIZE 10


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
   Read text from file by chunks.

   @param[in] fd File descriptor.
   @param[in] buf Buffer for readed data.
   @param[in] length Length of buffer.
   @return 0 on successfull read or non-zero value if error.
*/
int read_chunks(int fd, char * buf, unsigned int length);

/**
   Write text to file by chunks.

   @param[in] fd File descriptor.
   @param[in] buf Buffer for data to write.
   @param[in] length Length of buffer.
   @return 0 on successfull write or non-zero value if error.
*/
int write_chunks(int fd, char * buf, unsigned int length);

#endif
