
/* websocket frame 

0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+

*/

#include "litews_frame.h"
#include "litews_memory.h"

#include <stdlib.h>
#include <string.h>
//#include <assert.h>
#include <time.h>

#include "litews_log.h"

_litews_frame * litews_frame_create_with_recv_data(const void * data, const size_t data_size) 
{
	if (data && data_size >= 2) 
	{
		const unsigned char * udata = (const unsigned char *)data;
		
		const litews_opcode opcode = (litews_opcode)(udata[0] & 0x0f);
		const unsigned int is_finshd = (udata[0] >> 7) & 0x01;
		const unsigned int is_masked = (udata[1] >> 7) & 0x01;
		const unsigned int payload = udata[1] & 0x7f;
		unsigned int header_size = is_masked ? 6 : 2;
		
		unsigned int expected_size = 0, mask_pos = 0;
		size_t index = 0;
		_litews_frame * frame = NULL;
		const unsigned char * actual_udata = NULL;
		unsigned char * unmasked = NULL;
		
		switch (payload) 
		{
			case 126: header_size += 2; break;
			case 127: header_size += 8; break;
			default: break;
		}
		
		if (data_size < header_size) 
		{
			return NULL;
		}
		
		switch (payload) 
		{
			case 126:
				expected_size |= ((unsigned int)udata[2]) << 8;
				expected_size |= (unsigned int)udata[3];
				mask_pos = 4;
				break;
				
			case 127:
				expected_size |= ((unsigned int)udata[6]) << 24;
				expected_size |= ((unsigned int)udata[7]) << 16;
				expected_size |= ((unsigned int)udata[8]) << 8;
				expected_size |= (unsigned int)udata[9];
				mask_pos = 10;
				break;
				
			default:
				if (payload <= 125) {
					mask_pos = 2;
					expected_size = payload;
				}
				break;
		}
		
		frame = litews_frame_create();
		
		frame->opcode = opcode;
		
		if (is_finshd) 
		{
		    frame->is_finished = litews_true;
		}
		frame->header_size = (unsigned char)header_size;
		
		if (is_masked) 
		{
			frame->is_masked = litews_true;
			AG_OS_MEMCPY(frame->mask, &udata[mask_pos], 4);
		}
		
		if (opcode == litews_opcode_connection_close || opcode == litews_opcode_pong) 
		{
			return frame;
		}
		
		if (!is_finshd) 
		{
			frame->is_finished = litews_false;
		}
		
		if (expected_size > 0) 
		{
			frame->data = litews_malloc(expected_size);
			frame->data_size = expected_size;
			actual_udata = udata + header_size;
			if (is_masked) 
			{
				unmasked = (unsigned char *)frame->data;
				for (index = 0; index < expected_size; index++) 
				{
					*unmasked = *actual_udata ^ frame->mask[index & 0x3];
					unmasked++; actual_udata++;
				}
			} 
			else 
			{
				AG_OS_MEMCPY(frame->data, actual_udata, expected_size);
			}
		}
		return frame;
	}
	return NULL;
}

void litews_frame_create_bin_header(_litews_frame * f, unsigned char * header, const size_t data_size, int flag) 
{
	const unsigned int size = (unsigned int)data_size;

	if(flag == litews_frame_start)
	{
	    LOGV_LITEWS("litews_frame_start");
    	*header++ = 0x00 | f->opcode;
	}
	else if(flag == litews_frame_continue)
	{
	    LOGV_LITEWS("litews_frame_continue");
	    *header++ = 0x00 | f->opcode;
	}
	else
	{
	    LOGV_LITEWS("litews_frame_end");
	    *header++ = 0x80 | f->opcode;
	}
	
	if (size < 126) 
	{
		*header++ = (size & 0xff) | (f->is_masked ? 0x80 : 0);
		f->header_size = 2;
	} 
	else if (size < 65536) 
	{
		*header++ = 126 | (f->is_masked ? 0x80 : 0);
		*header++ = (size >> 8) & 0xff;
		*header++ = size & 0xff;
		f->header_size = 4;
	} 
	else 
	{
		*header++ = 127 | (f->is_masked ? 0x80 : 0);
		
		*(unsigned int *)header = 0;
		header += 4;
		
		*header++ = (size >> 24) & 0xff;
		*header++ = (size >> 16) & 0xff;
		*header++ = (size >> 8) & 0xff;
		*header++ = size & 0xff;
		f->header_size = 10;
	}
	
	if (f->is_masked) 
	{
		AG_OS_MEMCPY(header, f->mask, 4);
		f->header_size += 4;
	}
}

void litews_frame_create_header(_litews_frame * f, unsigned char * header, const size_t data_size) 
{
	const unsigned int size = (unsigned int)data_size;
	
	*header++ = 0x80 | f->opcode;
	
	if (size < 126) 
	{
		*header++ = (size & 0xff) | (f->is_masked ? 0x80 : 0);
		f->header_size = 2;
	} 
	else if (size < 65536) 
	{
		*header++ = 126 | (f->is_masked ? 0x80 : 0);
		*header++ = (size >> 8) & 0xff;
		*header++ = size & 0xff;
		f->header_size = 4;
	} 
	else 
	{
		*header++ = 127 | (f->is_masked ? 0x80 : 0);
		
		*(unsigned int *)header = 0;
		header += 4;
		
		*header++ = (size >> 24) & 0xff;
		*header++ = (size >> 16) & 0xff;
		*header++ = (size >> 8) & 0xff;
		*header++ = size & 0xff;
		f->header_size = 10;
	}
	
	if (f->is_masked) 
	{
		AG_OS_MEMCPY(header, f->mask, 4);
		f->header_size += 4;
	}
}

void litews_frame_fill_with_send_bin_data(_litews_frame * f, const void * data, const size_t data_size, int flag) 
{
	unsigned char header[16];
	unsigned char * frame = NULL;
	unsigned char mask[4];
	size_t index = 0;

	litews_frame_create_bin_header(f, header, data_size, flag);
	
	LOGV_LITEWS("====bin frame header====");
    LOGV_LITEWS("%d \n", header[0]);
    //check bin header data
    //  -----------------------------------------------
    //   F  |   |   |   |   |   |   |   |
    //   I  | 0 | 0 | 0 |    OPCODE     |
    //   N  |   |   |   |   |   |   |   |   

	f->data_size = data_size + f->header_size;
	f->data = litews_malloc(f->data_size);
	frame = (unsigned char *)f->data;
	AG_OS_MEMCPY(frame, header, f->header_size);
	
	if (data) 
	{ // have data to send
		frame += f->header_size;
		AG_OS_MEMCPY(frame, data, data_size);
		
		if (f->is_masked) 
		{
			AG_OS_MEMCPY(mask, &f->mask, 4);
			for (index = 0; index < data_size; index++) 
			{
				*frame = *frame ^ mask[index & 0x3];
				frame++;
			}
		}
	}
	if(flag == litews_frame_end)
	{
    	f->is_finished = litews_true;
	}
	else
	{
	    f->is_finished = litews_false;
	}
}


void litews_frame_fill_with_send_data(_litews_frame * f, const void * data, const size_t data_size) 
{
	unsigned char header[16];
	unsigned char * frame = NULL;
	unsigned char mask[4];
	size_t index = 0;
	
	litews_frame_create_header(f, header, data_size);

	LOGV_LITEWS("====text frame header====");
    LOGV_LITEWS("%d \n", header[0]);
	
	f->data_size = data_size + f->header_size;
	f->data = litews_malloc(f->data_size);
	frame = (unsigned char *)f->data;
	AG_OS_MEMCPY(frame, header, f->header_size);
	
	if (data) 
	{ // have data to send
		frame += f->header_size;
		AG_OS_MEMCPY(frame, data, data_size);
		
		if (f->is_masked) 
		{
			AG_OS_MEMCPY(mask, &f->mask, 4);
			for (index = 0; index < data_size; index++) 
			{
				*frame = *frame ^ mask[index & 0x3];
				frame++;
			}
		}
	}
	f->is_finished = litews_true;
}

void litews_frame_combine_datas(_litews_frame * to, _litews_frame * from) 
{
	unsigned char * comb_data = (unsigned char *)litews_malloc(to->data_size + from->data_size);
	if (comb_data) 
	{
		if (to->data && to->data_size) 
		{
			AG_OS_MEMCPY(comb_data, to->data, to->data_size);
		}
		comb_data += to->data_size;
		if (from->data && from->data_size) 
		{
			AG_OS_MEMCPY(comb_data, from->data, from->data_size);
		}
	}
	litews_free(to->data);
	to->data = comb_data;
	to->data_size += from->data_size;
}

_litews_frame * litews_frame_create(void) 
{
	_litews_frame * f = (_litews_frame *)litews_malloc_zero(sizeof(_litews_frame));
	union {
		unsigned int ui;
		unsigned char b[4];
	} mask_union;
	//assert(sizeof(unsigned int) == 4);
	//	mask_union.ui = 2018915346;
	mask_union.ui = (rand() / (RAND_MAX / 2) + 1) * rand();
	AG_OS_MEMCPY(f->mask, mask_union.b, 4);
	return f;
}

void litews_frame_delete(_litews_frame * f) 
{
	if (f) 
	{
		litews_free(f->data);
		litews_free(f);
	}
}

void litews_frame_delete_clean(_litews_frame ** f) 
{
    if (f) 
    {
        litews_frame_delete(*f);
        *f = NULL;
    }
}

size_t litews_check_recv_frame_size(const void * data, const size_t data_size) 
{
	if (data && data_size >= 2) 
	{
		const unsigned char * udata = (const unsigned char *)data;
		//        const unsigned int is_finshd = (udata[0] >> 7) & 0x01;
		const unsigned int is_masked = (udata[1] >> 7) & 0x01;
		const unsigned int payload = udata[1] & 0x7f;
		unsigned int header_size = is_masked ? 6 : 2;
		
		unsigned int expected_size = 0;
		
		switch (payload) 
		{
			case 126: header_size += 2; break;
			case 127: header_size += 8; break;
			default: break;
		}
		
		if (data_size < header_size) 
		{
			return 0;
		}
		
		switch (payload) 
		{
			case 126:
				expected_size |= ((unsigned int)udata[2]) << 8;
				expected_size |= (unsigned int)udata[3];
				break;
				
			case 127:
				expected_size |= ((unsigned int)udata[6]) << 24;
				expected_size |= ((unsigned int)udata[7]) << 16;
				expected_size |= ((unsigned int)udata[8]) << 8;
				expected_size |= (unsigned int)udata[9];
				break;
				
			default:
				if (payload <= 125) 
				{
					expected_size = payload;
				}
				break;
		}
		
		const unsigned int nPackSize = expected_size + header_size;
		return (nPackSize <= data_size) ? nPackSize : 0;
	}
	return 0;
}


