#include "pdb/memos.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	/* Read and write PDB Memos file */
	Memos * memos;
	int fd;
	if((fd = memos_open(argv[1])) == -1)
	{
		log_write(LOG_ERR, "Failed to open file: %s", argv[1]);
		return 1;
	}
	if((memos = memos_read(fd)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read memos");
		return 1;
	}
	if(memos_write(fd, memos))
	{
		log_write(LOG_ERR, "Failed to write memos");
		return 1;
	}
	memos_close(fd);
	memos_free(memos);

	/* Check the result */
	if((fd = memos_open(argv[1])) == -1)
	{
		log_write(LOG_ERR, "Failed to open file2: %s", argv[1]);
		return 1;
	}
	if((memos = memos_read(fd)) == NULL)
	{
		log_write(LOG_ERR, "Failed to read memos2");
		return 1;
	}

	Memo * memo;
	TAILQ_FOREACH(memo, &memos->queue, pointers)
	{
		log_write(LOG_INFO, "Header: %s", memo->header);
		log_write(LOG_INFO, "Text: %s", memo->text);
		log_write(LOG_INFO, "Category: %s", memo->category);
	}

	memos_close(fd);
	memos_free(memos);
	log_close();
	return 0;
}
