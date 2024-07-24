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
	PDB * pdb;
	if((pdb = pdb_memos_read(argv[1])) == NULL)
	{
		return 1;
	}
	if(pdb_memos_write(argv[1], pdb))
	{
		return 1;
	}

	/* Check the result */
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

	pdb_memos_free(pdb);
	pdb_free(-1, pdb);
	log_close();
	return 0;
}
