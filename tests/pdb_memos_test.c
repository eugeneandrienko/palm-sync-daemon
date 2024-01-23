#include "pdb_memos.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	/* Read and write PDB Memos file */
	PDBMemos * memos;
	if((memos = pdb_memos_read(argv[1])) == NULL)
	{
		return 1;
	}
	if(pdb_memos_write(argv[1], memos))
	{
		return 1;
	}

	/* Check the result */
	if((memos = pdb_memos_read(argv[1])) == NULL)
	{
		return 1;
	}

	PDBMemo * memo;
	TAILQ_FOREACH(memo, &memos->memos, pointers)
	{
		log_write(LOG_INFO, "Header: %s", memo->header);
		log_write(LOG_INFO, "Text: %s", memo->text);
		log_write(LOG_INFO, "Category ID: %d", memo->categoryId);
		log_write(LOG_INFO, "Category name: %s", memo->categoryName);
	}

	pdb_memos_free(memos);
	log_close();
	return 0;
}
