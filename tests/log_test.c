#include <syslog.h>
#include "log.h"


int main(int argc, char * argv[])
{
	log_init(1);

	log_write(LOG_EMERG, "Test emerg");
	log_write(LOG_ALERT, "Test alert");
	log_write(LOG_CRIT, "Test crit");
	log_write(LOG_ERR, "Test err");
	log_write(LOG_WARNING, "Test warning");
	log_write(LOG_NOTICE, "Test notice");
	log_write(LOG_INFO, "Test info");
	log_write(LOG_DEBUG, "Test debug");
	log_write(255, "Test unknown priority");

	log_close();
	return 0;
}
