
#ifndef __LITEWS_FRAME_H__
#define __LITEWS_FRAME_H__ 1

#include "litewebsocket.h"
#include "aligenie_os.h"

typedef enum _litews_opcode {
	litews_opcode_continuation = 0x0, // %x0 denotes a continuation frame
	litews_opcode_text_frame = 0x1, // %x1 denotes a text frame
	litews_opcode_binary_frame = 0x2, // %x2 denotes a binary frame
	litews_opcode_connection_close = 0x8, // %x8 denotes a connection close
	litews_opcode_ping = 0x9, // %x9 denotes a ping
	litews_opcode_pong = 0xA // %xA denotes a pong
} litews_opcode;

typedef struct _litews_frame_struct 
{
	void * data;
	size_t data_size;
	litews_opcode opcode;
	unsigned char mask[4];
	litews_bool is_masked;
	litews_bool is_finished;
	unsigned char header_size;
} _litews_frame;

size_t litews_check_recv_frame_size(const void * data, const size_t data_size);

_litews_frame * litews_frame_create_with_recv_data(const void * data, const size_t data_size);

// data - should be null, and setted by newly created. 'data' & 'data_size' can be null
void litews_frame_fill_with_send_data(_litews_frame * f, const void * data, const size_t data_size);

void litews_frame_fill_with_send_bin_data(_litews_frame * f, const void * data, const size_t data_size, int flag) ;

// combine datas of 2 frames. combined is 'to'
void litews_frame_combine_datas(_litews_frame * to, _litews_frame * from);

_litews_frame * litews_frame_create(void);

void litews_frame_delete(_litews_frame * f);

void litews_frame_delete_clean(_litews_frame ** f);

#endif
