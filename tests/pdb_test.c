#include "pdb/pdb.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	/* Read and write PDB file */
	PDB * pdb;
	int fd;
	if((fd = pdb_open(argv[1])) == -1)
	{
		return 1;
	}
	if((pdb = pdb_read(fd, true)) == NULL)
	{
		return 1;
	}
	if(pdb_write(fd, pdb))
	{
		return 1;
	}
	pdb_close(fd);
	pdb_free(pdb);

	/* Check the result */
	PDB * pdb2;
	if((fd = pdb_open(argv[1])) == -1)
	{
		return 1;
	}
	if((pdb2 = pdb_read(fd, true)) == NULL)
	{
		return 1;
	}

	log_write(LOG_INFO, "Database name: %s", pdb2->dbname);
	log_write(LOG_INFO, "Attributes: %d", pdb2->attributes);
	log_write(LOG_INFO, "Version: %d", pdb2->version);
	log_write(LOG_INFO, "Creation datetime: %lu", pdb2->ctime);
	log_write(LOG_INFO, "Modification datetime: %lu", pdb2->mtime);
	log_write(LOG_INFO, "Last backup datetime: %lu", pdb2->btime);
	log_write(LOG_INFO, "Modification number: %d", pdb2->modificationNumber);
	log_write(LOG_INFO, "Application info offset: 0x%02x", pdb2->appInfoOffset);
	log_write(LOG_INFO, "Sort info offset: 0x%02x", pdb2->sortInfoOffset);
	log_write(LOG_INFO, "Database type ID: 0x%x", pdb2->databaseTypeID);
	log_write(LOG_INFO, "Creator ID: 0x%x", pdb2->creatorID);
	log_write(LOG_INFO, "Unique ID seed: %d", pdb2->seed);
	log_write(LOG_INFO, "Qty of records: %d", pdb2->recordsQty);

	PDBRecord * record;
	TAILQ_FOREACH(record, &pdb2->records, pointers)
	{
		log_write(LOG_INFO, "Offset: 0x%08x", record->offset);
		log_write(LOG_INFO, "Attribute: 0x%02x", record->attributes);
		log_write(LOG_INFO, "Unique ID: 0x%02x 0x%02x 0x%02x",
				  record->id[0], record->id[1], record->id[2]);
	}

	PDBCategories * categories = pdb2->categories;
	log_write(LOG_INFO, "Renamed categories: %d", categories->renamedCategories);
	log_write(LOG_INFO, "Last unique ID: 0x%02x", categories->lastUniqueId);
	log_write(LOG_INFO, "Padding: %d", categories->padding);
	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
	{
		log_write(LOG_INFO, "Name: %s", categories->names[i]);
		log_write(LOG_INFO, "ID: %d", categories->ids[i]);
	}

	pdb_close(fd);
	pdb_free(pdb2);
	log_close();
	return 0;
}
