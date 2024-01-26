#include "pdb_memos.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	PDBMemos * memos;
	if((memos = pdb_memos_read(argv[1])) == NULL)
	{
		return 1;
	}

	PDBMemo * memo;
	if((memo = pdb_memos_memo_add(memos, "Test 2", "Sample text 2", "Personal")) == NULL)
	{
		return 1;
	}

	if((memo = pdb_memos_memo_get(memos, "Test")) == NULL)
	{
		return 1;
	}
	if(pdb_memos_memo_edit(memos, memo, "Test 3", "Sample text 3", "Personal"))
	{
		return 1;
	}

	if((memo = pdb_memos_memo_get(memos, "Test 2")) == NULL)
	{
		return 1;
	}
	if(pdb_memos_memo_delete(memos, memo))
	{
		return 1;
	}

	pdb_memos_write(argv[1], memos);

	if((memos = pdb_memos_read(argv[1])) == NULL)
	{
		return 1;
	}
	TAILQ_FOREACH(memo, &memos->memos, pointers)
	{
		log_write(LOG_INFO, "Header: %s", memo->header);
		log_write(LOG_INFO, "Text: %s", memo->text);
		log_write(LOG_INFO, "Category ID: %d", memo->record->attributes & 0x0f);
	}

	log_close();
	return 0;
}
