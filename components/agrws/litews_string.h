
#ifndef __LITEWS_STRING_H__
#define __LITEWS_STRING_H__ 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define litews_sprintf(s,l,f,...) sprintf_s(s,l,f,__VA_ARGS__)
#define litews_sscanf(s,f,...) sscanf_s(s,f,__VA_ARGS__)
#define litews_strerror(e) strerror(e)
#else
#define litews_sprintf(s,l,f,...) sprintf(s,f,__VA_ARGS__)
#define litews_sscanf(s,f,...) sscanf(s,f,__VA_ARGS__)
#define litews_strerror(e) strerror(e)
#endif

char * litews_string_copy(const char * str);

char * litews_string_copy_len(const char * str, const size_t len);

void litews_string_delete(char * str);

void litews_string_delete_clean(char ** str);

#endif
