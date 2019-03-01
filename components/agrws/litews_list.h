
#ifndef __LITEWS_LIST_H__
#define __LITEWS_LIST_H__ 1

#include <stdio.h>

typedef union _litews_node_value_union 
{
	void * object;
	char * string;
	int int_value;
	unsigned int uint_value;
} _litews_node_value;

typedef struct _litews_node_struct 
{
	_litews_node_value value;
	struct _litews_node_struct * next;
} _litews_node, _litews_list;


_litews_list * litews_list_create(void);

void litews_list_delete(_litews_list * list);

void litews_list_delete_clean(_litews_list ** list);

void litews_list_append(_litews_list * list, _litews_node_value value);

#endif

