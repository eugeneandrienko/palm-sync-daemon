#include "helper.h"
#include "log.h"


int main(int argc, char * argv[])
{
	log_init(1, 1);

	SyncSettings settings;
	PalmData data;
	settings.dataDir         = "/tmp/";
	settings.prevDatebookPDB = NULL;
	settings.prevMemosPDB    = NULL;
	settings.prevTodoPDB     = NULL;
	data.datebookDBPath      = "/tmp/datebook.pdb";
	data.memoDBPath          = "/tmp/memo.pdb";
	data.todoDBPath          = "/tmp/todo.pdb";
	if(save_as_previous_pdbs(&settings, &data))
	{
		log_write(LOG_ERR, "save_as_previous_pdbs returned an error");
		return -1;
	}

	log_close();
	return 0;
}
