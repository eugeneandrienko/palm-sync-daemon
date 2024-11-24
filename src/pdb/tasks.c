#include <endian.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "helper.h"
#include "log.h"
#include "pdb/pdb.h"
#include "pdb/tasks.h"


static Task * _tasks_read_task(int fd, PDBRecord * record, PDB * pdb);
static int _tasks_append_task(int fd, PDBRecord * record, Tasks * tasks);
static Task * __parse_taskdb_data(int fd, uint8_t type);
static void _task_free(Task * task);
static void __task_clear_ptod(Task * task);
static int _tasks_write_task(TasksFD tfd, Task * task);


TasksFD tasks_open(const char * pathToDoDB, const char * pathTasksDB)
{
	TasksFD result = {
		.todo_fd = -1,
		.tasks_fd = -1
	};

	result.todo_fd = pdb_open(pathToDoDB);
	result.tasks_fd = pdb_open(pathTasksDB);

	if(result.todo_fd == -1)
	{
		log_write(LOG_ERR, "Cannot open %s PDB file", pathToDoDB);
	}
	if(result.tasks_fd == -1)
	{
		log_write(LOG_ERR, "Cannot open %s PDB file", pathTasksDB);
	}
	return result;
}

Tasks * tasks_read(TasksFD tfd)
{
	Tasks * tasks;
	if((tasks = calloc(1, sizeof(Tasks))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for Tasks: %s",
				  strerror(errno));
		return NULL;
	}

	if((lseek(tfd.todo_fd, 0, SEEK_CUR) != 0) &&
	   (lseek(tfd.todo_fd, 0, SEEK_SET) != 0))
	{
		log_write(LOG_ERR, "Cannot rewind to the start of ToDoDB file: %s",
				  strerror(errno));
		free(tasks);
		return NULL;
	}
	if((lseek(tfd.tasks_fd, 0, SEEK_CUR) != 0) &&
	   (lseek(tfd.tasks_fd, 0, SEEK_SET) != 0))
	{
		log_write(LOG_ERR, "Cannot rewind to the start of TasksDB-PTod file: %s",
				  strerror(errno));
		free(tasks);
		return NULL;
	}

	if((tasks->_pdb_tododb = pdb_read(tfd.todo_fd, true)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read PDB header from ToDoDB");
		free(tasks);
		return NULL;
	}
	if((tasks->_pdb_tasks = pdb_read(tfd.tasks_fd, true)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read PDB header from TasksDB-PTod");
		free(tasks);
		return NULL;
	}

	TAILQ_INIT(&tasks->queue);

	PDBRecord * record;
	/* Read tasks from ToDoDB PDB structure: */
	TAILQ_FOREACH(record, &tasks->_pdb_tododb->records, pointers)
	{
		Task * task;
		if((task = _tasks_read_task(
				tfd.todo_fd, record, tasks->_pdb_tododb)) == NULL)
		{
			log_write(LOG_ERR, "Error when reading tasks from ToDoDB. "
					  "Offset: %x", record->offset);
			tasks_free(tasks);
			return NULL;
		}
		if(TAILQ_EMPTY(&tasks->queue))
		{
			TAILQ_INSERT_HEAD(&tasks->queue, task, pointers);
		}
		else
		{
			TAILQ_INSERT_TAIL(&tasks->queue, task, pointers);
		}
	}
	/* Append info to tasks with data from TasksDB-PTod  structure */
	TAILQ_FOREACH(record, &tasks->_pdb_tasks->records, pointers)
	{
		if(_tasks_append_task(tfd.tasks_fd, record, tasks))
		{
			log_write(LOG_ERR, "Error when appending tasks from TasksDB-PTod. "
					  "Offset: 0x%08x", record->offset);
			tasks_free(tasks);
			return NULL;
		}
	}

	return tasks;
}

int tasks_write(TasksFD tfd, Tasks * tasks)
{
	if((lseek(tfd.todo_fd, 0, SEEK_CUR) != 0) &&
	   (lseek(tfd.todo_fd, 0, SEEK_SET) != 0))
	{
		log_write(LOG_ERR, "Cannot rewind to the start of ToDoDB file: %s",
				  strerror(errno));
		return -1;
	}
	if((lseek(tfd.tasks_fd, 0, SEEK_CUR) != 0) &&
	   (lseek(tfd.tasks_fd, 0, SEEK_SET) != 0))
	{
		log_write(LOG_ERR, "Cannot rewind to the start of TasksDB-PTod file: %s",
				  strerror(errno));
		return -1;
	}

	if(pdb_write(tfd.todo_fd, tasks->_pdb_tododb))
	{
		log_write(LOG_ERR, "Cannot write header to ToDoDB PDB file");
		return -1;
	}
	if(pdb_write(tfd.tasks_fd, tasks->_pdb_tasks))
	{
		log_write(LOG_ERR, "Cannot write header to TasksDB-PTod PDB file");
		return -1;
	}

	/* Write tasks to PDB files */
	Task * task;
	TAILQ_FOREACH(task, &tasks->queue, pointers)
	{
		if(_tasks_write_task(tfd, task))
		{
			log_write(LOG_ERR, "Failed to write tasak with header \"%s\" to "
					  "Tasks related PDB files", task->header);
			return -1;
		}
	}

	return 0;
}

void tasks_close(TasksFD tfd)
{
	if(close(tfd.todo_fd) == -1)
	{
		log_write(LOG_DEBUG, "Failed to close ToDoDB file descriptor: %s",
				  strerror(errno));
	}
	if(close(tfd.tasks_fd) == -1)
	{
		log_write(LOG_DEBUG, "Failed to close TasksDB-PTod file descriptor: "
				  "%s", strerror(errno));
	}
}

void tasks_free(Tasks * tasks)
{
	if(tasks == NULL)
	{
		log_write(LOG_WARNING, "Got empty Tasks structure - nothing to do");
		return;
	}
	Task * task1 = TAILQ_FIRST(&tasks->queue);
	Task * task2;
	while(task1 != NULL)
	{
		task2 = TAILQ_NEXT(task1, pointers);
		TAILQ_REMOVE(&tasks->queue, task1, pointers);
		_task_free(task1);
		task1 = task2;
	}
	pdb_free(tasks->_pdb_tododb);
	pdb_free(tasks->_pdb_tasks);
	free(tasks);
}


/* Functions to operate with task */

/**
   Element of array with tasks. Maps task header and pointer of task.

   Used to sort tasks by header and search for desired task by it's header.
*/
struct __SortedTasks
{
	char * header; /**< Pointer to Tasks header */
	Task * task;   /**< Pointer to Task structure */
};

static int __compare_headers(const void * rec1, const void * rec2)
{
	return strcmp(
		((const struct __SortedTasks *)rec1)->header,
		((const struct __SortedTasks *)rec2)->header);
}

Task * tasks_task_get(Tasks * tasks, char * header)
{
	int tasksQty = tasks->_pdb_tododb->recordsQty;
	struct __SortedTasks sortedTasks[tasksQty];

	Task * task;
	unsigned short index = 0;

	TAILQ_FOREACH(task, &tasks->queue, pointers)
	{
		sortedTasks[index].header = task->header;
		sortedTasks[index].task = task;
		index++;
	}

	if(tasksQty != index)
	{
		log_write(LOG_ERR, "Tasks count in PDB header: %d, real tasks "
				  "count: %d", tasksQty, index);
		return NULL;
	}

	qsort(&sortedTasks, tasksQty, sizeof(struct __SortedTasks),
		  __compare_headers);
	struct __SortedTasks searchFor = {header, NULL};
	struct __SortedTasks * searchResult = bsearch(
		&searchFor, &sortedTasks, tasksQty, sizeof(struct __SortedTasks),
		__compare_headers);
	return searchResult != NULL ? searchResult->task : NULL;
}


/* Local private functions */

/**
   Read task, pointed by PDBRecord, from file with given descriptor.

   @param[in] fd File descriptor.
   @param[in] record PDBRecord, which points to task.
   @param[in] pdb PDB structure with data from file.
   @return Task or NULL if error.
*/
static Task * _tasks_read_task(int fd, PDBRecord * record, PDB * pdb)
{
	/* Go to task.
	   Skip scheduled date and priority - will read it from
	   TasksDB-PTod database
	*/
	if(lseek(fd, record->offset + 3, SEEK_SET) != (record->offset + 3))
	{
		log_write(LOG_ERR, "Cannot go to 0x%08x offset in ToDoDB PDB file "
				  "to read task: %s", record->offset + 3, strerror(errno));
		return NULL;
	}

	char buffer[CHUNK_SIZE] = "\0";
	ssize_t readedBytes = 0;

	/* Calculate header size */
	unsigned int headerSize = 0;
	while((readedBytes = read(fd, buffer, CHUNK_SIZE)) > 0)
	{
		const char * headerEnd = memchr(buffer, '\0', readedBytes);
		if(headerEnd != NULL)
		{
			headerSize += headerEnd - buffer;
			/* "+1" to skip '\0' at the end of header */
			if(lseek(fd, -(readedBytes - (headerEnd - buffer)) + 1,
					 SEEK_CUR) == (off_t)-1)
			{
				log_write(LOG_ERR, "Cannot rewind to start of task header: %s",
						  strerror(errno));
				return NULL;
			}
			break;
		}
		headerSize += readedBytes;
	}
	if(readedBytes < 0)
	{
		log_write(LOG_ERR, "Failed to locate task header: %s", strerror(errno));
		return NULL;
	}

	/* Calculate text size */
	unsigned int textSize = 0;
	while((readedBytes = read(fd, buffer, CHUNK_SIZE)) > 0)
	{
		const char * textEnd = memchr(buffer, '\0', readedBytes);
		if(textEnd != NULL)
		{
			textSize += textEnd - buffer;
			if(lseek(fd, -(readedBytes - (textEnd - buffer)),
					 SEEK_CUR) == (off_t)-1)
			{
				log_write(LOG_ERR, "Cannot rewind to end of task note: %s",
						  strerror(errno));
				return NULL;
			}
			break;
		}
		textSize += readedBytes;
	}
	if(readedBytes < 0)
	{
		log_write(LOG_ERR, "Failed to locate task note: %s", strerror(errno));
		return NULL;
	}

	/* Rewinding to the start of the task's header */
	if(lseek(fd, record->offset + 3, SEEK_SET) != (record->offset + 3))
	{
		log_write(LOG_ERR, "Cannot rewind to start of task: %s",
				  strerror(errno));
		return NULL;
	}

	/* Allocate memory for task */
	Task * task;
	if((task = calloc(1, sizeof(Task))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for task at offset: 0x%08x:"
				  " %s", record->offset, strerror(errno));
		return NULL;
	}
	/* "+ 1" for three next callocs for null-termination bytes */
	if((task->header = calloc(headerSize + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for task header: %s",
				  strerror(errno));
		free(task);
		return NULL;
	}
	if(textSize != 0 && (task->text = calloc(textSize + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for task note: %s",
				  strerror(errno));
		free(task->header);
		free(task);
		return NULL;
	}
	if((task->category = calloc(PDB_CATEGORY_LEN, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for task category: %s",
				  strerror(errno));
		free(task->text);
		free(task->header);
		free(task);
		return NULL;
	}

	log_write(LOG_DEBUG, "Header size: %d, note size: %d", headerSize, textSize);

	/* Reading header */
	if(read_chunks(fd, task->header, headerSize))
	{
		log_write(LOG_ERR, "Cannot read task header");
		free(task->header);
		free(task->text);
		free(task->category);
		free(task);
		return NULL;
	}
	/* Skip '\0' at the end of header */
	if(lseek(fd, 1, SEEK_CUR) == (off_t)-1)
	{
		log_write(LOG_ERR, "Failed to skip NULL terminator between task header "
				  "and note: %s", strerror(errno));
		free(task->header);
		free(task->text);
		free(task->category);
		free(task);
		return NULL;
	}

	/* Reading text */
	if(textSize != 0 && read_chunks(fd, task->text, textSize))
	{
		log_write(LOG_ERR, "Cannot read task note");
		free(task->header);
		free(task->text);
		free(task->category);
		free(task);
		return NULL;
	}

	/* Reading category */
	char * categoryName = pdb_category_get_name(pdb, record->attributes & 0x0f);
	strncpy(task->category, categoryName, PDB_CATEGORY_LEN);

	task->_record_todo = record;
	return task;
}

/**
   Read additional task's data from file and append it to task.

   Additional data will be read from TasksDB-PTod PDB file.

   @param[in] fd File descriptor.
   @param[in] record PDBRecord which points to task's data.
   @param[in] tasks Initialized Tasks structure to append data to necessary
   task.
   @return 0 on success or -1 on error.
*/
static int _tasks_append_task(int fd, PDBRecord * record, Tasks * tasks)
{
	/* Go to task's data */
	if(lseek(fd, record->offset, SEEK_SET) != record->offset)
	{
		log_write(LOG_ERR, "Cannot go to 0x%08x offset in TasksDB-PTod "
				  "PDB file to append task's data: %s", record->offset,
				  strerror(errno));
		return -1;
	}

	/* Read task type */
	uint8_t type = 0;
	if(read(fd, &type, 1) != 1)
	{
		log_write(LOG_ERR, "Cannot read task type from TaskDB-PTod. "
				  "Unique record ID: %dl", pdb_record_get_unique_id(
					  record));
		return -1;
	}
	log_write(LOG_DEBUG, "Task type: 0x%02x", type);

	/* Skip four zero bytes */
	if(lseek(fd, 4, SEEK_CUR) == (off_t)-1)
	{
		log_write(LOG_ERR, "Cannot skip four zero bytes at the start "
				  "of task: %s", strerror(errno));
		return -1;
	}

	/* Read task priority */
	TaskPriority priority;
	uint8_t rawPriority = 0;
	if(read(fd, &rawPriority, 1) != 1)
	{
		log_write(LOG_ERR, "Cannot read task priority from TaskDB-PTod."
				  " Unique record ID: %dl", pdb_record_get_unique_id(
					  record));
		return -1;
	}
	log_write(LOG_DEBUG, "Task raw priority: %d", rawPriority);
	switch(rawPriority)
	{
	case 1:
		priority = PRIORITY_1;
		break;
	case 2:
		priority = PRIORITY_2;
		break;
	case 3:
		priority = PRIORITY_3;
		break;
	case 4:
		priority = PRIORITY_4;
		break;
	case 5:
		priority = PRIORITY_5;
		break;
	default:
		log_write(LOG_WARNING, "Read unexpected priority: %d ("
				  "offset: 0x%08x). Defaulting to priority = 1",
				  rawPriority, record->offset);
		priority = PRIORITY_1;
	}
	/* Because PRIORITY_1 == 0 */
	log_write(LOG_DEBUG, "Task priority: %d", priority + 1);

	/* Parse task data */
	Task * parsedTaskData = __parse_taskdb_data(fd, type);
	Task * task;
	if(parsedTaskData == NULL)
	{
		log_write(LOG_ERR, "Cannot parse task data. Offset: 0x%08x",
				  record->offset);
		return -1;
	}

	/* Get Task element corresponding to parsed data */
	if((task = tasks_task_get(tasks, parsedTaskData->header)) == NULL)
	{
		log_write(LOG_ERR, "Cannot find task with header `%s' in Tasks queue!",
				  parsedTaskData->header);
		_task_free(parsedTaskData);
		return -1;
	}
	log_write(LOG_DEBUG, "Found task with header: %s", parsedTaskData->header);

	/* Copy parsed data to Task element */
	task->priority = priority;
	if(parsedTaskData->dueDay != 0 && parsedTaskData->dueMonth != 0 &&
	   parsedTaskData->dueYear != 0)
	{
		task->dueDay = parsedTaskData->dueDay;
		task->dueMonth = parsedTaskData->dueMonth;
		task->dueYear = parsedTaskData->dueYear;
	}
	if(parsedTaskData->alarm != NULL)
	{
		if((task->alarm = calloc(1, sizeof(Alarm))) == NULL)
		{
			log_write(LOG_ERR, "Failed to allocate memory for `alarm' "
					  "structure: %s", strerror(errno));
			_task_free(parsedTaskData);
			__task_clear_ptod(task);
			return -1;
		}
		memcpy(task->alarm, parsedTaskData->alarm, sizeof(Alarm));
	}
	if(parsedTaskData->repeat != NULL)
	{
		if((task->repeat = calloc(1, sizeof(Repeat))) == NULL)
		{
			log_write(LOG_ERR, "Failed to allocate memory for `repeat' "
					  "structure: %s", strerror(errno));
			_task_free(parsedTaskData);
			__task_clear_ptod(task);
			return -1;
		}
		memcpy(task->repeat, parsedTaskData->repeat, sizeof(Repeat));
	}
	task->_record_tasks = record;

	_task_free(parsedTaskData);
	return 0;
}

/* Bits, encoding types of task. Can be mixed with OR. */
#define HEADER_PRESENT   0x08
#define NOTE_PRESENT     0x04
#define DUE_DATE_PRESENT 0x80
#define ALARM_PRESENT    0x20
#define REPEAT_PRESENT   0x10

/* Bits, encoding repeat types of task. */
#define REPEAT_N_DAYS           0x0100
#define REPEAT_N_WEEKS          0x0200
#define REPEAT_N_MONTHS_BY_DAY  0x0300
#define REPEAT_N_MONTHS_BY_DATE 0x0400
#define REPEAT_N_YEARS          0x0500

/**
   Parse task data from TasksDB-PTod database.

   @param[in] fd File descriptor of opened TasksDB-PTod database.
   @param[in] type Type of Task from TasksDB-PTod database.
   @return Temporary Task only with parsed data.
*/
static Task * __parse_taskdb_data(int fd, uint8_t type)
{
	Task * result;
	if((result = calloc(1, sizeof(Task))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for task data parsed from "
				  "TasksDB-PTod: %s", strerror(errno));
		return NULL;
	}

	if(type & DUE_DATE_PRESENT)
	{
		uint16_t dueDate;
		if(read(fd, &dueDate, 2) != 2)
		{
			log_write(LOG_ERR, "Failed to read due date from TasksDB-PTod. "
					  "Record type: 0x%02x", type);
			_task_free(result);
			return NULL;
		}
		dueDate = be16toh(dueDate);
		result->dueDay = dueDate & 0x001f; /* First 5 bits */
		result->dueMonth = (dueDate & 0x01e0) >> 5; /* 4 bits starting from
													   5th bit */
		result->dueYear = ((dueDate & 0xfe00) >> 9) + 1904; /* Last 7 bits +
															   start year of Mac
															   OS X HFS epoch */
		log_write(LOG_DEBUG, "Due date: 0x%04x. Parsed: %04d-%02d-%02d",
				  dueDate, result->dueYear, result->dueMonth, result->dueDay);
	}
	if(type & ALARM_PRESENT)
	{
		if((result->alarm = calloc(1, sizeof(Alarm))) == NULL)
		{
			log_write(LOG_ERR, "Cannot allocate memory for task alarm data "
					  "parsed from TasksDB-PTod: %s", strerror(errno));
			_task_free(result);
			return NULL;
		}
		uint16_t alarmTime;
	    if(read(fd, &alarmTime, 2) != 2)
		{
			log_write(LOG_ERR, "Cannot read alarm time from task: %s",
					  strerror(errno));
			_task_free(result);
			return NULL;
		}
		alarmTime = be16toh(alarmTime);
		uint16_t alarmDaysEarlier;
		if(read(fd, &alarmDaysEarlier, 2) != 2)
		{
			log_write(LOG_ERR, "Cannot read alarm days earlier from task: %s",
					  strerror(errno));
			_task_free(result);
			return NULL;
		}
		alarmDaysEarlier = be16toh(alarmDaysEarlier);
		result->alarm->alarmHour = (uint8_t)(alarmTime >> 8);
		result->alarm->alarmMinute = (uint8_t)alarmTime;
		result->alarm->daysEarlier = alarmDaysEarlier;
		log_write(LOG_DEBUG, "Alarm: (0x%04x 0x%04x). Alarm time: %02d:%02d "
				  "days earlier: %d", alarmTime, alarmDaysEarlier,
				  result->alarm->alarmHour, result->alarm->alarmMinute,
				  result->alarm->daysEarlier);
	}
	if(type & REPEAT_PRESENT)
	{
		if((result->repeat = calloc(1, sizeof(Repeat))) == NULL)
		{
			log_write(LOG_ERR, "Cannot allocate memory for task repeat data "
					  "parsed from TasksDB-PTod: %s", strerror(errno));
			_task_free(result);
			return NULL;
		}
		/* Skip repeat of due date */
		if(lseek(fd, 2, SEEK_CUR) == (off_t)-1)
		{
			log_write(LOG_ERR, "Cannot skip duplicate of due date in repeat "
					  "section of task: %s", strerror(errno));
			_task_free(result);
			return NULL;
		}
		uint16_t repeatType;
		if(read(fd, &repeatType, 2) != 2)
		{
			log_write(LOG_ERR, "Cannot read repeat type from task: %s",
					  strerror(errno));
			_task_free(result);
			return NULL;
		}
		repeatType = be16toh(repeatType);
		uint16_t repeatUntilDate;
		if(read(fd, &repeatUntilDate, 2) != 2)
		{
			log_write(LOG_ERR, "Cannot read repeat until date data from task: "
					  "%s", strerror(errno));
			_task_free(result);
			return NULL;
		}
		repeatUntilDate = be16toh(repeatUntilDate);
		uint8_t repeatInterval;
		if(read(fd, &repeatInterval, 1) != 1)
		{
			log_write(LOG_ERR, "Cannot read repeat interval from task: %s",
					  strerror(errno));
			_task_free(result);
			return NULL;
		}
		uint32_t unknownBits;
		if(read(fd, &unknownBits, 3) != 3)
		{
			log_write(LOG_ERR, "Cannot read last three unknown bytes from "
					  "repeat interval: %s", strerror(errno));
			_task_free(result);
			return NULL;
		}
		unknownBits = be32toh(unknownBits & 0x00ffffff);

		switch(repeatType)
		{
		case REPEAT_N_DAYS:
			result->repeat->range = N_DAYS;
			break;
		case REPEAT_N_WEEKS:
			result->repeat->range = N_WEEKS;
			break;
		case REPEAT_N_MONTHS_BY_DAY:
			result->repeat->range = N_MONTHS_BY_DAY;
			break;
		case REPEAT_N_MONTHS_BY_DATE:
			result->repeat->range = N_MONTHS_BY_DATE;
			break;
		case REPEAT_N_YEARS:
			result->repeat->range = N_YEARS;
			break;
		default:
			log_write(LOG_ERR, "Got unknown repeater range: 0x%04x", repeatType);
			_task_free(result);
			return NULL;
		}
		if(repeatUntilDate != 0xffff)
		{
			result->repeat->day = repeatUntilDate & 0x001f;
			result->repeat->month = (repeatUntilDate & 0x01e0) >> 5;
			result->repeat->year = ((repeatUntilDate & 0xfe00) >> 9) + 1904;
		}
		else
		{
			result->repeat->day = 0;
			result->repeat->month = 0;
			result->repeat->year = 0;
		}
		result->repeat->interval = repeatInterval;
		log_write(LOG_DEBUG, "Repeat: (0x%04x 0x%04x 0x%02x 0x%06x). Repeat "
				  "range: %d, until: %04d-%02d-%02d, interval: %d", repeatType,
				  repeatUntilDate, repeatInterval, unknownBits,
				  result->repeat->range, result->repeat->year,
				  result->repeat->month, result->repeat->day,
				  result->repeat->interval);
	}
	if(type & HEADER_PRESENT)
	{
		char buffer[CHUNK_SIZE] = "\0";
		ssize_t readedBytes = 0;

		off_t currPos = lseek(fd, 0, SEEK_CUR);
		log_write(LOG_DEBUG, "Reading header. Current offset: 0x%08x", currPos);

		/* Calculate header size */
		unsigned int headerSize = 0;
		while((readedBytes = read(fd, buffer, CHUNK_SIZE)) > 0)
		{
			const char * headerEnd = memchr(buffer, '\0', readedBytes);
			if(headerEnd != NULL)
			{
				headerSize += headerEnd - buffer;
				break;
			}
			headerSize += readedBytes;
		}
		if(readedBytes < 0)
		{
			log_write(LOG_ERR, "Failed to locate task header: %s", strerror(errno));
			_task_free(result);
			return NULL;
		}
		/* Rewinding to the start of header */
		if(lseek(fd, currPos, SEEK_SET) != currPos)
		{
			log_write(LOG_ERR, "Cannot rewind to start of header: %s",
					  strerror(errno));
			_task_free(result);
			return NULL;
		}
		/* Allocate memory for header */
		if((result->header = calloc(headerSize + 1, sizeof(char))) == NULL)
		{
			log_write(LOG_ERR, "Cannot allocate memory for task header from "
					  "TasksDB-PTod: %s", strerror(errno));
			_task_free(result);
			return NULL;
		}
		/* Reading header */
		log_write(LOG_DEBUG, "Header size: %d", headerSize);
		if(read_chunks(fd, result->header, headerSize))
		{
			log_write(LOG_ERR, "Cannot read task header from TasksDB-PTod");
			_task_free(result);
			return NULL;
		}
		log_write(LOG_DEBUG, "Header: %s", result->header);
	}
	else
	{
		log_write(LOG_ERR, "No header for task. Task type: 0x%02x", type);
		_task_free(result);
		return NULL;
	}

	return result;
}

/**
   Free memory allocated to given Task.

   Given pointer will become unusable after call of this function.

   @param[in] task Pointer to Task structure.
*/
static void _task_free(Task * task)
{
	if(task == NULL)
	{
		log_write(LOG_WARNING, "Got NULL task - nothing to free");
		return;
	}

	free(task->header);
	if(task->text != NULL)
	{
		free(task->text);
	}
	free(task->category);
	__task_clear_ptod(task);

	task->_record_todo = NULL;
	task->_record_tasks = NULL;
	free(task);
}

/**
   Free task fields from TasksDB-PTod PDB file.

   @param[in] task Pointer to task.
*/
static void __task_clear_ptod(Task * task)
{
	if(task == NULL)
	{
		log_write(LOG_WARNING, "Nowhere to clear data from TasksDB-PTod - got "
				  "empty task");
		return;
	}
	if(task->dueDay != 0)
	{
		task->dueDay = 0;
	}
	if(task->dueMonth != 0)
	{
		task->dueMonth = 0;
	}
	if(task->dueYear != 0)
	{
		task->dueYear = 0;
	}
	if(task->alarm != NULL)
	{
		free(task->alarm);
	}
	if(task->repeat != NULL)
	{
		free(task->repeat);
	}
}

/**
   Write task to ToDoDB and TasksDB-PTod PDB files.

   @param[in] tfd Structure with file descriptors for ToDoDB and TasksDB-PTod
   files.
   @param[in] task Structure with task data to write.
   @return Zero on success or non-zero value if error occurs.
*/
static int _tasks_write_task(TasksFD tfd, Task * task)
{
	if(task == NULL)
	{
		log_write(LOG_ERR, "Got NULL task to write!");
		return -1;
	}

	/* Writing to ToDoDB PDB file: */

	PDBRecord * record = task->_record_todo;
	uint32_t offset = record->offset;
	log_write(LOG_DEBUG, "Starting to write todo item to ToDoDB, "
		 	  "address 0x%08x", offset);
	if(lseek(tfd.todo_fd, offset, SEEK_SET) != offset)
	{
		log_write(LOG_ERR, "Failed to go to 0x%08x position in ToDoDB PDB file",
				  offset);
		return -1;
	}

	/* Writing encoded scheduled date */
	uint16_t dueDate = 0xffff;
	if(task->dueDay && task->dueMonth && task->dueYear)
	{
		dueDate = (((task->dueYear - 1904) << 9) & 0xfe00) |
			((((uint16_t) task->dueMonth) << 5) & 0x01e0) |
			(((uint16_t) task->dueDay) & 0x001f);
		dueDate = htobe16(dueDate);
	}
	if(write(tfd.todo_fd, &dueDate, 2) != 2)
	{
		log_write(LOG_ERR, "Failed to write due date (0x%04x) to ToDoDB PDB "
				  "file", dueDate);
		return -1;
	}
	log_write(LOG_DEBUG, "Wrote due date 0x%04x to ToDoDB PDB file", dueDate);

	/* Writing priority */
	uint8_t priority;
	switch(task->priority)
	{
	case PRIORITY_1:
		priority = 1;
		break;
	case PRIORITY_2:
		priority = 2;
		break;
	case PRIORITY_3:
		priority = 3;
		break;
	case PRIORITY_4:
		priority = 4;
		break;
	case PRIORITY_5:
		priority = 5;
		break;
	default:
		log_write(LOG_ERR, "Cannot write unknown priority (%d) to ToDoDB PDB "
				  "file", task->priority);
		return -1;
	}
	if(write(tfd.todo_fd, &priority, 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write priority (%d) to ToDoDB PDB "
				  "file", priority);
		return -1;
	}
	log_write(LOG_DEBUG, "Wrote priority 0x%02x to ToDoDB PDB file", priority);

	/* Writing header */
	if(write_chunks(tfd.todo_fd, task->header, strlen(task->header)))
	{
		log_write(LOG_ERR, "Failed to write task header to ToDoDB PDB file!");
		return -1;
	}
	log_write(LOG_DEBUG, "Write header (len=%d) [%s] to ToDoDB PDB file",
			  strlen(task->header), iconv_cp1251_to_utf8(task->header));

	/* Insert '\0' as divider */
	if(write(tfd.todo_fd, "\0", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\0\" as divider after header");
		return -1;
	}

	/* Writing note */
	if(task->text != NULL)
	{
		if(write_chunks(tfd.todo_fd, task->text, strlen(task->text)))
		{
			log_write(LOG_ERR, "Failed to write task text!");
			return -1;
		}
		log_write(LOG_DEBUG, "Write text (len=%d) for task", strlen(task->text));
	}

	/* Writing '\0' at the end of record in ToDoDB PDB file */
	if(write(tfd.todo_fd, "\0", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\0\" at the end of ToDoDB PDB "
				  "file");
		return -1;
	}

	/* Writing to TasksDB-PTod file */

	record = task->_record_tasks;
	offset = record->offset;
	log_write(LOG_DEBUG, "Starting to write todo item to TasksDB-PTod file, "
		 	  "address 0x%08x", offset);
	if(lseek(tfd.tasks_fd, offset, SEEK_SET) != offset)
	{
		log_write(LOG_ERR, "Failed to go to 0x%08x position in TasksDB-PTod "
				  "PDB file", offset);
		return -1;
	}

	/* Writing entry type */
	uint8_t type = HEADER_PRESENT;
	if(task->text != NULL)
	{
		type |= NOTE_PRESENT;
	}
	if(task->dueDay && task->dueMonth && task->dueYear)
	{
		type |= DUE_DATE_PRESENT;
	}
	if(task->alarm != NULL)
	{
		type |= ALARM_PRESENT;
	}
	if(task->repeat != NULL)
	{
		type |= REPEAT_PRESENT;
	}
	if(write(tfd.tasks_fd, &type, 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write task type (0x%02d) to TasksDB-PTod "
				  "file", type);
		return -1;
	}
	log_write(LOG_DEBUG, "Wrote task type 0x%02x to TasksDB-PTod filel", type);

	/* Writing 4 zero bytes */
	uint8_t zeroBytes[4] = {0x00, 0x00, 0x00, 0x00};
	if(write(tfd.tasks_fd, &zeroBytes, 4) != 4)
	{
		log_write(LOG_ERR, "Failed to write 4 zero bytes between task type and "
				  "task priority in TasksDB-PTod file");
		return -1;
	}

	/* Writing priority */
	if(write(tfd.tasks_fd, &priority, 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write priority (%d) to TasksDB-PTod PDB "
				  "file", priority);
		return -1;
	}
	log_write(LOG_DEBUG, "Wrote task priority 0x%02x to TasksDB-PTod file",
		priority);

	/* Writing scheduled date */
	if(task->dueDay && task->dueMonth && task->dueYear &&
	   (write(tfd.tasks_fd, &dueDate, 2) != 2))
	{
		log_write(LOG_ERR, "Failed to write due date (0x%04d) to TasksDB-PTod "
				  "PDB file", dueDate);
		return -1;
	}
	log_write(LOG_DEBUG, "Wrote due date 0x%04x to TasksDB-PTod file",
			  dueDate);

	/* Writing alarm data */
	if(task->alarm != NULL)
	{
		/* Writing time */
		uint16_t alarmTime =
			((((uint16_t)task->alarm->alarmHour) << 8) & 0xff00) |
			(uint16_t)task->alarm->alarmMinute;
		alarmTime = htobe16(alarmTime);
		if(write(tfd.tasks_fd, &alarmTime, 2) != 2)
		{
			log_write(LOG_ERR, "Failed to write alarm time (0x%04x) to TasksDB-"
					  "PTod PDB file", alarmTime);
			return -1;
		}
		log_write(LOG_DEBUG, "Wrote alarm time 0x%04x to TasksDB-PTod file",
				  alarmTime);
		/* Writing days before due date */
		uint16_t daysEarlier = htobe16(task->alarm->daysEarlier);
		if(write(tfd.tasks_fd, &daysEarlier, 2) != 2)
		{
			log_write(LOG_ERR, "Failed to write alarm days earlier (0x%04x)"
					  " to TasksDB-PTod PDB file", task->alarm->daysEarlier);
			return -1;
		}
		log_write(LOG_DEBUG, "Wrote days earlier 0x%04x to TasksDB-PTod file",
				  daysEarlier);
	}

	/* Writing repeat data */
	if(task->repeat != NULL)
	{
		/* Writing scheduled date again */
		if(task->dueDay && task->dueMonth && task->dueYear &&
		   (write(tfd.tasks_fd, &dueDate, 2) != 2))
		{
			log_write(LOG_ERR, "Failed to write due date (0x%04d) to TasksDB-PTod "
					  "PDB file", dueDate);
			return -1;
		}
		/* Writing repeat type */
		uint16_t repeatType;
		switch(task->repeat->range)
		{
		case N_DAYS:
			repeatType = REPEAT_N_DAYS;
			break;
		case N_WEEKS:
			repeatType = REPEAT_N_WEEKS;
			break;
		case N_MONTHS_BY_DAY:
			repeatType = REPEAT_N_MONTHS_BY_DAY;
			break;
		case N_MONTHS_BY_DATE:
			repeatType = REPEAT_N_MONTHS_BY_DATE;
			break;
		case N_YEARS:
			repeatType = REPEAT_N_YEARS;
			break;
		default:
			log_write(LOG_ERR, "Got unknown repeat type: 0x%04x",
					  task->repeat->range);
			return -1;
		}
		repeatType = htobe16(repeatType);
		if(write(tfd.tasks_fd, &repeatType, 2) != 2)
		{
			log_write(LOG_ERR, "Failed to write repeat type (0x%04x) to TasksDB"
					  "-PTod file", repeatType);
			return -1;
		}
		log_write(LOG_DEBUG, "Wrote repeat type 0x%04x to TasksDB-PTod file",
				  repeatType);
		/* Write repeat unitl date */
		uint16_t repeatUntilDate = 0xffff;
		if(task->repeat->day && task->repeat->month && task->repeat->year)
		{
			repeatUntilDate = (((task->repeat->year - 1904) << 9) & 0xfe00) |
				((((uint16_t) task->repeat->month) << 5) & 0x01e0) |
				(((uint16_t) task->repeat->day) & 0x001f);
			repeatUntilDate = htobe16(repeatUntilDate);
		}
		if(write(tfd.tasks_fd, &repeatUntilDate, 2) != 2)
		{
			log_write(LOG_ERR, "Failed to write repeat until date (0x%04x) to"
					  " TasksDB-PTod PDB file", repeatUntilDate);
			return -1;
		}
		log_write(LOG_DEBUG, "Wrote repeat until date 0x%04x to TasksDB-PTod "
				  "file", repeatUntilDate);
		/* Write repeat N times value */
		if(write(tfd.tasks_fd, &task->repeat->interval, 1) != 1)
		{
			log_write(LOG_ERR, "Failed to write repeat until date (0x%02x) "
					  "value to TasksDB-PTod PDB file", task->repeat->interval);
			return -1;
		}
		log_write(LOG_DEBUG, "Wrote repeat interval 0x%02x to TasksDB-PTod",
				  task->repeat->interval);
		/* Write 3 unknown bytes */
		uint8_t unknown[3] = {0x00, 0x00, 0x00};
		if(write(tfd.tasks_fd, &unknown, 3) != 3)
		{
			log_write(LOG_ERR, "Failed to write 3 unknown bytes at the end of "
					  "repeat data to TasksDB-PTod file");
			return -1;
		}
	}

	/* Writing header */
	if(write_chunks(tfd.tasks_fd, task->header, strlen(task->header)))
	{
		log_write(LOG_ERR, "Failed to write task header to TasksDB-PTod PDB "
				  "file!");
		return -1;
	}
	log_write(LOG_DEBUG, "Write header (len=%d) [%s] to TasksDB-PTod PDB file",
			  strlen(task->header), iconv_cp1251_to_utf8(task->header));

	/* Insert '\0' as divider */
	if(write(tfd.tasks_fd, "\0", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\0\" as divider after header");
		return -1;
	}

	/* Writing note */
	if(task->text != NULL)
	{
		if(write_chunks(tfd.tasks_fd, task->text, strlen(task->text)))
		{
			log_write(LOG_ERR, "Failed to write task text!");
			return -1;
		}
		log_write(LOG_DEBUG, "Write text (len=%d) for task", strlen(task->text));
	}

	/* Writing '\0' at the end of record in TasksDB-PTod PDB file */
	if(write(tfd.tasks_fd, "\0", 1) != 1)
	{
		log_write(LOG_ERR, "Failed to write \"\\0\" at the end of TasksDB-PTod "
				  "PDB file");
		return -1;
	}

	return 0;
}
