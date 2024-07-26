#include "helper.h"
#include "log.h"


int main(int argc, char * argv[])
{
	log_init(1, 0);

	SyncSettings settings;
	settings.dataDir = "/tmp/";
	if(check_previous_pdbs(&settings))
	{
		log_write(LOG_ERR, "check_previous_pdbs returned an error");
		return -1;
	}
	if(settings.prevDatebookPDB == NULL && settings.prevMemosPDB == NULL &&
	   settings.prevTodoPDB == NULL)
	{
		log_write(LOG_INFO, "NO FILES");
	}
	else
	{
		log_write(LOG_INFO, "%s", settings.prevDatebookPDB);
		log_write(LOG_INFO, "%s", settings.prevMemosPDB);
		log_write(LOG_INFO, "%s", settings.prevTodoPDB);
	}

	log_close();
	return 0;
}
