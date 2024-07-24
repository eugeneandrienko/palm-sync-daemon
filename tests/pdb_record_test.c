#include "pdb.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	PDB * pdb;
	int fd = pdb_read(argv[1], 1, &pdb);
	if(fd == -1)
	{
		return 1;
	}

	/* Add two records and delete middle record */
	PDBRecord * record;
	if((record = pdb_record_create(pdb, 0x01, PDB_RECORD_ATTR_DIRTY | 2)) == NULL)
	{
		log_write(LOG_ERR, "Failed to write new record #1");
		return 1;
	}

	if(pdb_record_create(pdb, 0x02, PDB_RECORD_ATTR_DELETED | 3) == NULL)
	{
		log_write(LOG_ERR, "Failed to write new record #2");
		return 1;
	}
	if(pdb_record_delete(pdb, record))
	{
		log_write(LOG_ERR, "Failed to delete record #1");
		return 1;
	}

	log_write(LOG_INFO, "Application info offset: 0x%02x", pdb->appInfoOffset);
	log_write(LOG_INFO, "Qty of records: %d", pdb->recordsQty);

	TAILQ_FOREACH(record, &pdb->records, pointers)
	{
		log_write(LOG_INFO, "Offset: 0x%08x", record->offset);
		log_write(LOG_INFO, "Attribute: 0x%02x", record->attributes);
		log_write(LOG_INFO, "Unique ID: 0x%02x 0x%02x 0x%02x",
				  record->id[0], record->id[1], record->id[2]);
	}

	pdb_free(fd, pdb);
	log_close();
	return 0;
}
