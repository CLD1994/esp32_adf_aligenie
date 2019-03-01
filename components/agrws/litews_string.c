
#include "litews_string.h"
#include "litews_memory.h"
#include "aligenie_os.h"

char * litews_string_copy(const char * str) 
{
	return str ? litews_string_copy_len(str, strlen(str)) : NULL;
}

char * litews_string_copy_len(const char * str, const size_t len) 
{
	char * s = (str && len > 0) ? (char *)litews_malloc(len + 1) : NULL;
	if (s) 
	{
		AG_OS_MEMCPY(s, str, len);
		s[len] = 0;
		return s;
	}
	return NULL;
}

void litews_string_delete(char * str) 
{
	litews_free(str);
}

void litews_string_delete_clean(char ** str) 
{
	if (str) 
	{
		litews_free(*str);
		*str = NULL;
	}
}

