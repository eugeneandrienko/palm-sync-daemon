#include "pdb_memos.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	PDB * pdb;
	if((pdb = pdb_memos_read(argv[1])) == NULL)
	{
		return 1;
	}

	PDBMemo * memo;
	if((memo = pdb_memos_memo_add(pdb, "Test 2", "Sample text 2", "Personal")) == NULL)
	{
		return 1;
	}

	if((memo = pdb_memos_memo_get(pdb, "Test")) == NULL)
	{
		return 1;
	}
	if(pdb_memos_memo_edit(pdb, memo, "Test 3", "Sample text 3", "Personal"))
	{
		return 1;
	}

	if((memo = pdb_memos_memo_get(pdb, "Test 2")) == NULL)
	{
	 	return 1;
	}
    if(pdb_memos_memo_delete(pdb, memo))
	{
	 	return 1;
	}

	pdb_memos_write(argv[1], pdb);

	if((pdb = pdb_memos_read(argv[1])) == NULL)
	{
		return 1;
	}
	PDBRecord * record;
	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		log_write(LOG_INFO, "Header: %s", ((PDBMemo *)record->data)->header);
		log_write(LOG_INFO, "Text: %s", ((PDBMemo *)record->data)->text);
		log_write(LOG_INFO, "Category ID: %d", record->attributes & 0x0f);
	}

	log_close();
	return 0;
}
