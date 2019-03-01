
#ifndef __LITEWS_MEMORY_H__
#define __LITEWS_MEMORY_H__ 1

#include <stdio.h>
#include <stdlib.h>

// size > 0 => malloc
void * litews_malloc(const size_t size);

// size > 0 => malloc
void * litews_malloc_zero(const size_t size);

void litews_free(void * mem);

void litews_free_clean(void ** mem);

#endif


