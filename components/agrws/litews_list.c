/*
 *   Copyright (c) 2014 - 2017 Kulykov Oleh <info@resident.name>
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *   THE SOFTWARE.
 */


#include "litews_list.h"
#include "litews_memory.h"

_litews_list * litews_list_create(void) 
{
	return (_litews_list *)litews_malloc_zero(sizeof(_litews_list));
}

void litews_list_delete(_litews_list * list) 
{
	_litews_list * cur = list;
	while (cur) 
	{
		_litews_list * prev = cur;
		cur = cur->next;
		litews_free(prev);
	}
}

void litews_list_delete_clean(_litews_list ** list) 
{
	if (list) 
	{
		litews_list_delete(*list);
		*list = NULL;
	}
}

void litews_list_append(_litews_list * list, _litews_node_value value) 
{
	if (list) 
	{
		_litews_list * cur = list;
		while (cur->next) 
		{
			cur = cur->next;
		}
		cur->next = (_litews_node *)litews_malloc_zero(sizeof(_litews_node));
		cur->next->value = value;
	}
}

