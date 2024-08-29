#include <stddef.h>
#include "log.h"
#include "org_notes.h"


int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	int fd;
	if((fd = org_notes_open(argv[1])) == -1)
	{
		return 1;
	}

	if(org_notes_write(fd, "Just a header TEST", NULL, NULL))
	{
		return 1;
	}
	if(org_notes_write(fd, "Just a header TEST2", NULL, "Unfiled"))
	{
		return 1;
	}
	if(org_notes_write(fd, "Header with tag TEST", NULL, "tag"))
	{
		return 1;
	}
	if(org_notes_write(fd, "Header with text TEST", "Some test text\nSecond line", NULL))
	{
		return 1;
	}
	if(org_notes_write(fd, "Header with text and tag TEST",
					   "Some test text 2\nLast line", "tag2"))
	{
		return 1;
	}

	if(org_notes_close(fd))
	{
		log_close();
		return 1;
	}

	log_close();
	return 0;
}
