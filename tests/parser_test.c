#include <stddef.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "orgmode_parser.h"
#include "parser.h"


int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	OrgModeEntries * parseResult;
	if((parseResult = parse_orgmode_file(argv[1])) == NULL)
	{
		return 1;
	}

	OrgModeEntry * entry;
	TAILQ_FOREACH(entry, parseResult, pointers)
	{
		log_write(LOG_INFO, "---");
		log_write(LOG_INFO, "Header: %s", entry->header);
		switch(entry->keyword)
		{
		case TODO:
			log_write(LOG_INFO, "TODO-keyword: TODO");
			break;
		case VERIFIED:
			log_write(LOG_INFO, "TODO-keyword: VERIFIED");
			break;
		case DONE:
			log_write(LOG_INFO, "TODO-keyword: DONE");
			break;
		case CANCELLED:
			log_write(LOG_INFO, "TODO-keyword: CANCELLED");
			break;
		case NO_TODO_KEYWORD:
			log_write(LOG_INFO, "TODO-keyword: no keyword");
			break;
		}
		log_write(LOG_INFO, "Tag: %s", entry->tag);
		switch(entry->priority)
		{
		case A:
			log_write(LOG_INFO, "Priority: A");
			break;
		case B:
			log_write(LOG_INFO, "Priority: B");
			break;
		case C:
			log_write(LOG_INFO, "Priority: C");
			break;
		case NO_PRIORITY:
			log_write(LOG_INFO, "Priority: no priority");
			break;
		}
		log_write(LOG_INFO, "Text: %s", entry->text);
		if(entry->datetime2 != (time_t)-1)
		{
			char datetime1[26];
			char datetime2[26];
			if(ctime_r(&entry->datetime1, datetime1) == NULL)
			{
				log_write(LOG_ERR, "Cannot convert timestamp %lu to string",
						  entry->datetime1);
				return -1;
			}
			if(ctime_r(&entry->datetime2, datetime2) == NULL)
			{
				log_write(LOG_ERR, "Cannot convert timestamp %lu to string",
						  entry->datetime2);
				return -1;
			}
			datetime1[strlen(datetime1) - 1] = '\0';
			datetime2[strlen(datetime2) - 1] = '\0';
			log_write(LOG_INFO, "Time range: %s-%s", datetime1, datetime2);
		}
		else if(entry->datetime1 != (time_t)-1)
		{
			char datetime[26];
			if(ctime_r(&entry->datetime1, datetime) == NULL)
			{
				log_write(LOG_ERR, "Cannot convert timestamp %lu to string",
						  entry->datetime1);
				return -1;
			}
			datetime[strlen(datetime) - 1] = '\0';
			log_write(LOG_INFO, "Time: %s", datetime);
		}
		if(entry->repeaterRange != NO_RANGE)
		{
			switch(entry->repeaterRange)
			{
			case HOUR:
				log_write(LOG_INFO, "Repeater interval: +%dh", entry->repeaterValue);
				break;
			case DAY:
				log_write(LOG_INFO, "Repeater interval: +%dd", entry->repeaterValue);
				break;
			case WEEK:
				log_write(LOG_INFO, "Repeater interval: +%dw", entry->repeaterValue);
				break;
			case MONTH:
				log_write(LOG_INFO, "Repeater interval: +%dm", entry->repeaterValue);
				break;
			case YEAR:
				log_write(LOG_INFO, "Repeater interval: +%dy", entry->repeaterValue);
				break;
			default:
				log_write(LOG_ERR, "Unknown repeater interval: %d", entry->repeaterRange);
			}
		}
	}

	free_orgmode_parser(parseResult);
	log_close();
	return 0;
}
