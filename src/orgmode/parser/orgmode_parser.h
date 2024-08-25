/**
   @author Eugene Andrienko
   @brief Basic structures for OrgMode parser
   @file orgmode_parser.h

   Basic structures for OrgMode parser. All parsed data will be saved in these
   structures for further processing.
*/

/**
   @page orgmode_parser Low-level OrgMode file parser

   Header file for OrgMode parser, based on Flex+Bison lexer/parser. Each
   first-level headline treated as separate OrgModeEntry. All contents of parsed
   OrgMode file will be stored in queue OrgModeEntries.

   All parsed text strings will be in UTF-8 encoding. All parsed datetime stamps
   will be stored as time_t value (zero value is a start of Unix epoch). If
   there are no datetime value for OrgMode first-level headline — there are will
   be (time_t) -1 instead of time values.

   For pure dates (without time) — timestamp will be stored as YYYY-MM-DD
   00:00:00.

   Assume what all datetimes in OrgMode file were written in the same timezone
   and this timezone is using on the Palm device.

   If parsing process is failed parse_orgmode_file() will return NULL instead of
   initialized and filled OrgModeEntries queue.

   To free initialized OrgModeEntries queue — use free_orgmode_parser() function.
*/

#ifndef _ORGMODE_PARSER_H_
#define _ORGMODE_PARSER_H_

#include <sys/queue.h>
#include <time.h>


/**
   Priorities for OrgMode headline.

   - A — max priority
   - B
   - C — min priority
*/
enum Priority
{
	A,          /**< [#A] priority. */
	B,          /**< [#B] priority. */
	C,          /**< [#C] priority. */
	NO_PRIORITY /**< No priority. */
};

/**
   TODO-keywords for OrgMode headline.
*/
enum TODOKeyword
{
	TODO,
	VERIFIED,       /**< "VERIFIED" keyword. */
	DONE,           /**< "DONE" keyword. */
	CANCELLED,      /**< "CANCELLED" keyword. */
	NO_TODO_KEYWORD /**< No any of TODO-keywords. */
};

/**
   Repeater range - letter (h|d|w|m|y) from repeater interval.
*/
enum RepeaterRange
{
	HOUR,    /**< "h" letter in repeater interval. */
	DAY,     /**< "d" letter in repeater interval. */
	WEEK,    /**< "w" letter in repeater interval. */
	MONTH,   /**< "m" letter in repeater interval. */
	YEAR,    /**< "y" letter in repeater interval. */
	NO_RANGE /**< No repeater interval. */
};

/**
   Emacs OrgMode headline contents.

   Includes data from Flex+Bison parser. If some data is not exists in
   first-level headline — here will be NULL in corresponding field.
*/
struct OrgModeEntry
{
	char * header;                    /**< OrgMode first-level header */
	enum Priority priority;           /**< First-level header priority */
	enum TODOKeyword keyword;         /**< TODO-keyword for first-level
										 header. */
	char * tag;                       /**< Tag of first-level header. May be
										 NULL if no exists. */
	char * text;                      /**< Text below first-level header. May be
										 NULL if no exists. */
	time_t datetime1;                 /**< Datetime or first part of time range
										 or (time_t)-1 if no datetime. */
	time_t datetime2;                 /**< Second part of time range. May be
										 (time_t)-1 if no time range on
										 datetime. */
	unsigned char repeaterValue;      /**< Repeater value or 0 if no repeater
										 interval. */
	enum RepeaterRange repeaterRange; /**< Repeater range. */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	TAILQ_ENTRY(OrgModeEntry) pointers;
#endif
};
#ifndef DOXYGEN_SHOULD_SKIP_THIS
TAILQ_HEAD(OrgModeEntries, OrgModeEntry);
typedef struct OrgModeEntry OrgModeEntry;
#endif

/**
   Queue of Emacs OrgMode first-level headlines.

   Contains OrgModeEntry entries.
*/
typedef struct OrgModeEntries OrgModeEntries;

/**
   Start parsing process.

   Contents of OrgMode file will be stored in OrgModeEntries queue.

   @param[in] path Path to OrgMode file.
   @return Initialized and filled OrgModeEntries structure or NULL if parsing
   failed.
*/
OrgModeEntries * parse_orgmode_file(const char * path);

/**
   Free initialized and filled OrgModeEntries structure.

   @param[in] entries OrgModeEntries structure, which should be freed.
*/
void free_orgmode_parser(OrgModeEntries * entries);

#endif
