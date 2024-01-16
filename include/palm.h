#ifndef _PALM_H_
#define _PALM_H_

/**
   Check is file a device file with Palm PDA connected to it

   @param device Path to file
   @param printErrors Print errors to syslog
   @return 0 if Palm connected to system, otherwise returns 1
*/
int palm_device_test(const char * device, int printErrors);

#endif
