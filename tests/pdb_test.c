#include "pdb.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	/* Read and write PDB file */
	int fd = pdb_open(argv[1]);
	if(fd == -1)
	{
		return 1;
	}
	PDBFile pdbFile;
	if(pdb_read(fd, &pdbFile, 1))
	{
		return 1;
	}
	if(pdb_write(fd, &pdbFile))
	{
		return 1;
	}
	pdb_close(fd);
	pdb_free(&pdbFile);

	/* Check the result */
	fd = pdb_open(argv[1]);
	if(fd == -1)
	{
		return 1;
	}
	if(pdb_read(fd, &pdbFile, 1))
	{
		return 1;
	}

	log_write(LOG_INFO, "Database name: %s", pdbFile.dbname);
	log_write(LOG_INFO, "Attributes: %d", pdbFile.attributes);
	log_write(LOG_INFO, "Version: %d", pdbFile.version);
	log_write(LOG_INFO, "Creation datetime: %lu", pdbFile.ctime);
	log_write(LOG_INFO, "Modification datetime: %lu", pdbFile.mtime);
	log_write(LOG_INFO, "Last backup datetime: %lu", pdbFile.btime);
	log_write(LOG_INFO, "Modification number: %d", pdbFile.modificationNumber);
	log_write(LOG_INFO, "Application info offset: 0x%02x", pdbFile.appInfoOffset);
	log_write(LOG_INFO, "Sort info offset: 0x%02x", pdbFile.sortInfoOffset);
	log_write(LOG_INFO, "Database type ID: 0x%x", pdbFile.databaseTypeID);
	log_write(LOG_INFO, "Creator ID: 0x%x", pdbFile.creatorID);
	log_write(LOG_INFO, "Unique ID seed: %d", pdbFile.seed);
	log_write(LOG_INFO, "Qty of records: %d", pdbFile.recordsQty);

	PDBRecord * record;
	TAILQ_FOREACH(record, &(pdbFile.records), pointers)
	{
		log_write(LOG_INFO, "Offset: 0x%08x", record->offset);
		log_write(LOG_INFO, "Attribute: 0x%02x", record->attributes);
		log_write(LOG_INFO, "Unique ID: 0x%02x 0x%02x 0x%02x",
				  record->id[0], record->id[1], record->id[2]);
	}

	PDBCategories categories = *(pdbFile.categories);
	log_write(LOG_INFO, "Renamed categories: %d", categories.renamedCategories);
	log_write(LOG_INFO, "Last unique ID: 0x%02x", categories.lastUniqueId);
	log_write(LOG_INFO, "Padding: %d", categories.padding);
	for(int i = 0; i < PDB_CATEGORIES_STD_LEN; i++)
	{
		log_write(LOG_INFO, "Name: %s", categories.names[i]);
		log_write(LOG_INFO, "ID: %d", categories.ids[i]);
	}

	pdb_close(fd);
	pdb_free(&pdbFile);
	log_close();
	return 0;
}
