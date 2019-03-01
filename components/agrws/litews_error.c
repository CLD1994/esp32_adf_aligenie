
#include "litewebsocket.h"
#include "litews_error.h"
#include "litews_string.h"
#include "litews_memory.h"


// private
litews_error litews_error_create(void) 
{
	return (litews_error)litews_malloc_zero(sizeof(struct litews_error_struct));
}

litews_error litews_error_new_code_descr(const int code, const char * description) 
{
	litews_error e = (litews_error)litews_malloc_zero(sizeof(struct litews_error_struct));
	e->code = code;
	e->description = litews_string_copy(description);
	return e;
}

void litews_error_delete(litews_error error) 
{
	if (error) 
	{
		litews_string_delete(error->description);
		litews_free(error);
	}
}

void litews_error_delete_clean(litews_error * error) 
{
	if (error) 
	{
		litews_error_delete(*error);
		*error = NULL;
	}
}

// public
int litews_error_get_code(litews_error error) 
{
	return error ? error->code : 0;
}

int litews_error_get_http_error(litews_error error) 
{
	return error ? error->http_error : 0;
}

const char * litews_error_get_description(litews_error error) 
{
	return error ? error->description : NULL;
}

