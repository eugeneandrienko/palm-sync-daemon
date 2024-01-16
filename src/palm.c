/**
   @author Eugene Andrienko
   @brief Functions to read/write from/to Palm device.
*/

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libpisock/pi-dlp.h>
#include <libpisock/pi-socket.h>
#include "log.h"
#include "palm.h"
#include "config.h"

#if HAVE_LIBUSB_1_0_LIBUSB_H == 1 &&			\
	HAVE_LIBUSB_1_0 == 1
#include <libusb-1.0/libusb.h>
#define PALM_VID 0x0830
#define PALM_PID 0x0061
#else
#warning "No libusb-1.0 - do not check VID/PID on connected device"
#endif


/**
   Check is file a device file with Palm PDA connected to it

   Function checks is given device file belongs to Palm PDA. And is this PDA in
   HotSync and ready to synchronize?

   @param device Path to file
   @param printErrors Print errors to syslog
   @return 0 if Palm connected to system, otherwise returns 1
*/
int palm_device_test(const char * device, int printErrors)
{
	/* Is file exists* */
	if(access(device, R_OK | W_OK))
	{
		if(printErrors)
		{
			log_write(LOG_ERR, "%s not readable/writable", device);
		}
		return 1;
	}

	/* Is file a device file? */
	struct stat deviceStat;
	if(stat(device, &deviceStat))
	{
		if(printErrors)
		{
			log_write(LOG_ERR, "Cannot stat %s file: %s", device, strerror(errno));
		}
		return 1;
	}
	if(!S_ISCHR(deviceStat.st_mode))
	{
		if(printErrors)
		{
			log_write(LOG_ERR, "%s file is not a character file", device);
		}
		return 1;
	}

	/* Is file belongs to Palm? */
#if HAVE_LIBUSB_1_0_LIBUSB_H == 1 &&			\
	HAVE_LIBUSB_1_0 == 1
	int result = 0;
	libusb_context * usbCtx;
	if(result = libusb_init(&usbCtx))
	{
		if(printErrors)
		{
			log_write(LOG_ERR, "Cannot initialize libusb");
			log_write(LOG_ERR, "%s: %s", libusb_error_name(result), libusb_strerror(result));
		}
		return 1;
	}

	libusb_device_handle * palmHandle;
	if((palmHandle = libusb_open_device_with_vid_pid(
			usbCtx, PALM_VID, PALM_PID)) == NULL)
	{
		if(printErrors)
		{
			log_write(
				LOG_WARNING,
				"Device with VID=0x%04x, PID=0x%04x not found - looks like Palm device not connected to system",
				PALM_VID,
				PALM_PID);
		}
		libusb_exit(usbCtx);
		return 1;
	}

	if(printErrors)
	{
		log_write(LOG_INFO, "Device with VID=0x%04x, PID=0x%04x connected to the system",
				  PALM_VID, PALM_PID);
	}

	libusb_close(palmHandle);
	libusb_exit(usbCtx);
#endif

	log_write(LOG_INFO, "Palm device %s connected to the system", device);
	return 0;
}

static void _log_system_info(struct SysInfo * info)
{
	log_write(LOG_DEBUG, "Device ROM version: major=%d, minor=%d, fix=%d, stage=%d, build=%d",
			  (info->romVersion >> 32) & 0x000000ff,
			  (info->romVersion >> 24) & 0x000000ff,
			  (info->romVersion >> 16) & 0x000000ff,
			  (info->romVersion >> 8)  & 0x000000ff,
			  (info->romVersion)       & 0x000000ff);
	log_write(LOG_DEBUG, "DLP protocol: %d.%d", info->dlpMajorVersion, info->dlpMinorVersion);
	log_write(LOG_DEBUG, "Compatible DLP protocol: %d.%d",
			  info->compatMajorVersion,
			  info->compatMinorVersion);
}

/**
   Open connection to Palm device

   @param device Path to symbolic device on the system
   @return Device descriptor or -1 if error happens
*/
int palm_open(char * device)
{
	int sd = -1;
	int result = 0;

	if((sd = pi_socket(PI_AF_PILOT, PI_SOCK_STREAM, PI_PF_DLP)) < 0)
	{
		log_write(LOG_WARNING, "Cannot create socket for Palm: %s", strerror(errno));
		return -1;
	}

	if((result = pi_bind(sd, device)) < 0)
	{
		log_write(LOG_ERR, "Cannot bind %s", device);
		if(result == PI_ERR_SOCK_INVALID)
		{
			log_write(LOG_ERR, "Socket is invalid for %s", device);
		}
		return -1;
	}

	if(pi_listen(sd, 1) < 0)
	{
		log_write(LOG_ERR, "Cannot listen %s", device);
		pi_close(sd);
		return -1;
	}

	result = pi_accept_to(sd, 0, 0, 0);
	if(result < 0)
	{
		log_write(LOG_ERR, "Cannot accept data on %s", device);
		pi_close(sd);
		return -1;
	}
	sd = result;

	struct SysInfo sysInfo;
	if(dlp_ReadSysInfo(sd, &sysInfo) < 0)
	{
		log_write(LOG_ERR, "Cannot read system info from Palm on %s", device);
		return -1;
	}
	_log_system_info(&sysInfo);

	dlp_OpenConduit(sd);
	return sd;
}

/**
   Close connection to Palm device.

   @param sd Device descriptor
*/
void palm_close(int sd)
{
	pi_close(sd);
}