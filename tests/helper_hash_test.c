#include "helper.h"
#include "log.h"


static void hash_test();


int main(int argc, char * argv[])
{
	log_init(1, 0);
	hash_test();
	log_close();
	return 0;
}

static void hash_test()
{
	char * str1 = "Test string 1";
	char * str2 = "Test string 2";
	char * str3 = "Test string 1";
	uint64_t hash1 = str_hash(str1, 13);
	uint64_t hash2 = str_hash(str2, 13);
	uint64_t hash3 = str_hash(str3, 13);

	if(hash1 != hash2)
	{
		log_write(LOG_INFO, "Hashes of str1 and str2 are not equal");
	}
	else
	{
		log_write(LOG_INFO, "Hashes of str1 and str2 are equal");
	}

	if(hash1 == hash3)
	{
		log_write(LOG_INFO, "Hashes of str1 and str3 are equal");
	}
	else
	{
		log_write(LOG_INFO, "Hashes of str1 and str3 are not equal");
	}
}
