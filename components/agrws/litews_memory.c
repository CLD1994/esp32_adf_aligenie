
#include "aligenie_os.h"
#include <string.h>
#include "litews_log.h"

void * litews_malloc(const size_t size) 
{
    if (size > 0) 
    {
        void * mem = AG_OS_MALLOC(size);
        return mem;
    }
    return NULL;
}

void * litews_malloc_zero(const size_t size) 
{
    void * mem = litews_malloc(size);
    if (mem == NULL)
    {
        LOGE_LITEWS("MALLOC MEM FAILED!!");
    }
    else
    {
        AG_OS_MEMSET(mem, 0, size);
    }
    return mem;
}

void litews_free(void * mem) 
{
    if (mem) 
    {
        AG_OS_FREE(mem);
    }
}

void litews_free_clean(void ** mem) 
{
    if (mem) 
    {
        litews_free(*mem);
        *mem = NULL;
    }
}

