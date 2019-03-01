
#ifndef __LITEWS_ERROR_H__
#define __LITEWS_ERROR_H__ 1

#include "litews_string.h"

struct litews_error_struct 
{
	int code;
	int http_error;
	char * description;
};

litews_error litews_error_create(void);

litews_error litews_error_new_code_descr(const int code, const char * description);

void litews_error_delete(litews_error error);

void litews_error_delete_clean(litews_error * error);

#endif
