#include <string.h>
#include "pdb/pdb.h"
#include "log.h"

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		return 1;
	}
	log_init(1, 0);

	int fd;
	PDB * pdb;
	if((fd = pdb_open(argv[1])) == -1)
	{
		return 1;
	}
	if((pdb = pdb_read(fd, true)) == NULL)
	{
		return 1;
	}

	/* Add two categories and delete middle category */
	if(pdb_category_add(pdb, "NEW") == UINT8_MAX)
	{
		log_write(LOG_ERR, "Failed to add category \"NEW\"");
		return 1;
	}
	if(pdb_category_add(pdb, "NEW2") == UINT8_MAX)
	{
		log_write(LOG_ERR, "Failed to add category \"NEW2\"");
		return 1;
	}
	if(pdb_category_delete(pdb, 3))
	{
		log_write(LOG_ERR, "Failed to delete category #%d", 3);
		return 1;
	}
	char * name = pdb_category_get_name(pdb, 0);
	if(name == NULL)
	{
		log_write(LOG_ERR, "Failed to read category #%d", 0);
		return 1;
	}
	strcpy(name, "EDITED");

	PDBCategories * categories = pdb->categories;
	log_write(LOG_INFO, "Renamed categories: %d", categories->renamedCategories);
	log_write(LOG_INFO, "Last unique ID: 0x%02x", categories->lastUniqueId);
	log_write(LOG_INFO, "Padding: %d", categories->padding);
	for(int i = 0; i < PDB_CATEGORIES_STD_QTY; i++)
	{
		log_write(LOG_INFO, "Name: %s", categories->names[i]);
		log_write(LOG_INFO, "ID: %d", categories->ids[i]);
	}

	pdb_free(pdb);
	pdb_close(fd);
	log_close();
	return 0;
}
