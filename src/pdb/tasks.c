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
		free(task1->header);
		if(task1->text != NULL)
		{
			free(task1->text);
		}
		free(task1->category);
		__task_clear_ptod(task1);
		PDBRecord * recordToDoDB = task1->_record_todo;
		PDBRecord * recordTasksDB = task1->_record_tasks;
		task1->_record_todo = NULL;
		task1->_record_tasks = NULL;
		free(task1);
		recordToDoDB->data = NULL;
		recordTasksDB->data = NULL;
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

Task * tasks_task_add(Tasks * tasks, char * header, char * text,
					  char * category, TaskPriority priority)
{
	if(tasks == NULL)
	{
		log_write(LOG_ERR, "No tasks structure - nowhere to add new task!");
		return NULL;
	}
	if(header == NULL)
	{
		log_write(LOG_ERR, "Header of new task is NULL! Cannot add new task!");
		return NULL;
	}

	/* Search category ID for given category */
	if(category == NULL)
	{
		category = PDB_DEFAULT_CATEGORY;
	}
	int categoryIdToDoDB = pdb_category_get_id(tasks->_pdb_tododb, category);
	if(categoryIdToDoDB == UINT8_MAX)
	{
		log_write(LOG_DEBUG, "Category with name \"%s\" not found in ToDoDB "
				  "PDB!", category);
		if((categoryIdToDoDB = pdb_category_add(tasks->_pdb_tododb,
												category)) == UINT8_MAX)
		{
			log_write(LOG_ERR, "Cannot add new category with name \"%s\" "
					  "to ToDoDB PDB!", category);
			return NULL;
		}
	}
	int categoryIdTasksDB = pdb_category_get_id(tasks->_pdb_tasks, category);
	if(categoryIdTasksDB == UINT8_MAX)
	{
		log_write(LOG_DEBUG, "Category with name \"%s\" not found in TasksDB "
				  "PDB!", category);
		if((categoryIdTasksDB = pdb_category_add(tasks->_pdb_tasks,
												 category)) == UINT8_MAX)
		{
			log_write(LOG_ERR, "Cannot add new category with name \"%s\" "
					  "to TasksDB PDB!", category);
			return NULL;
		}
	}
	if(categoryIdToDoDB != categoryIdTasksDB)
	{
		log_write(LOG_ERR, "Successfully added \"%s\" category to ToDoDB and "
			"TasksDB PDBs, but category IDs in these files are differ: %d and "
				  "%d", category, categoryIdToDoDB, categoryIdTasksDB);
		return NULL;
	}

	/* Calculating offsets for new task: */

	/* Get last task in ToDoDB */
	PDBRecord * recordToDoDB;
	PDBRecord * recordTasksDB;
	Task * task;
	if((recordToDoDB = TAILQ_LAST(&tasks->_pdb_tododb->records,
								  RecordQueue)) == NULL)
	{
		log_write(LOG_ERR, "Cannot get last task's record from ToDoDB PDB "
				  "header");
		return NULL;
	}
	if((recordTasksDB = TAILQ_LAST(&tasks->_pdb_tasks->records,
								   RecordQueue)) == NULL)
	{
		log_write(LOG_ERR, "Cannot get last task's record from TasksDB PDB "
				  "header");
		return NULL;
	}
	if((task = TAILQ_LAST(&tasks->queue, TasksQueue)) == NULL)
	{
		log_write(LOG_ERR, "Cannot get last task from ToDoDB PDB header");
		return NULL;
	}
	if(task->_record_todo != recordToDoDB || task->_record_tasks != recordTasksDB)
	{
		log_write(LOG_ERR, "Latest task and latest PDB record doesn't match!\n"
				  "[ToDoDB] Latest task record: 0x%08x, "
				  "latest PDB record: 0x%08x\n",
				  "[TasksDB] Latest task record: 0x%08x, "
				  "latest PDB record: 0x%08x",
				  task->_record_todo, recordToDoDB,
				  task->_record_tasks, recordTasksDB);
		return NULL;
	}
	uint32_t offsetToDoDB = recordToDoDB->offset;
	uint32_t offsetTasksDB = recordTasksDB->offset;
	log_write(LOG_DEBUG, "Offset of the last record in ToDoDB: 0x%08x\n"
			  "Offset of the last record in TasksDB: 0x%08x",
			  offsetToDoDB, offsetTasksDB);

	/* Calculate size of last record and offset beyond it for ToDoDB: */
	/* Scheduled date (2 bytes) */
	offsetToDoDB += 2;
	/* Priority (1 byte) */
	offsetToDoDB += 1;
	/* Header + '\0' */
	offsetToDoDB += strlen(task->header) + sizeof(char);
	/* Text (if exists) + '\0' */
	offsetToDoDB += (task->text != NULL ? strlen(task->text) : 0) + sizeof(char);
	/* Size of new task item in record list */
	offsetToDoDB += PDB_RECORD_ITEM_SIZE;

	/* Calculate size of last record and offset beyond it for TasksDB: */
	/* Entry type (1 byte) */
	offsetTasksDB += 1;
	/* Zero bytes (4 bytes) */
	offsetTasksDB += 4;
	/* Priority (1 byte) */
	offsetTasksDB += 1;
	/* Sheduled date (2 bytes) */
	if(task->dueDay != 0 && task->dueMonth != 0 && task->dueYear != 0)
	{
		offsetTasksDB += 2;
	}
	/* Alarm data (4 bytes) */
	if(task->alarm != NULL)
	{
		offsetTasksDB += 4;
	}
	/* Repeat interval (7 bytes) */
	if(task->repeat != NULL)
	{
		offsetTasksDB += 10;
	}
	/* Header + '\0' */
	offsetTasksDB += strlen(task->header) + sizeof(char);
	/* Text (if exists) + '\0' */
	offsetTasksDB += (task->text != NULL ? strlen(task->text) : 0) + sizeof(char);
	/* Size of new task item in record list */
	offsetTasksDB += PDB_RECORD_ITEM_SIZE;

	/* Allocate memory for new task */
	if((task = calloc(1, sizeof(Task))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new task: %s",
				  strerror(errno));
		return NULL;
	}
	/* '+ 1' for NULL-terminating character */
	if((task->header = calloc(strlen(header) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new task header: %s",
				  strerror(errno));
		free(task);
		return NULL;
	}
	if(text != NULL &&
	   (task->text = calloc(strlen(text) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new task text: %s",
				  strerror(errno));
	    free(task->header);
		free(task);
		return NULL;
	}
	if((task->category = calloc(strlen(category) + 1, sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Cannot allocate memory for new task category: %s",
				  strerror(errno));
		if(text != NULL)
		{
			free(task->text);
		}
		free(task->header);
		free(task);
		return NULL;
	}

	/* Add new records for new task */
	if((recordToDoDB = pdb_record_create(
			tasks->_pdb_tododb, offsetToDoDB,
			PDB_RECORD_ATTR_EMPTY | (0x0f & categoryIdToDoDB), task)) == NULL)
	{
		log_write(LOG_ERR, "Cannot add new record for new task in ToDoDB PDB");
		free(task->category);
		if(text != NULL)
		{
			free(task->text);
		}
		free(task->header);
		free(task);
		return NULL;
	}
	uint8_t id[3] = {
		recordToDoDB->id[0],
		recordToDoDB->id[1],
		recordToDoDB->id[2]
	};
	if((recordTasksDB = pdb_record_create_with_id(
			tasks->_pdb_tasks, offsetTasksDB,
			PDB_RECORD_ATTR_EMPTY | (0x0f & categoryIdTasksDB),
			id, task)) == NULL)
	{
		log_write(LOG_ERR, "Cannot add new record for new task in TasksDB PDB");
		free(task->category);
		if(text != NULL)
		{
			free(task->text);
		}
		free(task->header);
		free(task);
		return NULL;
	}

	/* Fill new task with data and append it to PDB structure */
	strcpy(task->header, header);
	task->text = text != NULL ? strcpy(task->text, text) : NULL;
	strcpy(task->category, category);
	task->priority = priority;
	task->dueDay = 0;
	task->dueMonth = 0;
	task->dueYear = 0;
	task->alarm = NULL;
	task->repeat = NULL;
	task->_record_todo = recordToDoDB;
	task->_record_tasks = recordTasksDB;
	if(TAILQ_EMPTY(&tasks->queue))
	{
		TAILQ_INSERT_HEAD(&tasks->queue, task, pointers);
	}
	else
	{
		TAILQ_INSERT_TAIL(&tasks->queue, task, pointers);
	}

	/* Recalculate and update offsets for old tasks */
	log_write(LOG_DEBUG, "Changing offsets for old tasks in ToDoDB PDB");
	PDBRecord * oldRecord = TAILQ_PREV(recordToDoDB, RecordQueue, pointers);
	while(oldRecord != NULL)
	{
		log_write(LOG_DEBUG, "For existing record: old offset=0x%08x, "
				  "new offset=0x%08x", oldRecord->offset,
				  oldRecord->offset + PDB_RECORD_ITEM_SIZE);
		oldRecord->offset += PDB_RECORD_ITEM_SIZE;
		oldRecord = TAILQ_PREV(oldRecord, RecordQueue, pointers);
	}

	log_write(LOG_DEBUG, "Changing offsets for old tasks in TasksDB PDB");
	oldRecord = TAILQ_PREV(recordTasksDB, RecordQueue, pointers);
	while(oldRecord != NULL)
	{
		log_write(LOG_DEBUG, "For existing record: old offset=0x%08x, "
				  "new offset=0x%08x", oldRecord->offset,
				  oldRecord->offset + PDB_RECORD_ITEM_SIZE);
		oldRecord->offset += PDB_RECORD_ITEM_SIZE;
		oldRecord = TAILQ_PREV(oldRecord, RecordQueue, pointers);
	}

	return task;
}

int tasks_task_set_due(Task * task, uint16_t dueYear, uint8_t dueMonth,
					   uint8_t dueDay)
{
	int32_t offsetTasksDBDelta = 0;
	if(task == NULL)
	{
		log_write(LOG_ERR, "Got NULL pointer to task, can't set due date");
		return -1;
	}

	/* Update task with given due date.
	   Or remove due date at all */
	if(dueYear == 0 || dueMonth == 0 || dueDay == 0)
	{
		if(task->dueDay == 0 && task->dueMonth == 0 && task->dueYear == 0)
		{
			log_write(LOG_DEBUG, "Due date for task already empty - no need to "
					  "clear it!");
			return 0;
		}
		log_write(LOG_DEBUG, "Clearing due date for task. Old due date:: year: "
				  "%d, month: %d, day: %d", task->dueYear, task->dueMonth,
				  task->dueDay);
		/* If repeat interval is exists and due date was removed - we should
		   change offset delta to -2, because scheduled date repeated before
		   repeat date. */
		if(task->repeat != NULL)
		{
			offsetTasksDBDelta = -2;
		}
		task->dueDay = 0;
		task->dueMonth = 0;
		task->dueYear = 0;
	}
	else
	{
		if(task->dueDay == 0 && task->dueMonth == 0 && task->dueYear == 0)
		{
			log_write(LOG_DEBUG, "No due date in task - setting the new one");
			/* If repeat interval is set and due date not exists before - we
			   should change offset delta to 2, because scheduled date repeated
			   before repeat date. */
			if(task->repeat != NULL)
			{
				offsetTasksDBDelta = 2;
			}
		}
		else
		{
			log_write(LOG_DEBUG, "Changing existing due date in task.");
		}
		task->dueDay = dueDay;
		task->dueMonth = dueMonth;
		task->dueYear = dueYear;
	}

	/* Change offsets for the next tasks in TasksDB PDB */
	if(offsetTasksDBDelta != 0)
	{
		log_write(LOG_DEBUG, "Changing offsets for next tasks in TasksDB PDB. "
				  "Offset delta = %d", offsetTasksDBDelta);
		PDBRecord * nextRecord = TAILQ_NEXT(task->_record_tasks, pointers);
		while(nextRecord != NULL)
		{
			log_write(LOG_DEBUG, "For existing record: old offset=0x%08x, "
					  "new offset=0x%08x", nextRecord->offset,
					  nextRecord->offset + offsetTasksDBDelta);
			nextRecord->offset += offsetTasksDBDelta;
			nextRecord = TAILQ_NEXT(nextRecord, pointers);
		}
	}

	return 0;
}

int tasks_task_set_alarm(Task * task, Alarm * alarm)
{
	if(task == NULL)
	{
		log_write(LOG_ERR, "Got NULL pointer to task, can't set alarm");
		return -1;
	}
	if(task->dueYear == 0 && task->dueMonth == 0 && task->dueDay)
	{
		log_write(LOG_WARNING, "There is no due date set - can't set alarm!");
		log_write(LOG_WARNING, "Problem task header: %s",
				  iconv_cp1251_to_utf8(task->header));
		return -1;
	}

	int32_t offsetDelta = 0;
	/* Setting new alarm for task */
	if(task->alarm == NULL && alarm == NULL)
	{
		log_write(LOG_DEBUG, "Alarm already is not set in task, nothing to do");
		return 0;
	}
	else if(alarm == NULL)
	{
		log_write(LOG_DEBUG, "Clearing task's alarm");
		free(task->alarm);
		task->alarm = NULL;
		offsetDelta = -4;
	}
	else if(task->alarm == NULL)
	{
		log_write(LOG_DEBUG, "Setting new alarm for task");
		if((task->alarm = calloc(1, sizeof(Alarm))) == NULL)
		{
			log_write(LOG_ERR, "Failed to allocate memory for new alarm in "
					  "tasks_task_set_alarm(): %s", strerror(errno));
			return -1;
		}
		memcpy(task->alarm, alarm, sizeof(Alarm));
		offsetDelta = 4;
	}
	else
	{
		log_write(LOG_DEBUG, "Updating existing alarm for task");
		task->alarm->alarmHour = alarm->alarmHour;
		task->alarm->alarmMinute = alarm->alarmMinute;
		task->alarm->daysEarlier = alarm->daysEarlier;
	}

	/* Updating offsets for next tasks in queue */
	if(offsetDelta != 0)
	{
		log_write(LOG_DEBUG, "Changing offsets for next tasks in TasksDB PDB. "
				  "Offset delta = %d", offsetDelta);
		PDBRecord * nextRecord = TAILQ_NEXT(task->_record_tasks, pointers);
		while(nextRecord != NULL)
		{
			log_write(LOG_DEBUG, "For existing record: old offset=0x%08x, "
					  "new offset=0x%08x", nextRecord->offset,
					  nextRecord->offset + offsetDelta);
			nextRecord->offset += offsetDelta;
			nextRecord = TAILQ_NEXT(nextRecord, pointers);
		}
	}

	return 0;
}

int tasks_task_set_repeat(Task * task, Repeat * repeat)
{
	if(task == NULL)
	{
		log_write(LOG_ERR, "Got NULL pointer to task, can't set repeat interval");
		return -1;
	}

	int32_t offsetDelta = 0;
	/* Setting new repeat inteval for task */
	if(task->repeat == NULL && repeat == NULL)
	{
		log_write(LOG_DEBUG, "Repeat interval is not set in task,  nothing to do");
		return 0;
	}
	else if(repeat == NULL)
	{
		log_write(LOG_DEBUG, "Clearing task's repeat interval");
		free(task->repeat);
		task->repeat = NULL;
		/* Second scheduled - 2 bytes
		   Repeat type - 2 bytes
		   Repeat data - 3 bytes
		   Unknown 3 bytes - 3 bytes */
		offsetDelta = -10;
	}
	else if(task->repeat == NULL)
	{
		log_write(LOG_DEBUG, "Setting new repeat interval for task");
		if((task->repeat = calloc(1, sizeof(Repeat))) == NULL)
		{
			log_write(LOG_ERR, "Failed to allocate memory for new repeat "
					  "interval in tasks_task_set_repeat(): %s",
					  strerror(errno));
			return -1;
		}
		memcpy(task->repeat, repeat, sizeof(Repeat));
		offsetDelta = 10;
	}
	else
	{
		log_write(LOG_DEBUG, "Updating existing interval for task");
		log_write(LOG_DEBUG, "Range: %d", repeat->range);
		log_write(LOG_DEBUG, "Day: %d", repeat->day);
		log_write(LOG_DEBUG, "Month: %d", repeat->month);
		log_write(LOG_DEBUG, "Year: %d", repeat->year);
		log_write(LOG_DEBUG, "Interval: %d", repeat->interval);
		task->repeat->range = repeat->range;
		task->repeat->day = repeat->day;
		task->repeat->month = repeat->month;
		task->repeat->year = repeat->year;
		task->repeat->interval = repeat->interval;
		log_write(LOG_DEBUG, "Update of existing interval complete");
	}

	/* Updating offsets for next tasks in queue */
	if(offsetDelta != 0)
	{
		log_write(LOG_DEBUG, "Changing offsets for next tasks in TasksDB PDB. "
				  "Offset delta = %d", offsetDelta);
		PDBRecord * nextRecord = TAILQ_NEXT(task->_record_tasks, pointers);
		while(nextRecord != NULL)
		{
			log_write(LOG_DEBUG, "For existing record: old offset=0x%08x, "
					  "new offset=0x%08x", nextRecord->offset,
					  nextRecord->offset + offsetDelta);
			nextRecord->offset += offsetDelta;
			nextRecord = TAILQ_NEXT(nextRecord, pointers);
		}
	}

	return 0;
}

int tasks_task_edit(Tasks * tasks, Task * task, char * header, char * text,
					char * category, TaskPriority * priority)
{
	if(tasks == NULL)
	{
		log_write(LOG_ERR, "Got NULL tasks queue - cannot edit task");
		return -1;
	}
	if(task == NULL);
	{
		log_write(LOG_ERR, "Got NULL task - cannot edit it");
		return -1;
	}

	PDBRecord * recordToDo = task->_record_todo;
	PDBRecord * recordTasks = task->_record_tasks;

	/* Allocate memory for new header and text, if necessary */
	char * newHeader = NULL;
	char * newText = NULL;
	int strlenTaskText = task->text != NULL ? strlen(task->text) : 0;
	if(header != NULL && strlen(header) > strlen(task->header) &&
	   (newHeader = calloc(strlen(header), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for new task's header: "
				  "%s", strerror(errno));
		return -1;
	}
	if(text != NULL && strlen(text) > strlenTaskText &&
	   (newText = calloc(strlen(text), sizeof(char))) == NULL)
	{
		log_write(LOG_ERR, "Failed to allocate memory for new task's text: %s",
				  strerror(errno));
		if(newHeader != NULL)
		{
			free(newHeader);
		}
		return -1;
	}

	/* Read and check category ID if necessary */
	uint8_t categoryIdToDoDB = 0;
	uint8_t categoryIdTasksDB = 0;
	if(category != NULL &&
	   ((categoryIdToDoDB = pdb_category_get_id(tasks->_pdb_tododb,
												category)) == UINT8_MAX ||
		(categoryIdTasksDB = pdb_category_get_id(tasks->_pdb_tasks,
												 category)) == UINT8_MAX))
	{
		log_write(LOG_ERR, "Cannot find category ID for category \"%s\" in "
				  "%s PDB", category,
				  categoryIdToDoDB == UINT8_MAX ?
				  "ToDoDB" :
				  "TasksDB");
		if(newHeader != NULL)
		{
			free(newHeader);
		}
		if(newText != NULL)
		{
			free(newText);
		}
		return -1;
	}
	if(categoryIdToDoDB != categoryIdTasksDB)
	{
		log_write(LOG_DEBUG, "Found category IDs for \"%s\" category, but "
				  "they are differs:: ToDoDB: %d, TasksDB: %d", category,
				  categoryIdToDoDB, categoryIdTasksDB);
		if(newHeader != NULL)
		{
			free(newHeader);
		}
		if(newText != NULL)
		{
			free(newText);
		}
		return -1;
	}

	/* Writing changes to memory */
	int32_t headerSizeDiff = header != NULL ?
		strlen(header) - strlen(task->header) :
		0;
	int32_t textSizeDiff = text != NULL ?
		strlen(text) - strlen(task->text) :
		0;

	if(newHeader != NULL)
	{
		free(task->header);
		task->header = newHeader;
		strcpy(task->header, header);
	}
	else if(header != NULL)
	{
		explicit_bzero(task->header, strlen(task->header));
		strcpy(task->header, header);
	}

	if(newText != NULL)
	{
		if(task->text != NULL)
		{
			free(task->text);
		}
		task->text = newText;
		strcpy(task->text, text);
	}
	else if(task != NULL)
	{
		if(task->text == NULL)
		{
			if((task->text = calloc(strlen(text), sizeof(char))) == NULL)
			{
				log_write(LOG_ERR, "Failed to allocate memory for new text in "
						  "task structure: %s", strerror(errno));
				if(newHeader != NULL)
				{
					free(newHeader);
				}
				return -1;
			}
		}
		explicit_bzero(task->text, strlen(task->text));
		strcpy(task->text, text);
	}

	if(category != NULL)
	{
		recordToDo->attributes &= 0xf0;
		recordToDo->attributes |= categoryIdToDoDB;
		recordTasks->attributes &= 0xf0;
		recordTasks->attributes |= categoryIdTasksDB;
	}

	/* Should recalculate offset for next tasks */
	log_write(LOG_DEBUG, "Recalculate offsets for next tasks");
	if(headerSizeDiff + textSizeDiff != 0)
	{
		PDBRecord * record;
		while((record = TAILQ_NEXT(recordToDo, pointers)) != NULL)
		{
			log_write(LOG_DEBUG, "[ToDoDB] Next task: old offset=0x%08x, "
					  "new offset=0x%08x", record->offset,
					  record->offset + headerSizeDiff + textSizeDiff);
			record->offset += headerSizeDiff + textSizeDiff;
		}
		while((record = TAILQ_NEXT(recordTasks, pointers)) != NULL)
		{
			log_write(LOG_DEBUG, "[TasksDB] Next task: old offset=0x%08x, "
					  "new offset=0x%08x", record->offset,
					  record->offset + headerSizeDiff + textSizeDiff);
			record->offset += headerSizeDiff + textSizeDiff;
		}
	}

	return 0;
}


int tasks_task_delete(Tasks * tasks, Task * task)
{
	if(tasks == NULL)
	{
		log_write(LOG_ERR, "Got no tasks, cannot delete task");
		return -1;
	}
	if(task == NULL)
	{
		log_write(LOG_ERR, "Got completely empty task to delete. Nothing to "
				  "delete");
		return -1;
	}

	uint32_t offsetToDo = 2; /* Scheduled date */
	offsetToDo += 1; /* Priority */
	offsetToDo += strlen(task->header) + sizeof(char); /* header + '\0' */
	offsetToDo += (task->text != NULL ? strlen(task->text) : 0) + sizeof(char); /* text + '\0' */
	offsetToDo += PDB_RECORD_ITEM_SIZE;

	uint32_t offsetTasks = 1; /* Entry type */
	offsetTasks += 4; /* Zero bytes */
	offsetTasks += 1; /* Priority */
	if(task->dueDay != 0 && task->dueMonth != 0 && task->dueYear != 0)
	{
		offsetTasks += 2;
	}
	if(task->alarm != NULL)
	{
		offsetTasks += 4;
	}
	if(task->repeat != NULL)
	{
		offsetTasks += 2; /* Repeat of scheduled */
		offsetTasks += 2; /* Repeat type */
		offsetTasks += 3; /* Repeat data */
		offsetTasks += 3; /* Unknown 3 bytes at the end */
	}
	offsetTasks += strlen(task->header) + sizeof(char); /* header + '\0' */
	offsetTasks += (task->text != NULL ? strlen(task->text) : 0) + sizeof(char); /*text + '\0' */
	offsetTasks += PDB_RECORD_ITEM_SIZE;

	/* Delete task */
	PDBRecord * recordToDoDB = task->_record_todo;
	PDBRecord * recordTasksDB = task->_record_tasks;
	free(task->header);
	if(task->text != NULL)
	{
		free(task->text);
	}
	free(task->category);
	if(task->alarm != NULL)
	{
		free(task->alarm);
	}
	if(task->repeat != NULL)
	{
		free(task->repeat);
	}
	TAILQ_REMOVE(&tasks->queue, task, pointers);

	/* Recalculate offsets for next tasks */
	PDBRecord * recordToDoDB2 = recordToDoDB;
	PDBRecord * recordTasksDB2 = recordTasksDB;
	log_write(LOG_DEBUG, "Recalculate offsets for next tasks");
	while((recordToDoDB2 = TAILQ_NEXT(recordToDoDB2, pointers)) != NULL)
	{
		log_write(LOG_DEBUG, "[ToDoDB] Existing task: old offset=0x%08x, "
				  "new offset=0x%08x", recordToDoDB2->offset,
				  recordToDoDB2->offset - offsetToDo);
		recordToDoDB2->offset -= offsetToDo;
	}
	while((recordTasksDB2 = TAILQ_NEXT(recordTasksDB2, pointers)) != NULL)
	{
		log_write(LOG_DEBUG, "[TasksDB] Existing task: old offset=0x%08x, "
				  "new offset=0x%08x", recordTasksDB2->offset,
				  recordTasksDB2->offset - offsetTasks);
		recordTasksDB2->offset -= offsetTasks;
	}
	/* Recalculate offsets for previous tasks,
	   due to record list size change. */
	recordToDoDB2 = recordToDoDB;
	recordTasksDB2 = recordTasksDB;
	log_write(LOG_DEBUG, "Recalculate offsets due to record list size change "
			  "for previous change");
	while((recordToDoDB2 = TAILQ_PREV(
			   recordToDoDB2, RecordQueue, pointers)) != NULL)
	{
		log_write(LOG_DEBUG, "[ToDoDB] Existing task [2]: old offset=0x%08x, "
				  "new offset=0x%08x", recordToDoDB2->offset,
				  recordToDoDB2->offset - PDB_RECORD_ITEM_SIZE);
		recordToDoDB2->offset -= PDB_RECORD_ITEM_SIZE;
	}
	while((recordTasksDB2 = TAILQ_PREV(
			   recordTasksDB2, RecordQueue, pointers)) != NULL)
	{
		log_write(LOG_DEBUG, "[TasksDB] Existing task [2]: old offset=0x%08x, "
				  "new offset=0x%08x", recordTasksDB2->offset,
				  recordTasksDB2->offset - PDB_RECORD_ITEM_SIZE);
		recordTasksDB2->offset -= PDB_RECORD_ITEM_SIZE;
	}

	/* Delete records in ToDoDB and TasksDB-PTod files */
	long uniqueRecordId = pdb_record_get_unique_id(recordToDoDB);
	if(pdb_record_delete(tasks->_pdb_tododb, uniqueRecordId))
	{
		log_write(LOG_ERR, "[ToDoDB] Cannot delete task record from record list"
				  " (offset: 0x%08x)", recordToDoDB->offset);
		return -1;
	}
	uniqueRecordId = pdb_record_get_unique_id(recordTasksDB);
	if(pdb_record_delete(tasks->_pdb_tasks, uniqueRecordId))
	{
		log_write(LOG_ERR, "[TasksDB] Cannot delete task record from record "
				  "list (offset: 0x%08x)", recordTasksDB->offset);
		return -1;
	}

	return 0;
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
