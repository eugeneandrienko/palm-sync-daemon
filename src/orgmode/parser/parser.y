%{
#define _XOPEN_SOURCE 500
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "orgmode_parser.h"

#define LINE_LEN 200
#define EMPTY_PRIORITY '-'


extern int yylex();
extern int yyparse();
extern FILE * yyin;

void yyerror(const char* s);

static OrgModeEntries * entries;
static OrgModeEntry * entry;

static char parsedLine[LINE_LEN];
static char * pointerLine;

static int _create_entry(const char * header, const char * keyword, const char priority, const char * tag);
static int _insert_text(const char * text);
static int _append_text(const char * text);
static int _insert_datetime(const char * datetime);
%}

%union {
    char * keyword;
    char priority;
    char * tag;
    char * word;
    char * datetime;
}

%token T_HEADLINE_STAR
%token <keyword> T_TODO_KEYWORD
%token <priority> T_PRIORITY
%token <tag> T_TAG
%token <word> T_WORD
%token T_SCHEDULED
%token <datetime> T_DATETIME;
%token T_NEWLINE

%start entries
%%
entries : entry
        | entries entry
        ;

entry : header
      | header text
      ;

header : headline T_NEWLINE
       | headline T_NEWLINE T_SCHEDULED T_DATETIME T_NEWLINE
       {
          if(_insert_datetime($4))
          {
              YYERROR;
          }
       }
       ;

headline : T_HEADLINE_STAR line
         {
            if(_create_entry(parsedLine, NULL, EMPTY_PRIORITY, NULL))
            {
                YYERROR;
            }
         }
         | T_HEADLINE_STAR line T_TAG
         {
            if(_create_entry(parsedLine, NULL, EMPTY_PRIORITY, $3))
            {
                YYERROR;
            }
         }
         | T_HEADLINE_STAR T_TODO_KEYWORD line
         {
            if(_create_entry(parsedLine, $2, EMPTY_PRIORITY, NULL))
            {
                YYERROR;
            }
         }
         | T_HEADLINE_STAR T_TODO_KEYWORD line T_TAG
         {
            if(_create_entry(parsedLine, $2, EMPTY_PRIORITY, $4))
            {
                YYERROR;
            }
         }
         | T_HEADLINE_STAR T_PRIORITY line
         {
            if(_create_entry(parsedLine, NULL, $2, NULL))
            {
                YYERROR;
            }
         }
         | T_HEADLINE_STAR T_PRIORITY line T_TAG
         {
            if(_create_entry(parsedLine, NULL, $2, $4))
            {
                YYERROR;
            }
         }
         | T_HEADLINE_STAR T_TODO_KEYWORD T_PRIORITY line
         {
            if(_create_entry(parsedLine, $2, $3, NULL))
            {
                YYERROR;
            }
         }
         | T_HEADLINE_STAR T_TODO_KEYWORD T_PRIORITY line T_TAG
         {
            if(_create_entry(parsedLine, $2, $3, $5))
            {
                YYERROR;
            }
         }
         ;

text : line T_NEWLINE
     {
        _insert_text(parsedLine);
     }
     | text T_NEWLINE
     {
        _append_text("");
     }
     | text line T_NEWLINE
     {
        _append_text(parsedLine);
     }
     ;

line : T_WORD
     {
        memset(parsedLine, 0, LINE_LEN);
        pointerLine = parsedLine;
        strncpy(pointerLine, $1, strlen($1));
        pointerLine += strlen($1);
     }
     | line T_WORD
     {
        *pointerLine = ' ';
        pointerLine++;
        strncpy(pointerLine, $2, strlen($2));
        pointerLine += strlen($2);
     }
     ;
%%
OrgModeEntries * parse_orgmode_file(const char * path)
{
    if(access(path, R_OK))
    {
        log_write(LOG_ERR, "No access to %s OrgMode file: %s", path,
				  strerror(errno));
        return NULL;
    }

    if((yyin = fopen(path, "r")) == NULL)
    {
        log_write(LOG_ERR, "No access to %s OrgMode file, cannot open: %s",
				  path, strerror(errno));
        return NULL;
    }

    if((entries = calloc(1, sizeof(OrgModeEntries))) == NULL)
    {
        log_write(LOG_ERR, "Cannot allocate memory for parsed org mode entries",
				  strerror(errno));
        fclose(yyin);
        return NULL;
    }

    TAILQ_INIT(entries);
    entry = NULL;

    do
    {
        yyparse();
    }
    while(!feof(yyin));

    if(fclose(yyin))
    {
        log_write(LOG_ERR, "Cannot close OrgMode file %s: %s",
				  path, strerror(errno));
    }
    return entries;
}

void free_orgmode_parser(OrgModeEntries * entries)
{
    if(entries == NULL)
    {
        return;
    }
    struct OrgModeEntry * entry1 = TAILQ_FIRST(entries);
    struct OrgModeEntry * entry2;
    while(entry1 != NULL)
    {
        entry2 = TAILQ_NEXT(entry1, pointers);
        free(entry1->header);
        if(entry1->tag != NULL)
        {
            free(entry1->tag);
        }
        if(entry1->text != NULL)
        {
            free(entry1->text);
        }
        free(entry1);
        entry1 = entry2;
    }
    TAILQ_INIT(entries);
    free(entries);
    entries = NULL;
}

void yyerror(const char* s)
{
    extern int yylineno;
    extern char * yytext;
    log_write(LOG_ERR, "OrgMode parse error: %s at line %d, symbols: \"%s\"",
			  s, yylineno, yytext);
	exit(1);
}

static int _create_entry(const char * header, const char * keyword, const char priority, const char * tag)
{
    if((entry = calloc(1, sizeof(OrgModeEntry))) == NULL)
    {
        log_write(LOG_ERR, "Cannot allocate memory for new OrgMode entry");
        return -1;
    }

    /* Insert header */
    if(header == NULL)
    {
        log_write(LOG_ERR, "Got NULL entry header");
        free(entry);
        return -1;
    }
    if((entry->header = strdup(header)) == NULL)
    {
        log_write(LOG_ERR, "Cannot copy new header \"%s\" to memory: %s",
				  header, strerror(errno));
        return -1;
    }

    /* Insert key (if exists) */
    if(keyword != NULL)
    {
        if(strlen(keyword) == strlen("TODO"))
        {
            if(!strcmp("TODO", keyword))
            {
                entry->keyword = TODO;
            }
            else if(!strcmp("DONE", keyword))
            {
                entry->keyword = DONE;
            }
            else
            {
                log_write(LOG_WARNING, "Unknown TODO keyword: %s", keyword);
                entry->keyword = NO_TODO_KEYWORD;
            }
        }
        else if((strlen(keyword) == strlen("VERIFIED")) &&
				!strcmp("VERIFIED", keyword))
        {
            entry->keyword = VERIFIED;
        }
        else if((strlen(keyword) == strlen("CANCELLED")) &&
				!strcmp("CANCELLED", keyword))
        {
            entry->keyword = CANCELLED;
        }
        else
        {
            log_write(LOG_WARNING, "Unknown TODO-keyword: %s", keyword);
            entry->keyword = NO_TODO_KEYWORD;
        }
    }
    else
    {
        entry->keyword = NO_TODO_KEYWORD;
    }

    /* Insert priority (if exists) */
    switch(priority)
    {
    case 'A':
        entry->priority = A;
        break;
    case 'B':
        entry->priority = B;
        break;
    case 'C':
        entry->priority = C;
        break;
    case EMPTY_PRIORITY:
        entry->priority = NO_PRIORITY;
        break;
    default:
        log_write(LOG_WARNING, "Unknown priority: %c", priority);
        entry->priority = NO_PRIORITY;
    }

    /* Insert tag (if exists) */
    if(tag != NULL)
    {
        if((entry->tag = strdup(tag)) == NULL)
        {
            log_write(LOG_ERR, "Cannot copy new tag \"%s\" to memory: %s",
                      tag, strerror(errno));
            return -1;
        }
    }
    else
    {
        entry->tag = NULL;
    }

    entry->text = NULL;
    entry->datetime1 = (time_t)-1;
    entry->datetime2 = (time_t)-1;
    entry->repeaterValue = 0;
    entry->repeaterRange = NO_RANGE;

    if(TAILQ_EMPTY(entries))
    {
        TAILQ_INSERT_HEAD(entries, entry, pointers);
    }
    else
    {
        TAILQ_INSERT_TAIL(entries, entry, pointers);
    }

    return 0;
}

static int _insert_text(const char * text)
{
    if(entry == NULL)
    {
        return -1;
    }
    if(entry->text != NULL)
    {
        free(entry->text);
    }
    if((entry->text = strdup(text)) == NULL)
    {
        log_write(LOG_ERR, "Cannot copy new text \"%s\" to memory: %s",
				  text, strerror(errno));
        return -1;
    }
    return 0;
}

static int _append_text(const char * text)
{
    if(entry == NULL)
    {
        return -1;
    }
    if(entry->text == NULL)
    {
        log_write(LOG_ERR, "Cannot append new text (\"%s\") - pointer to existing text in memory is NULL");
        return -1;
    }

    char * oldPointer = entry->text;
    size_t oldTextLen = strlen(oldPointer);

    /* 1st byte for \n, 2nd byte for trailing \0. */
    if((entry->text = realloc(oldPointer, oldTextLen + strlen(text) + 2)) == NULL)
    {
        log_write(LOG_ERR, "Cannot append new string \"%s\" to existing: %s",
				  text, strerror(errno));
        return -1;
    }
    entry->text[oldTextLen] = '\n';
    char * newStringPosition = entry->text + oldTextLen + 1;
    strncpy(newStringPosition, text, strlen(text));
    *(entry->text + oldTextLen + 1 + strlen(text)) = '\0';
    return 0;
}


static void __regerror(int regErrorCode, const regex_t * reg)
{
    char * buffer;
    size_t buflen;

    buflen = regerror(regErrorCode, reg, NULL, 0);
    if((buffer = calloc(buflen, sizeof(char))) == NULL)
    {
        log_write(LOG_ERR, "Cannot allocate buffer for error message: %s",
                  strerror(errno));
        return;
    }
    regerror(regErrorCode, reg, buffer, buflen);
    log_write(LOG_ERR, "Regex error: %s", buffer);
    free(buffer);
}

static int __insert_datetime(const char * datetime, int nsub, regmatch_t * regMatch, int regexNo)
{
    const char REGEX_N_SUBS[] = {5, 4, 3, 2, 1};
    if(REGEX_N_SUBS[regexNo] != nsub)
    {
        log_write(LOG_ERR, "Real regex subst groups count (%d) not matched with desired (%d)",
				  nsub, REGEX_N_SUBS[regexNo]);
        return -1;
    }

    time_t zerotime = 0;
    struct tm time;
    size_t matchlen;
    char * buffer;

    /* Extract date */
    if(nsub == 1)
    {
        if(regMatch[1].rm_so == -1)
        {
            log_write(LOG_ERR, "Cannot cut date from %s", datetime);
            return -1;
        }
        const char * zeroTime = " 00:00";
        matchlen = regMatch[1].rm_eo - regMatch[1].rm_so;
        buffer = calloc(matchlen + strlen(zeroTime), sizeof(char));
        strncpy(buffer, datetime + regMatch[1].rm_so, matchlen);
        strncpy(buffer + matchlen, zeroTime, strlen(zeroTime));

        localtime_r(&zerotime, &time);
        if(strptime(buffer, "%Y-%m-%d %H:%M", &time) == NULL)
        {
            log_write(LOG_ERR, "Cannot parse \"%s\" date", buffer);
            free(buffer);
            return -1;
        }
        if((entry->datetime1 = mktime(&time)) == (time_t)-1)
        {
            log_write(LOG_ERR, "Fail convert \"%s\" to Unix-time: %s", buffer,
					  strerror(errno));
            free(buffer);
            return -1;
        }
        free(buffer);
    }
    /* Extract datetime */
    else
    {
        if(regMatch[1].rm_so == -1 || regMatch[2].rm_so == -1)
        {
            log_write(LOG_ERR, "Cannot cut datetime from %s", datetime);
            return -1;
        }
        matchlen = (regMatch[1].rm_eo - regMatch[1].rm_so) +
            (regMatch[2].rm_eo - regMatch[2].rm_so) + 1 /* For space symbol */;
        buffer = calloc(matchlen, sizeof(char));
        matchlen = regMatch[1].rm_eo - regMatch[1].rm_so;
        strncpy(buffer, datetime + regMatch[1].rm_so, matchlen);

        *(buffer + matchlen) = ' ';

        size_t matchlen2 = regMatch[2].rm_eo - regMatch[2].rm_so;
        strncpy(buffer + matchlen + 1, datetime + regMatch[2].rm_so, matchlen2);

        localtime_r(&zerotime, &time);
        if(strptime(buffer, "%Y-%m-%d %H:%M", &time) == NULL)
        {
            log_write(LOG_ERR, "Cannot parse \"%s\" datetime", buffer);
            free(buffer);
            return -1;
        }
        if((entry->datetime1 = mktime(&time)) == (time_t)-1)
        {
            log_write(LOG_ERR, "Fail convert \"%s\" to Unix-time: %s", buffer,
					  strerror(errno));
            free(buffer);
            return -1;
        }
        free(buffer);
    }

    /* Extract second part of time range */
    if(nsub == 3 || nsub == 5)
    {
        if(regMatch[1].rm_so == -1 || regMatch[3].rm_so == -1)
        {
            log_write(LOG_ERR, "Cannot cut datetime from %s", datetime);
            return -1;
        }
        matchlen = (regMatch[1].rm_eo - regMatch[1].rm_so) +
            (regMatch[3].rm_eo - regMatch[3].rm_so) + 1 /* For space symbol */;
        buffer = calloc(matchlen, sizeof(char));
        matchlen = regMatch[1].rm_eo - regMatch[1].rm_so;
        strncpy(buffer, datetime + regMatch[1].rm_so, matchlen);

        *(buffer + matchlen) = ' ';

        size_t matchlen2 = regMatch[3].rm_eo - regMatch[3].rm_so;
        strncpy(buffer + matchlen + 1, datetime + regMatch[3].rm_so, matchlen2);

        localtime_r(&zerotime, &time);
        if(strptime(buffer, "%Y-%m-%d %H:%M", &time) == NULL)
        {
            log_write(LOG_ERR, "Cannot parse \"%s\" datetime", buffer);
            free(buffer);
            return -1;
        }
        if((entry->datetime2 = mktime(&time)) == (time_t)-1)
        {
            log_write(LOG_ERR, "Fail convert \"%s\" to Unix-time: %s", buffer,
					  strerror(errno));
            free(buffer);
            return -1;
        }
        free(buffer);
    }

    /* Extract repetitive interval */
    if(nsub == 4 || nsub == 5)
    {
        unsigned char repeaterValuePos = nsub == 4 ? 3 : 4;
        unsigned char repeaterRangePos = repeaterValuePos + 1;

        if(regMatch[repeaterValuePos].rm_so == -1 ||
		   regMatch[repeaterRangePos].rm_so == -1)
        {
            log_write(LOG_ERR, "Cannot cut repetitive interval from: %s", datetime);
            return -1;
        }
        matchlen = regMatch[repeaterValuePos].rm_eo - regMatch[repeaterValuePos].rm_so;
        buffer = calloc(matchlen, sizeof(char));
        strncpy(buffer, datetime + regMatch[repeaterValuePos].rm_so, matchlen);
        entry->repeaterValue = atoi(buffer);
        free(buffer);

        matchlen = regMatch[repeaterRangePos].rm_eo - regMatch[repeaterRangePos].rm_so;
        buffer = calloc(matchlen, sizeof(char));
        strncpy(buffer, datetime + regMatch[repeaterRangePos].rm_so, matchlen);
        switch(buffer[0])
        {
        case 'h':
            entry->repeaterRange = HOUR;
            break;
        case 'd':
            entry->repeaterRange = DAY;
            break;
        case 'w':
            entry->repeaterRange = WEEK;
            break;
        case 'm':
            entry->repeaterRange = MONTH;
            break;
        case 'y':
            entry->repeaterRange = YEAR;
            break;
        default:
            log_write(LOG_ERR, "Unknown repeater range \"%s\" in \"%s\"",
					  buffer, datetime);
            free(buffer);
            return -1;
        }
        free(buffer);
    }

    return 0;
}

static int _insert_datetime(const char * datetime)
{
    const unsigned char REGEX_QTY = 5;
    char * regexToCheck[] = {
        /* Datetime with range and repetitive interval */
        "([0-9]{4}-[0-9]{2}-[0-9]{2}) .+ ([0-9]{2}:[0-9]{2})-([0-9]{2}:[0-9]{2}) \\+([0-9]+)([hdwmy])",
        /* Datetime with repetitive interval */
        "([0-9]{4}-[0-9]{2}-[0-9]{2}) .+ ([0-9]{2}:[0-9]{2}) \\+([0-9]+)([hdwmy])",
        /* Datetime with range */
        "([0-9]{4}-[0-9]{2}-[0-9]{2}) .+ ([0-9]{2}:[0-9]{2})-([0-9]{2}:[0-9]{2})",
        /* Datetime */
        "([0-9]{4}-[0-9]{2}-[0-9]{2}) .+ ([0-9]{2}:[0-9]{2})",
        /* Date */
        "([0-9]{4}-[0-9]{2}-[0-9]{2}) .+"
    };

    for(int i = 0; i < REGEX_QTY; i++)
    {
        regex_t regex;
        regmatch_t * regMatch;
        int regError;

        if((regError = regcomp(&regex, regexToCheck[i], REG_EXTENDED | REG_NEWLINE)) != 0)
        {
            __regerror(regError, &regex);
            return -1;
        }
        if((regMatch = calloc((regex.re_nsub + 1), sizeof(regmatch_t))) == NULL)
        {
            log_write(LOG_ERR, "Cannot allocate memory for regex matching groups when parsing datetime: %s",
                      strerror(errno));
            regfree(&regex);
            return -1;
        }

        if((regError = regexec(&regex, datetime, regex.re_nsub + 1, regMatch, 0)) != 0)
        {
            if(regError == REG_NOMATCH)
            {
                /* Continue to check for next regex */
                free(regMatch);
                regfree(&regex);
                continue;
            }
            else
            {
                __regerror(regError, &regex);
                free(regMatch);
                regfree(&regex);
                return -1;
            }
        }

        if(__insert_datetime(datetime, regex.re_nsub, regMatch, i))
        {
            free(regMatch);
            regfree(&regex);
            return -1;
        }
        free(regMatch);
        regfree(&regex);
        return 0;
    } /* for(int i = 0; i < REGEX_QTY; i++) */

    log_write(LOG_ERR, "\"%s\" is not a valid OrgMode timestamp", datetime);
    return 1;
}
