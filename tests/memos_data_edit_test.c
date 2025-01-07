#include "pdb/memos.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	Memos * memos;
	int fd;
	if((fd = memos_open(argv[1])) == -1)
	{
		return 1;
	}
	if((memos = memos_read(fd)) == NULL)
	{
		return 1;
	}

	uint32_t memoId;
	if((memoId = memos_memo_add(memos, "Test 2", "Sample text 2", "Personal")) == 0)
	{
		return 1;
	}
	if(memos_memo_edit(memos, memoId, "Test 3", "Sample text 3", "Personal"))
	{
		return 1;
	}

	if(memos_memo_get_id(memos, "Test", NULL, &memoId) != 0)
	{
		return 1;
	}
    if(memos_memo_delete(memos, memoId))
	{
	 	return 1;
	}

	if((memoId = memos_memo_add(memos, "Тест 4", "Тестовая заметка", "Unfiled")) == 0)
	{
		return 1;
	}

	if(memos_write(fd, memos))
	{
		return 1;
	}
	memos_close(fd);
	memos_free(memos);

	if((fd = memos_open(argv[1])) == -1)
	{
		return 1;
	}
	if((memos = memos_read(fd)) == NULL)
	{
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
