/**
   @author Eugene Andrienko
   @brief Functions to read/write from/to Palm device.
*/

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libpisock/pi-socket.h>
#include "log.h"
#include "palm.h"
#include "config.h"


/**
   Check is file a device file with Palm PDA connected to it

   Function checks is given device file belongs to Palm PDA. And is this PDA in
   HotSync and ready to synchronize?

   @param device Path to file
   @return 0 if Palm connected to system, otherwise returns 1
*/
int palm_device_test(const char * device)
{
	/* Is file exists* */
	if(access(device, R_OK | W_OK))
	{
		return 1;
	}

	/* Is file a device file? */
	struct stat deviceStat;
	if(stat(device, &deviceStat))
	{
		log_write(LOG_DEBUG, "Cannot stat %s file: %s", device, strerror(errno));
		return 1;
	}
	if(!S_ISCHR(deviceStat.st_mode))
	{
		log_write(LOG_DEBUG, "%s file is not a character file", device);
		return 1;
	}

	/* Is file belongs to Palm? */
	int sd = -1;
	int result = 0;
	if((sd = pi_socket(PI_AF_PILOT, PI_SOCK_STREAM, PI_PF_DLP)) < 0)
	{
		log_write(LOG_WARNING, "Cannot create socket for Palm: %s", strerror(errno));
		return 1;
	}
	if((result = pi_bind(sd, device)) < 0)
	{
		log_write(LOG_ERR, "Cannot bind %s", device);
		if(result == PI_ERR_SOCK_INVALID)
		{
			log_write(LOG_ERR, "Socket is invalid for %s", device);
		}
		return 1;
	}

	log_write(LOG_INFO, "Palm device %s connected to the system", device);
	return 0;
}
