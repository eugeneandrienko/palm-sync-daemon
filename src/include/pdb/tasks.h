/**
   @author Eugene Andrienko
   @brief Module to operate with ToDoDB and TasksDB-PTod databases.
   @file tasks.h

   This module can be used to open PDB file from Tasks application and
   read it's structure to Tasks structure — with tasks_open() and
   tasks_read() functions.

   To write changed Tasks back to file — use tasks_write(). To close
   opened file use tasks_close(). To free memory, allocated inside
   Tasks structure, used tasks_free().

   To work with Tasks structure use tasks_task_get(),
   tasks_task_add(), tasks_task_edit() and tasks_task_delete()
   functions.
*/

/**
   @page tasks Operate with Tasks application data from Palm PDA

   This module can operate with tasks from Palm PDA via ToDoDB and
   TasksDB-PTod databases.

   There are five main functions:
   - tasks_open() - open PDB files with Tasks inside.
   - tasks_read() - read tasks from opened files to Tasks structure.
   - tasks_write() - writes tasks from Tasks structure to corresponding PDB files.
   - tasks_close() - close openend PDB files with Tasks.
   - tasks_free() - free memory, allocated inside Tasks structure.

   To operate with Tasks structure there are the next functions:
   - tasks_task_get()
   - tasks_task_add()
   - tasks_task_edit()
   - tasks_task_delete()
*/

#ifndef _TASKS_H_
#define _TASKS_H_

#include <stdint.h>
#include <sys/queue.h>
#include <sys/time.h>
#include "pdb/pdb.h"


/**
   Task priority values.
*/
enum TaskPriority
{
	PRIORITY_1, /**< Priority 1 */
	PRIORITY_2, /**< Priority 2 */
	PRIORITY_3, /**< Priority 3 */
	PRIORITY_4, /**< Priority 4 */
	PRIORITY_5  /**< Priority 5 */
};
#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef enum TaskPriority TaskPriority;
#endif

/**
   Alarm configuration.
*/
struct Alarm
{
	uint8_t alarmHour;    /**< Alarm hour */
	uint8_t alarmMinute;  /**< Alarm minute */
	uint16_t daysEarlier; /**< Days before due date to raise alarm */
};
typedef struct Alarm Alarm;

/**
   Repeater's interval ranges.
*/
enum RepeatRange
{
	N_DAYS,           /**< Repeat every N days */
	N_WEEKS,          /**< Repeat every N weeks */
	N_MONTHS_BY_DAY,  /**< Repeat every N month by day */
	N_MONTHS_BY_DATE, /**< Repeat every N month by date */
	N_YEARS           /**< Repeat every N years */
};
#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef enum RepeatRange RepeatRange;
#endif

/**
   Repeat data.
 */
struct Repeat
{
	RepeatRange range; /**< Range of interval to repeat */
	uint8_t day;       /**< Repeat until this day. 0 if forever. */
	uint8_t month;     /**< Repeat until this month. 0 if forever. */
	uint16_t year;     /**< Repeat until this year. 0 if forever. */
	uint8_t interval;  /**< Interval of repeat */
};
typedef struct Repeat Repeat;

/**
   One task from Tasks application.
*/
struct Task
{
	char * header;         /**< Task header */
	char * text;           /**< Task note */
	char * category;       /**< Human-readable category of task */
	TaskPriority priority; /**< Priority (from 1 to 5) */
	uint8_t dueDay;        /**< Day of due date. 0 if no due date. */
	uint8_t dueMonth;      /**< Month of due date. 0 if no due date. */
	uint16_t dueYear;      /**< Year of due date. 0 if no due date. */
	Alarm * alarm;         /**< Alarm data or NULL if no alarm */
	Repeat * repeat;       /**< Repeat data or NULL if no repeat enabled */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	TAILQ_ENTRY(Task) pointers;
	PDBRecord * _record_todo;  /**< PDBRecord from ToDoDB */
	PDBRecord * _record_tasks; /**< PDBRecord from TasksDB-PTod */
#endif
};
typedef struct Task Task;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
TAILQ_HEAD(TasksQueue, Task);
typedef struct TasksQueue TasksQueue;
#endif

/**
   Tasks from PDB file.
*/
struct Tasks
{
	TasksQueue queue;  /**< Tasks queue */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	PDB * _pdb_tododb; /**< PDB structure from ToDoDB file */
	PDB * _pdb_tasks;  /**< PDB structure from TasksDB-PTod file */
#endif
};
typedef struct Tasks Tasks;


/**
   \defgroup tasks_ops Operate with Tasks from corresponding PDB files

   Set of functions to operate with PDB structures specific for Tasks
   application.

   @{
*/

/**
   File descriptors necessary for Tasks files.
*/
struct TasksFD
{
	int todo_fd;  /**< File descriptor for ToDoDB */
	int tasks_fd; /**< File descriptor for TasksDB-PTod */
};
typedef struct TasksFD TasksFD;

/**
   Opens Tasks-related PDB files and returns it's descriptors.

   @param[in] pathToDoDB  Path to ToDoDB PDB file.
   @param[in] pathTasksDB Path to TasksDB-PTod PDB file.
   @return Structure with two elements with file descriptors or -1 in any
   element on error.
*/
TasksFD tasks_open(const char * pathToDoDB, const char * pathTasksDB);

/**
   Read tasks from PDB files.

   Function will read Tasks data from opened by tasks_open() PDB
   files. Memory for Tasks structure will be initialized inside this
   function and can be freed by tasks_free().

   @param[in] tfd Initialized TasksFD with valid file descriptors inside.
   @return Tasks structure on success, NULL on error.
*/
Tasks * tasks_read(TasksFD tfd);

/**
   Write data from Tasks structure to PDB files.

   Function will write data from Tasks structure to file descriptors
   from TasksFD structure.

   @param[in] tfd TasksFD structure with valid file descriptors inside.
   @param[in] tasks Pointer to Tasks structure.
   @return 0 on success or non-zero on error.
*/
int tasks_write(TasksFD tfd, Tasks * tasks);

/**
   Close opened Tasks files.

   @param[in] tfd TasksFD structure with valid file descriptors inside.
*/
void tasks_close(TasksFD tfd);

/**
   Free Tasks structure.

   @param[in] tasks Tasks structure.
*/
void tasks_free(Tasks * tasks);

/**
   @}
*/


/**
   \defgroup task_ops Actions, specific for one task entry

   Functions to operate with one task from Tasks application.

   @{
*/

/**
   Returns task from Tasks queue, if exists.

   Will be search task by it's header. If mutliple tasks with same
   header exists — first found task will be returned. If no task with
   given header found — NULL value will be returned.

   @param[in] tasks Tasks structure.
   @param[in] header Will search task with this header.
   @return Pointer to task or NULL if no task found or error occured.
*/
Task * tasks_task_get(Tasks * tasks, char * header);

/**
   @}
*/

#endif
