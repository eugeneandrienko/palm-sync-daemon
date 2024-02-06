#include <string.h>
#include "helper.h"
#include "log.h"


static void iconv_utf8_to_cp1251_test();


int main(int argc, char * argv[])
{
	log_init(1, 0);

	iconv_utf8_to_cp1251_test();

	log_close();
	return 0;
}

static void iconv_utf8_to_cp1251_test()
{
	char * USUAL_STRING = "Usual string";
	char * CYR_STRING = "Кириллическая строка";

	log_write(LOG_INFO, "UTF8 string: \"%s\", len = %d",
			  USUAL_STRING, strlen(USUAL_STRING));
	log_write(LOG_INFO, "CP1251 string: \"%s\", len = %d",
			  iconv_utf8_to_cp1251(USUAL_STRING),
			  strlen(iconv_utf8_to_cp1251(USUAL_STRING)));
	log_write(LOG_INFO, "UTF8 string: \"%s\", len = %d",
			  CYR_STRING, strlen(CYR_STRING));
	log_write(LOG_INFO, "CP1251 string: \"%s\", len = %d",
			  iconv_utf8_to_cp1251(CYR_STRING),
			  strlen(iconv_utf8_to_cp1251(CYR_STRING)));
}
