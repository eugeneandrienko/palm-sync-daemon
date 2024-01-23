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

	/* Add two categories, delete middle category and edit first category */
	if(pdb_category_add(pdbFile, 4, "NEW"))
	{
		log_write(LOG_ERR, "Failed to add category \"NEW\"");
		return 1;
	}
	if(pdb_category_add(pdbFile, 5, "NEW2"))
	{
		log_write(LOG_ERR, "Failed to add category \"NEW2\"");
		return 1;
	}
	if(pdb_category_delete(pdbFile, 4))
	{
		log_write(LOG_ERR, "Failed to delete category #%d", 3);
		return 1;
	}
	char * name = pdb_category_get(pdbFile, 0);
	if(name == NULL)
	{
		log_write(LOG_ERR, "Failed to read category #%d", 0);
		return 1;
	}
	if(pdb_category_edit(name, "EDITED", 6))
	{
		log_write(LOG_ERR, "Failed to edit category #%d", 0);
		return 1;
	}

	PDBCategories * categories = pdbFile->categories;
	log_write(LOG_INFO, "Renamed categories: %d", categories->renamedCategories);
	log_write(LOG_INFO, "Last unique ID: 0x%02x", categories->lastUniqueId);
	log_write(LOG_INFO, "Padding: %d", categories->padding);
	for(int i = 0; i < PDB_CATEGORIES_STD_LEN; i++)
	{
		log_write(LOG_INFO, "Name: %s", categories->names[i]);
		log_write(LOG_INFO, "ID: %d", categories->ids[i]);
	}

	pdb_close(fd);
	pdb_free(pdbFile);
	log_close();
	return 0;
}
