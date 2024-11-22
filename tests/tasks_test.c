#include "log.h"
#include "pdb/tasks.h"

int main(int argc, char * argv[])
{
	if(argc != 3)
	{
		return 1;
	}
	log_init(1, 0);

	/* Read and write back ToDoDB and TasksDB-PTod files */
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
	Task * task;
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
