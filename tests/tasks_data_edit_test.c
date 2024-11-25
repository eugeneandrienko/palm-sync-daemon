#include "log.h"
#include "pdb/tasks.h"

int main(int argc, char * argv[])
{
	if(argc != 3)
	{
		return 1;
	}
	log_init(1, 0);

	Tasks * tasks;
	TasksFD tfd = tasks_open(argv[1], argv[2]);
	if(tfd.todo_fd == -1)
	{
		log_write(LOG_ERR, "Failed to open ToDoDB file: %s", argv[1]);
		return 1;
	}
	if(tfd.tasks_fd == -1)
	{
		log_write(LOG_ERR, "Failed to open TasksDB-PTod file: %s", argv[2]);
		return 1;
	}
	if((tasks = tasks_read(tfd)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read tasks");
		return 1;
	}

	/* Starting to edit the tasks.
	   Existing tasks:
	   1) "Test task", "Test note", category: Unfiled, priority 2,
	      due date: 2024-09-09, alarm time: 16:45, alarm days earlier: 5,
		  repeat range: N days, repeat interval: 1, repeat until: 2024-09-15.
	   2) "Repeat every other week", category: Unfiled, priority: 5, due date:
	      2024-10-29, repeat range: N weeks, repeat interval: 2, repeat until:
		  forever.
	   3) "Repeat every week", category: Unfiled, priority: 5, due date:
	      2024-10-29, repeat range: N weeks, repeat interval: 1, repeat until:
		  forever.
	   4) "Just a header", category: Personal, priority: 1.
	*/
	Task * task;
	if((task = tasks_task_add(tasks, "New task", "Note for new task", "Personal",
							  PRIORITY_3)) == NULL)
	{
		log_write(LOG_ERR, "Failed to add new task");
		return 1;
	}
	if((task = tasks_task_get(tasks, "Repeat every other week")) == NULL)
	{
		log_write(LOG_ERR, "Failed to get task [1]");
		return 1;
	}
	if(tasks_task_set_due(task, 2025, 5, 11))
	{
		log_write(LOG_ERR, "Failed to set due date");
		return 1;
	}
	Alarm alarm = {
		.alarmHour = 9,
		.alarmMinute = 11,
		.daysEarlier = 2
	};
	if(tasks_task_set_alarm(task, &alarm))
	{
		log_write(LOG_ERR, "Failed to set alarm");
		return 1;
	}
	Repeat repeat = {
		.range = N_YEARS,
		.day = 20,
		.month = 2,
		.year = 2025,
		.interval = 3
	};
	if(tasks_task_set_repeat(task, &repeat))
	{
		log_write(LOG_ERR, "Failed to set repeat data");
		return 1;
	}
	if((task = tasks_task_get(tasks, "Repeat every week")) == NULL)
	{
		log_write(LOG_ERR, "Failed to get task [2]");
		return 1;
	}
	if(tasks_task_delete(tasks, task))
	{
		log_write(LOG_ERR, "Failed to delete task");
		return 1;
	}

	/* List of tasks after editing:
	   1) "Test task", "Test note", category: Unfiled, priority 2,
	      due date: 2024-09-09, alarm time: 16:45, alarm days earlier: 5,
		  repeat range: N days, repeat interval: 1, repeat until: 2024-09-15.
	   2) "Repeat every other week", category: Unfiled, priority: 5, due date:
	      2025-05-11, alarm time: 09:11, alarm days earlier: 2, repeat range:
		  N years, repeat interval: 3, repeat until: 2025-02-20.
	   3) "Just a header", category: Personal, priority: 1.
	   4) "New task", "Note for new task", category: "Personal", priority: 3.
	*/
	if(tasks_write(tfd, tasks))
	{
		log_write(LOG_ERR, "Failed to write tasks");
		return 1;
	}
	tasks_close(tfd);
	tasks_free(tasks);

	/* Read newly written files */
	tfd = tasks_open(argv[1], argv[2]);
	if(tfd.todo_fd == -1)
	{
		log_write(LOG_ERR, "Failed to open ToDoDB file: %s", argv[1]);
		return 1;
	}
	if(tfd.tasks_fd == -1)
	{
		log_write(LOG_ERR, "Failed to open TasksDB-PTod file: %s", argv[2]);
		return 1;
	}
	if((tasks = tasks_read(tfd)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read tasks");
		return 1;
	}
	log_write(LOG_INFO, "Read these data:");
	TAILQ_FOREACH(task, &tasks->queue, pointers)
	{
		log_write(LOG_INFO, "Header: %s", task->header);
		log_write(LOG_INFO, "Note: %s", task->text);
		log_write(LOG_INFO, "Category: %s", task->category);
		switch(task->priority)
		{
		case PRIORITY_1:
			log_write(LOG_INFO, "Priority: 1");
			break;
		case PRIORITY_2:
			log_write(LOG_INFO, "Priority: 2");
			break;
		case PRIORITY_3:
			log_write(LOG_INFO, "Priority: 3");
			break;
		case PRIORITY_4:
			log_write(LOG_INFO, "Priority: 4");
			break;
		case PRIORITY_5:
			log_write(LOG_INFO, "Priority: 5");
			break;
		default:
			log_write(LOG_ERR, "Unknown priority!");
		}
		if(task->dueYear == 0 || task->dueMonth == 0 || task->dueDay == 0)
		{
			log_write(LOG_INFO, "Due date: -");
		}
		else
		{
			log_write(LOG_INFO, "Due date: %04d-%02d-%02d", task->dueYear,
					  task->dueMonth, task->dueDay);
		}
		if(task->alarm != NULL)
		{
			log_write(LOG_INFO, "Alarm time: %02d:%02d", task->alarm->alarmHour,
					  task->alarm->alarmMinute);
			log_write(LOG_INFO, "Alarm days earlier: %d",
					  task->alarm->daysEarlier);
		}
		else
		{
			log_write(LOG_INFO, "No alarm set");
		}
		if(task->repeat != NULL)
		{
		    char * range;
			switch(task->repeat->range)
			{
			case N_DAYS:
				range = "days";
				break;
			case N_WEEKS:
				range = "weeks";
				break;
			case N_MONTHS_BY_DAY:
				range = "month by day";
				break;
			case N_MONTHS_BY_DATE:
				range = "month by date";
				break;
			case N_YEARS:
				range = "years";
				break;
			default:
				range = "unknown range!";
			}
			log_write(LOG_INFO, "Repeat range: N %s", range);
			log_write(LOG_INFO, "Repeat interval: %d", task->repeat->interval);
			if(task->repeat->month == 0)
			{
				log_write(LOG_INFO, "Repeat until: forever");
			}
			else
			{
				log_write(LOG_INFO, "Repeat until: %04d-%02d-%02d",
						  task->repeat->year, task->repeat->month,
						  task->repeat->day);
			}
		}
		else
		{
			log_write(LOG_INFO, "No repeat");
		}
	}

	tasks_close(tfd);
	tasks_free(tasks);
	log_close();
	return 0;
}
