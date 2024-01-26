#include "pdb.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	int fd = pdb_open(argv[1]);
	if(fd == -1)
	{
		return 1;
	}
	PDBFile * pdbFile;
	if((pdbFile = pdb_read(fd, 1)) == NULL)
	{
		return 1;
	}

	/* Add two records, delete middle record and edit last record */
	PDBRecord * record;
	if((record = pdb_record_add(pdbFile, 0x01, PDB_RECORD_ATTR_DIRTY | 2)) == NULL)
	{
		log_write(LOG_ERR, "Failed to write new record #1");
		return 1;
	}

	if(pdb_record_add(pdbFile, 0x02, PDB_RECORD_ATTR_DELETED | 3) == NULL)
	{
		log_write(LOG_ERR, "Failed to write new record #2");
		return 1;
	}
	if(pdb_record_delete(pdbFile, record))
	{
		log_write(LOG_ERR, "Failed to delete record #2");
		return 1;
	}
	record = pdb_record_get(pdbFile, 2);
	if(record == NULL)
	{
		log_write(LOG_ERR, "Failed to ger record #2");
		return 1;
	}
	record->attributes = PDB_RECORD_ATTR_DIRTY | 4;

	log_write(LOG_INFO, "Application info offset: 0x%02x", pdbFile->appInfoOffset);
	log_write(LOG_INFO, "Qty of records: %d", pdbFile->recordsQty);

	TAILQ_FOREACH(record, &pdbFile->records, pointers)
	{
		log_write(LOG_INFO, "Offset: 0x%08x", record->offset);
		log_write(LOG_INFO, "Attribute: 0x%02x", record->attributes);
		log_write(LOG_INFO, "Unique ID: 0x%02x 0x%02x 0x%02x",
				  record->id[0], record->id[1], record->id[2]);
	}

	pdb_close(fd);
	pdb_free(pdbFile);
	log_close();
	return 0;
}
