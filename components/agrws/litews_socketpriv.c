
#include "litewebsocket.h"
#include "litews_socket.h"
#include "litews_memory.h"
#include "litews_string.h"
#include "litews_log.h"

#define LITEWS_CONNECT_RETRY_DELAY 200
#define LITEWS_CONNECT_ATTEMPS 5

#ifndef  LITEWS_OS_WINDOWS 
#define  WSAEWOULDBLOCK  EAGAIN
#define  WSAEINPROGRESS     EINPROGRESS
#endif

#ifdef SUPPORT_WOLFSSL
static WOLFSSL_CTX* xWolfSSL_Context = NULL;
static WOLFSSL* xWolfSSL_Object = NULL;
static size_t WS_PING_LOOP = 0; //ping loop 20S no data send, send ping
#elif defined(SUPPORT_MBEDTLS)
static size_t WS_PING_LOOP = 0; //ping loop 20S no data send, send ping
#else
#endif

static const char * k_litews_socket_min_http_ver = "1.1";
static const char * k_litews_socket_sec_websocket_accept = "sec-websocket-accept";

unsigned int litews_socket_get_next_message_id(litews_socket s) 
{
	const unsigned int mess_id = ++s->next_message_id;
	if (mess_id > 9999999) 
	{
		s->next_message_id = 0;
	}
	return mess_id;
}

void litews_socket_send_ping(litews_socket s) 
{
    char buff[16];
    size_t len = 0;
    _litews_frame * frame = litews_frame_create();

    len = litews_sprintf(buff, 16, "%u", litews_socket_get_next_message_id(s));
    LOGD_LITEWS("%s, buff: %s", __FUNCTION__, buff);

    frame->is_masked = litews_true;
    frame->opcode = litews_opcode_ping;
    litews_frame_fill_with_send_data(frame, buff, len);
    litews_socket_append_send_frames(s, frame);
}

#ifdef SUPPORT_REDUCE_MEM
void litews_socket_inform_recvd_frames(litews_socket s) 
{
    litews_bool is_all_finished = litews_true;
    _litews_frame * frame = NULL;
    _litews_node * cur = s->recvd_frames;

    while (cur) 
    {
        frame = (_litews_frame *)cur->value.object;

        if (frame) 
        {
            //if (frame->is_finished) //bin file no need frame finished
            {
                switch (frame->opcode) 
                {
                    case litews_opcode_text_frame:
                    {
                        if(frame->is_finished)  //text data need finished and send
                        {
                            if (s->on_recvd_text) 
                            {
                            s->on_recvd_text(s, (const char *)frame->data, (unsigned int)frame->data_size);
#ifdef SUPPORT_REDUCE_MEM
                            //clear buffer
                            s->buffer_size = SSL_REC_BUFFER_SIZE;
#endif
                            }
                        }
                    }
                    break;
                    
                    case litews_opcode_binary_frame:
                    {
                        if (s->on_recvd_bin) 
                        {
                            LOGD_LITEWS("received bin frame start , lens = %d", frame->data_size);
                            s->on_recvd_bin(s, frame->data, (unsigned int)frame->data_size, litews_frame_start);
                            s->buffer_size = SSL_REC_BUFFER_SIZE;
                        }
                    }
                    break;
                    
                    case litews_opcode_continuation:
                    {
                        if(s->on_recvd_bin)
                        {
                            if(frame->data_size == 0)
                            {
                                LOGD_LITEWS("received bin frame end, lens = %d", frame->data_size);
                                s->on_recvd_bin(s, frame->data, (unsigned int)frame->data_size, litews_frame_end);
                            }
                            else
                            {
                                LOGD_LITEWS("received bin frame continue, lens = %d", frame->data_size);
                                s->on_recvd_bin(s, frame->data, (unsigned int)frame->data_size, litews_frame_continue);
                            }
                            s->buffer_size = SSL_REC_BUFFER_SIZE;
                        }
                    }
                    break;
                    
                    default: 
                        break;
                    }

                litews_frame_delete(frame);
                cur->value.object = NULL;
            }
        }
        cur = cur->next;
    }

    if (is_all_finished) 
    {
        litews_list_delete_clean(&s->recvd_frames);
    }
}

#else
void litews_socket_inform_recvd_frames(litews_socket s) 
{
    litews_bool is_all_finished = litews_true;
    _litews_frame * frame = NULL;
    _litews_node * cur = s->recvd_frames;
    while (cur) 
    {
        frame = (_litews_frame *)cur->value.object;

        if (frame) 
        {
            if (frame->is_finished) 
            {
                switch (frame->opcode) 
                {
                    case litews_opcode_text_frame:
                        if (s->on_recvd_text) 
                        {
                            s->on_recvd_text(s, (const char *)frame->data, (unsigned int)frame->data_size);
                        }
                        break;
                    
                    case litews_opcode_binary_frame:
                        if (s->on_recvd_bin) 
                        {
                            s->on_recvd_bin(s, frame->data, (unsigned int)frame->data_size);
                        }
                        break;
                    
                    default: 
                        break;
                }
                litews_frame_delete(frame);
                cur->value.object = NULL;
            } 
            else 
            {
                is_all_finished = litews_false;
            }
        }
        cur = cur->next;
    }
    
    if (is_all_finished) 
    {
        litews_list_delete_clean(&s->recvd_frames);
    }
}
#endif

void litews_socket_read_handshake_responce_value(const char * str, char ** value) 
{
	const char * s = NULL;
	size_t len = 0;

	while (*str == ':' || *str == ' ') 
	{
		str++;
	}
	s = str;
	while (*s != '\r' && *s != '\n') 
	{
		s++;
		len++;
	}
	if (len > 0) 
	{
		*value = litews_string_copy_len(str, len);
	}
}

litews_bool litews_socket_process_handshake_responce(litews_socket s) 
{
#ifdef SUPPORT_REDUCE_MEM
    const char * str = s->received_buffer;
#else
    const char * str = (const char *)s->received;
#endif
    const char * sub = NULL;
    float http_ver = -1;
    int http_code = -1;

    litews_error_delete_clean(&s->error);
    sub = strstr(str, "HTTP/");
    if (!sub) 
    {
        LOGE_LITEWS("sub error return litews_false");
        return litews_false;
    }

    sub += 5;
    if (litews_sscanf(sub, "%f %i", &http_ver, &http_code) != 2) 
    {
        LOGE_LITEWS("sscanf error, return false");
        http_ver = -1;
        http_code = -1;
    }

    sub = strstr(str, k_litews_socket_sec_websocket_accept); // "Sec-WebSocket-Accept" //?? sec-websocket-accept

    if (sub) 
    {
        sub += strlen(k_litews_socket_sec_websocket_accept);
        litews_socket_read_handshake_responce_value(sub, &s->sec_ws_accept);
    }

    if (http_code != 101 || !s->sec_ws_accept) 
    {
        s->error = litews_error_new_code_descr(litews_error_code_parse_handshake,
                                                (http_code != 101) ? "HTPP code not found or non 101" : "Accept key not found");
        return litews_false;
    }

    return litews_true;
}

// need close socket on error
#ifdef SUPPORT_REDUCE_MEM
litews_bool litews_socket_send(litews_socket s, const void * data, const size_t data_size) 
{
    int sended = -3, error_number = -1;
    litews_error_delete_clean(&s->error);

#if defined(LITEWS_OS_WINDOWS)
    sended = send(s->socket, (const char *)data, data_size, 0);
    error_number = WSAGetLastError();
#else

#ifdef SUPPORT_MBEDTLS
    sended = mbedtls_ssl_write(&s->ssl->ssl_ctx, (const unsigned char *)data, (int)data_size);
#elif defined(SUPPORT_WOLFSSL)
    sended = wolfSSL_write(xWolfSSL_Object, data, (int)data_size);
#else
    sended = (int)send(s->socket, data, (int)data_size, 0);
#endif

#endif

    LOGD_LITEWS("WebSocket send wanted [%d], finished [%d]", data_size, sended);

    if (sended > 0) 
    {
        return litews_true;
    }
    else
    {
        litews_socket_check_write_error(s, error_number);
        if (s->error) 
        {
            LOGE_LITEWS("socket error : errorcode[%d], description[%s]", s->error->code, s->error->description);
            litews_socket_close(s);
            return litews_false;
        }
        LOGE_LITEWS("socket error appended");
        return litews_false;
    }
    //#endif

    return litews_true;
}
#else
litews_bool litews_socket_send(litews_socket s, const void * data, const size_t data_size) 
{
    int sended = -1, error_number = -1;
    litews_error_delete_clean(&s->error);

    //errno = -1;
#if defined(LITEWS_OS_WINDOWS)
    sended = send(s->socket, (const char *)data, data_size, 0);
    error_number = WSAGetLastError();
#else
    sended = (int)send(s->socket, data, (int)data_size, 0);
    //error_number = errno;
#endif

    if (sended > 0) 
    {
        return litews_true;
    }

    litews_socket_check_write_error(s, error_number);
    if (s->error) 
    {
        litews_socket_close(s);
        return litews_false;
    }
    return litews_true;
}
#endif


#ifdef SUPPORT_REDUCE_MEM
litews_bool litews_socket_recv(litews_socket s) 
{
    int is_reading = 1;
    int error_number = 1; 
    int len = -1;
    //char * received = NULL;
    unsigned char buff[SSL_WEBSOCKET_RECV_BUF_LEN];

    litews_error_delete_clean(&s->error);

    s->buffer_len = 0;

    while ((is_reading) && (((s->buffer_size) > SSL_WEBSOCKET_RECV_BUF_LEN)))
    {
#ifdef SUPPORT_WOLFSSL
        len = wolfSSL_read(xWolfSSL_Object, buff, SSL_WEBSOCKET_RECV_BUF_LEN);    //once recieve 2048
        #elif defined(SUPPORT_MBEDTLS)
        len = mbedtls_ssl_read(&(s->ssl->ssl_ctx), (unsigned char *)buff, SSL_REC_ONCE_SIZE);
#endif
        //LOGV_LITEWS("buffer_size = %d, buffer_len = %d, len = %d", s->buffer_size, s->buffer_len, len);

        if (len > 0)    //received data, put to buffer
        {
            AG_OS_MEMCPY((s->received_buffer)+(s->buffer_len), buff, len);
            s->buffer_len += len;
            s->buffer_size = SSL_REC_BUFFER_SIZE - s->buffer_len;
        }
        else
        {
            is_reading = 0;
        }
        litews_thread_sleep(100);
    }

    if (error_number == 0)  //socket abnormal
    {
        s->error = litews_error_new_code_descr(litews_error_code_read_write_socket, "Failed read/write socket");
        litews_socket_close(s);
        LOGE_LITEWS("FAILED read/write socket");
        return litews_false;
    }

    return litews_true;
}
#else
litews_bool litews_socket_recv(litews_socket s) 
{
	int is_reading = 1, error_number = -1, len = -1;
	char * received = NULL;
	size_t total_len = 0;
	char buff[8192];
	
	litews_error_delete_clean(&s->error);
	
	while (is_reading) 
	{
		len = (int)recv(s->socket, buff, 8192, 0);
#if defined(LITEWS_OS_WINDOWS)
		error_number = WSAGetLastError();
#else
		//error_number = errno;
#endif
		if (len > 0) 
		{
			total_len += len;
			if (s->received_size - s->received_len < len) 
			{
				litews_socket_resize_received(s, s->received_size + len);
			}
			received = (char *)s->received;
			if (s->received_len) 
			{
				received += s->received_len;
			}
			AG_OS_MEMCPY(received, buff, len);
			s->received_len += len;
		} 
		else 
		{
			is_reading = 0;
		}
	}
	//if (error_number < 0) return litews_true;
	if (error_number != WSAEWOULDBLOCK && error_number != WSAEINPROGRESS) 
	{
		s->error = litews_error_new_code_descr(litews_error_code_read_write_socket, "Failed read/write socket");
		litews_socket_close(s);
		return litews_false;
	}
	return litews_true;
}
#endif

_litews_frame * litews_socket_last_unfin_recvd_frame_by_opcode(litews_socket s, const litews_opcode opcode) 
{
	_litews_frame * last = NULL;
	_litews_frame * frame = NULL;
	_litews_node * cur = s->recvd_frames;
	
	while (cur) 
	{
		frame = (_litews_frame *)cur->value.object;
		
		if (frame) 
		{
            //  [FIN=0,opcode !=0 ],[FIN=0,opcode ==0 ],....[FIN=1,opcode ==0 ]
			if (!frame->is_finished /*&& frame->opcode == opcode*/) 
			{
				last = frame;
			}
		}
		cur = cur->next;
	}
	return last;
}

void litews_socket_process_bin_or_text_frame(litews_socket s, _litews_frame * frame) 
{
	_litews_frame * last_unfin = litews_socket_last_unfin_recvd_frame_by_opcode(s, frame->opcode);
	
	if (last_unfin) 
	{
	    LOGV_LITEWS("last unfin status, frame data size = %d", frame->data_size);
		litews_frame_combine_datas(last_unfin, frame);
		last_unfin->is_finished = frame->is_finished;
		litews_frame_delete(frame);
	} 
	else if (frame->data && frame->data_size) 
	{
	    LOGV_LITEWS("frame data status, frame data size = %d", frame->data_size);
		litews_socket_append_recvd_frames(s, frame);
	} 
	else 
	{
	    LOGV_LITEWS("End frame, frame data size = %d", frame->data_size);
	    #ifdef SUPPORT_REDUCE_MEM
	    //last bin frame
	    litews_socket_append_recvd_frames(s, frame);
	    #else
		litews_frame_delete(frame);
		#endif
	}
}

void litews_socket_process_ping_frame(litews_socket s, _litews_frame * frame) 
{
	_litews_frame * pong_frame = litews_frame_create();
	pong_frame->opcode = litews_opcode_pong;
	pong_frame->is_masked = litews_true;
	litews_frame_fill_with_send_data(pong_frame, frame->data, frame->data_size);
	litews_frame_delete(frame);
	litews_socket_append_send_frames(s, pong_frame);
}

void litews_socket_process_conn_close_frame(litews_socket s, _litews_frame * frame) 
{
	s->command = COMMAND_INFORM_DISCONNECTED;
	s->error = litews_error_new_code_descr(litews_error_code_connection_closed, "Connection was closed by endpoint");
	//litews_socket_close(s);
	litews_frame_delete(frame);
}

void litews_socket_process_received_frame(litews_socket s, _litews_frame * frame) 
{
	switch (frame->opcode) 
	{
		case litews_opcode_ping: 
		    {
		        LOGD_LITEWS("Received Ping frame");
    		    litews_socket_process_ping_frame(s, frame); 
		    }
		    break;

		case litews_opcode_pong:
		    {
		        LOGD_LITEWS("Received Pong frame \r\n");
		        litews_frame_delete(frame);
		        //litews_socket_process_ping_frame(s, frame);
		    }
		    break;
		case litews_opcode_text_frame:
    		{
		        LOGV_LITEWS("text frame");
		        litews_socket_process_bin_or_text_frame(s, frame);
		    }
		    break;
		case litews_opcode_binary_frame:
		    {
		        LOGV_LITEWS("bin frame");
		        litews_socket_process_bin_or_text_frame(s, frame);
		    }
		    break;
		case litews_opcode_continuation:
		    {
		        LOGV_LITEWS("continuation frame");
    			litews_socket_process_bin_or_text_frame(s, frame);
    		}
			break;
			
		case litews_opcode_connection_close:
		    {
		        LOGD_LITEWS("connect close frame");
        		litews_socket_process_conn_close_frame(s, frame); 
        	}
        	break;

		default:
		    #ifdef SUPPORT_REDUCE_MEM
			// unprocessed => delete
			LOGV_LITEWS("wait frame data");    //wait data
			#else
			litews_frame_delete(frame);
			#endif
			break;
	}
}

#ifdef SUPPORT_REDUCE_MEM
int litews_socket_idle_recv(litews_socket s) 
{
	_litews_frame * frame = NULL;
	//int is_reading = 1;
	//int error_number = 1; 
	int len = -1;
	//char * received = NULL;
	unsigned char buff[SSL_REC_ONCE_SIZE];
	
	litews_error_delete_clean(&s->error);
    
	if((SSL_REC_BUFFER_SIZE - (s->buffer_len)) > SSL_REC_ONCE_SIZE)
	{
#ifdef SUPPORT_WOLFSSL
        len = wolfSSL_read(xWolfSSL_Object, buff, SSL_REC_ONCE_SIZE);    //once recieve
#elif defined(SUPPORT_MBEDTLS)
        len = mbedtls_ssl_read(&(s->ssl->ssl_ctx), (unsigned char *)buff, SSL_REC_ONCE_SIZE);
#endif
        //LOGV_LITEWS("buffer_size = %d, buffer_len = %d, len = %d", s->buffer_size, s->buffer_len, len);
    }

    if (len > 0)   //received data, put to buffer
    {
        AG_OS_MEMCPY((s->received_buffer)+(s->buffer_len), buff, len);
        s->buffer_len += len;
        s->buffer_size = SSL_REC_BUFFER_SIZE - s->buffer_len;
    }
    /*
    else
    {
    is_reading = 0;
    }
    */
    //litews_thread_sleep(100);

    const size_t nframe_size = litews_check_recv_frame_size(s->received_buffer, s->buffer_len); //check data

   /*   check received bin frame
   if(nframe_size == 2)
   {
       LOGN_LITEWS("nframe size = %d", nframe_size);
   }
   */
   
   if (nframe_size) 
   {
       frame = litews_frame_create_with_recv_data(s->received_buffer, nframe_size);
       //LOGV_LITEWS("frame = %d", frame);
       if (frame)  
       {
           litews_socket_process_received_frame(s, frame);
       }

       if (nframe_size == s->buffer_len) 
       {
           s->buffer_len = 0;
       } 
       else if (s->buffer_len > nframe_size) 
       {
           const size_t nLeftLen = s->buffer_len - nframe_size;
           memmove(s->received_buffer, s->received_buffer + nframe_size, nLeftLen);
           s->buffer_len = nLeftLen;
       }
   }
   return len;
}
#else
int litews_socket_idle_recv(litews_socket s) 
{
	_litews_frame * frame = NULL;

	if (!litews_socket_recv(s)) 
	{
		// sock already closed
		if (s->error) 
		{
			s->command = COMMAND_INFORM_DISCONNECTED;
		}
		return;
	}

   const size_t nframe_size = litews_check_recv_frame_size(s->received, s->received_len);

   if (nframe_size) 
   {
       frame = litews_frame_create_with_recv_data(s->received, nframe_size);
       if (frame)  
       {
           litews_socket_process_received_frame(s, frame);
       }
	   
       if (nframe_size == s->received_len) 
       {
           s->received_len = 0;
       } 
       else if (s->received_len > nframe_size) 
       {
           const size_t nLeftLen = s->received_len - nframe_size;
           memmove((char*)s->received, (char*)s->received + nframe_size, nLeftLen);
           s->received_len = nLeftLen;
       }
   }
   return 0;
}
#endif

litews_bool litews_socket_idle_send(litews_socket s) 
{
    _litews_node * cur = NULL;
    litews_bool sending = litews_true;
    litews_bool ret = litews_false;
    _litews_frame * frame = NULL;

    litews_mutex_lock(s->send_mutex);
    cur = s->send_frames;

    if (cur) 
    {
        while (cur && s->is_connected && sending) 
        {
            frame = (_litews_frame *)cur->value.object;
            cur->value.object = NULL;
            if (frame) 
            {
                WS_PING_LOOP = 0;
                sending = litews_socket_send(s, frame->data, frame->data_size);
            //printf("frame->data_size = %d\n", frame->data_size);
            }
            litews_frame_delete(frame);
            cur = cur->next;
        }
        litews_list_delete_clean(&s->send_frames);
        if ((s->error)||(sending == litews_false) ) 
        {
            s->command = COMMAND_INFORM_DISCONNECTED;
        }
        ret = litews_true;
    }
    litews_mutex_unlock(s->send_mutex);
    return ret;
}

#if defined(SUPPORT_MBEDTLS) || defined(SUPPORT_WOLFSSL)
void ssl_socket_wait_handshake_responce(litews_socket s) 
{
    LOGV_LITEWS("wait hand shake responce!!!");
    
    #ifdef SUPPORT_REDUCE_MEM
    s->buffer_size = SSL_REC_BUFFER_SIZE;   //init buffer size
    #endif
    
	if (!litews_socket_recv(s)) 
	{
		// sock already closed
		if (s->error) 
		{
			s->command = COMMAND_INFORM_DISCONNECTED;
			LOGE_LITEWS("socket receive error, need disconnect socket");
		}
		return;
	}

	if (s->buffer_len == 0) 
	{
		return;
	}

	if (litews_socket_process_handshake_responce(s)) 
	{
	    #ifdef SUPPORT_REDUCE_MEM
	    #else
        s->received_len = 0;
        #endif
		s->is_connected = litews_true;
		s->command = COMMAND_INFORM_CONNECTED;
		LOGD_LITEWS("handshake OK!");
		
		#ifdef SUPPORT_REDUCE_MEM
        s->buffer_len = 0;
        s->buffer_size = SSL_REC_BUFFER_SIZE;
		#endif
	} 
	else 
	{
		litews_socket_close(s);
		s->command = COMMAND_INFORM_DISCONNECTED;
		LOGE_LITEWS("handshake NG!");
	}
}

#else
void litews_socket_wait_handshake_responce(litews_socket s) {
	if (!litews_socket_recv(s)) {
		// sock already closed
		if (s->error) {
			s->command = COMMAND_INFORM_DISCONNECTED;
		}
		return;
	}
	
	if (s->received_len == 0) {
		return;
	}

	if (litews_socket_process_handshake_responce(s)) {
        s->received_len = 0;
		s->is_connected = litews_true;
		s->command = COMMAND_INFORM_CONNECTED;
	} else {
		litews_socket_close(s);
		s->command = COMMAND_INFORM_DISCONNECTED;
	}
}
#endif

void litews_socket_send_disconnect(litews_socket s) 
{
	char buff[16];
	size_t len = 0;
	_litews_frame * frame = litews_frame_create();

	len = litews_sprintf(buff, 16, "%u", litews_socket_get_next_message_id(s));

	frame->is_masked = litews_true;
	frame->opcode = litews_opcode_connection_close;
	litews_frame_fill_with_send_data(frame, buff, len);
	litews_socket_send(s, frame->data, frame->data_size);
	litews_frame_delete(frame);
	
	s->command = COMMAND_END;
	litews_thread_sleep(LITEWS_CONNECT_RETRY_DELAY); // little bit wait after send message
}

#if defined(SUPPORT_WOLFSSL) || defined(SUPPORT_MBEDTLS) 
void ssl_socket_send_handshake(litews_socket s) 
{
    {
        char buff[SSL_WEBSOCKET_SEND_BUF_LEN];
        char * ptr = buff;
        size_t writed = 0;
        writed = litews_sprintf(ptr, SSL_WEBSOCKET_SEND_BUF_LEN, "GET %s HTTP/%s\r\n", s->path, k_litews_socket_min_http_ver);
    
        if (s->port == 80) 
        {
            writed += litews_sprintf(ptr + writed, SSL_WEBSOCKET_SEND_BUF_LEN - writed, "Host: %s\r\n", s->host);
        } 
        else
        {
            writed += litews_sprintf(ptr + writed, SSL_WEBSOCKET_SEND_BUF_LEN - writed, "Host: %s:%i\r\n", s->host, s->port);
        }
    
        writed += litews_sprintf(ptr + writed, SSL_WEBSOCKET_SEND_BUF_LEN - writed,
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Origin: %s://%s\r\n",
                              s->scheme, s->host);
    
        writed += litews_sprintf(ptr + writed, SSL_WEBSOCKET_SEND_BUF_LEN - writed,
                              "Sec-WebSocket-Key: %s\r\n"
                              "Sec-WebSocket-Protocol: chat, superchat\r\n"
                              "Sec-WebSocket-Version: 13\r\n"
                              "\r\n",
                              "dGhlIHNhbXBsZSBub25jZQ==");
    
        if (litews_socket_send(s, buff, writed)) 
        {
            s->command = COMMAND_WAIT_HANDSHAKE_RESPONCE;
        } 
        else 
        {
            if (s->error) 
            {
                s->error->code = litews_error_code_send_handshake;
            } 
            else 
            {
                s->error = litews_error_new_code_descr(litews_error_code_send_handshake, "Send handshake");
            }
            LOGE_LITEWS("send websocket handshake error, need close socket");
            litews_socket_close(s);
            s->command = COMMAND_INFORM_DISCONNECTED;
        }
    }
}

#else
void litews_socket_send_handshake(litews_socket s) 
{
	char buff[512];
	char * ptr = buff;
	size_t writed = 0;
	writed = litews_sprintf(ptr, 512, "GET %s HTTP/%s\r\n", s->path, k_litews_socket_min_http_ver);

	if (s->port == 80) 
	{
		writed += litews_sprintf(ptr + writed, 512 - writed, "Host: %s\r\n", s->host);
	} 
	else 
	{
		writed += litews_sprintf(ptr + writed, 512 - writed, "Host: %s:%i\r\n", s->host, s->port);
	}

	writed += litews_sprintf(ptr + writed, 512 - writed,
						  "Upgrade: websocket\r\n"
						  "Connection: Upgrade\r\n"
						  "Origin: %s://%s\r\n",
						  s->scheme, s->host);

	writed += litews_sprintf(ptr + writed, 512 - writed,
						  "Sec-WebSocket-Key: %s\r\n"
						  "Sec-WebSocket-Protocol: chat, superchat\r\n"
						  "Sec-WebSocket-Version: 13\r\n"
						  "\r\n",
						  "dGhlIHNhbXBsZSBub25jZQ==");

	if (litews_socket_send(s, buff, writed)) 
	{
		s->command = COMMAND_WAIT_HANDSHAKE_RESPONCE;
	} 
	else 
	{
		if (s->error) 
		{
			s->error->code = litews_error_code_send_handshake;
		} 
		else 
		{
			s->error = litews_error_new_code_descr(litews_error_code_send_handshake, "Send handshake");
		}
		litews_socket_close(s);
		s->command = COMMAND_INFORM_DISCONNECTED;
	}
}
#endif

struct addrinfo * litews_socket_connect_getaddr_info(litews_socket s) {
	struct addrinfo hints;
	char portstr[16];
	struct addrinfo * result = NULL;
	int ret = 0, retry_number = 0;
#if defined(LITEWS_OS_RTOS)
#else
	int last_ret = 0;
#endif

#if defined(LITEWS_OS_WINDOWS)
	WSADATA wsa;
#endif

	litews_error_delete_clean(&s->error);

#if defined(LITEWS_OS_WINDOWS)
	AG_OS_MEMSET(&wsa, 0, sizeof(WSADATA));
	if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
		s->error = litews_error_new_code_descr(litews_error_code_connect_to_host, "Failed initialise winsock");
		s->command = COMMAND_INFORM_DISCONNECTED;
		return NULL;
	}
#endif

	litews_sprintf(portstr, 16, "%i", s->port);
	while (++retry_number < LITEWS_CONNECT_ATTEMPS) {
		result = NULL;
		AG_OS_MEMSET(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		ret = getaddrinfo(s->host, portstr, &hints, &result);
		if (ret == 0 && result) {
			return result;
		}
		
#if defined(LITEWS_OS_RTOS)
#else
		if (ret != 0) 
		{
			last_ret = ret;
		}
#endif
		if (result) {
			freeaddrinfo(result);
		}
		
		litews_thread_sleep(LITEWS_CONNECT_RETRY_DELAY);
	}

#if defined(LITEWS_OS_WINDOWS)
	WSACleanup();
#endif

#if defined(LITEWS_OS_RTOS)
	s->error = litews_error_new_code_descr(litews_error_code_connect_to_host, "Failed connect to host");
#else
	s->error = litews_error_new_code_descr(litews_error_code_connect_to_host,
										(last_ret > 0) ? gai_strerror(last_ret) : "Failed connect to host");
#endif										
	s->command = COMMAND_INFORM_DISCONNECTED;
	return NULL;
}


#ifdef SUPPORT_WOLFSSL
void wolfssl_socket_connect_to_host(litews_socket s)
{
	struct addrinfo * result = NULL;
	struct addrinfo * p = NULL;
	litews_socket_t sock = LITEWS_INVALID_SOCKET;
	int retry_number = 0;
	int ret = 0 ;

	result = litews_socket_connect_getaddr_info(s);

	if (!result) 
	{
	    LOGE_LITEWS("litews socket connect get address error!!");
		return;
	}

	while ((++retry_number < LITEWS_CONNECT_ATTEMPS) && (sock == LITEWS_INVALID_SOCKET)) 
	{
		for (p = result; p != NULL; p = p->ai_next) 
		{
			sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (sock != LITEWS_INVALID_SOCKET) 
			{
				litews_socket_set_option(sock, SO_ERROR, 1); // 
				litews_socket_set_option(sock, SO_KEEPALIVE, 1); //

				if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) 
				{
				    #ifdef SUPPORT_REDUCE_MEM
				    #else
                    s->received_len = 0;
                    #endif
                    
					s->socket = sock;
					fcntl(s->socket, F_SETFL, O_NONBLOCK);

                    ret = wolfSSL_Init(); //init wolfssl
                    if(ret != WOLFSSL_SUCCESS)
                    {
                        LOGE_LITEWS("wolfSSL_Init failed!");
                        goto failed1;
                    }
                    xWolfSSL_Context = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
                    if(xWolfSSL_Context != NULL)
                    {   
                        #if 1
                        LOGV_LITEWS("cert lens = %s", (s->client_cert));
                        ret = wolfSSL_CTX_load_verify_buffer(xWolfSSL_Context, (unsigned char*)s->client_cert, strlen(s->client_cert), SSL_FILETYPE_PEM);
                        if (WOLFSSL_SUCCESS != ret) 
                        {
                			LOGE_LITEWS("Loading the CA root certificate failed...");
                			goto failed2;
                		}
                        wolfSSL_CTX_set_verify(xWolfSSL_Context, WOLFSSL_VERIFY_PEER, NULL);
                        #else
                        wolfSSL_CTX_set_verify(xWolfSSL_Context, WOLFSSL_VERIFY_NONE, NULL);
                        #endif
                    }
                    else
                    {
                        LOGE_LITEWS("wolfssl context creat failed!");
                        goto failed2;
                    }
                    xWolfSSL_Object = wolfSSL_new(xWolfSSL_Context);

                    if(xWolfSSL_Object != NULL)
                    {
                        wolfSSL_set_fd(xWolfSSL_Object, sock);
                        ret = 0;
                        while((ret != WOLFSSL_SUCCESS))
                        {
                            ret = wolfSSL_connect(xWolfSSL_Object);
                            litews_thread_sleep(100);
                        }
                    }
                    else
                    {
                        LOGE_LITEWS("wolfssl object creat failed");
                        goto failed3;
                    }
                    
					LOGD_LITEWS("SOCKET CONNECT");
					break;

					//wolfssl failed
            		failed3: wolfSSL_free(xWolfSSL_Object);
            		failed2: wolfSSL_CTX_free(xWolfSSL_Context);
            		failed1: wolfSSL_Cleanup();

            		close(sock);
                    s->socket = LITEWS_INVALID_SOCKET;
					//
				}
				else
				{
				    //socket connect fail close
				    LOGE_LITEWS("socket connect failed, close socket");
				    close(sock);
				}
			}
		}
		if (sock == LITEWS_INVALID_SOCKET) 
		{
			litews_thread_sleep(LITEWS_CONNECT_RETRY_DELAY);
		}
	}

	freeaddrinfo(result);

	if (s->socket == LITEWS_INVALID_SOCKET) 
	{
		s->error = litews_error_new_code_descr(litews_error_code_connect_to_host, "Failed connect to host");
		s->command = COMMAND_END;   //connect error, no need send disconnect message;
		LOGE_LITEWS("SOCKET ERROR -> LITEWS_INVALID_SOCKET");
	} 
	else 
	{
		s->command = COMMAND_SEND_HANDSHAKE;
	}
	
}

#elif defined(SUPPORT_MBEDTLS)
static void litews_debug( void *ctx, int level,
                                const char *file, int line,
                                const char *str )
{
    ((void) level);
    fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
    fflush(  (FILE *) ctx  );
}


void mbedtls_socket_connect_to_host(litews_socket s) 
{
    int authmode = MBEDTLS_SSL_VERIFY_NONE;
    const char *pers = "https";
    int value, ret = 0;
    uint32_t flags;
    _litews_ssl *ssl = NULL;
    char portstr[16] = {0};

    int retry_number = 0;
    //char portbuf[100];

    //FUNCTION_BEGIN

    s->ssl = litews_malloc_zero(sizeof(_litews_ssl));
    if(!s->ssl)
    {
        LOGE_LITEWS("mbedtls Memory malloc error");
        ret = -1;
        goto exit;
    }
    ssl = s->ssl;
    
    if(s->client_cert)
    {
        authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
    }
    
    //mbedtls_debug_set_threshold(1); //enable mbedtls debug; set debug level = 3; 
    
    mbedtls_net_init(&ssl->net_ctx);
    mbedtls_ssl_init(&ssl->ssl_ctx);
    mbedtls_ssl_config_init(&ssl->ssl_conf);
    //mbedtls_x509_crt_init(&ssl->cacert);
    mbedtls_x509_crt_init(&ssl->clicert);
    //mbedtls_pk_init(&ssl->pkey);
    mbedtls_ctr_drbg_init(&ssl->ctr_drbg);  //zz mark

    /* 
        0. Initialize the RNG and the session data
    */
    
    //zz mark
    
    mbedtls_entropy_init(&ssl->entropy);
    if((value = mbedtls_ctr_drbg_seed(&ssl->ctr_drbg, mbedtls_entropy_func, &ssl->entropy,(const unsigned char*)pers, strlen(pers))) != 0)
    {
        LOGE_LITEWS("mbedtls_ctr_drbg_seed() failed, value:-0x%x.", -value);
        ret = -1;
        goto exit;
    }
    
    /* 
        1. Load the client certificate
    */
    //LOGD_LITEWS("== client_cert ==");
    //LOGD_LITEWS("%s", s->client_cert);
    if(s->client_cert)
    {
        ret = mbedtls_x509_crt_parse(&ssl->clicert, (const unsigned char *)s->client_cert, strlen(s->client_cert)+1);
        if(ret < 0)
        {
            LOGE_LITEWS("mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
            goto exit;
        }
    }

     /* 
         2. Start the connection
     */

    litews_sprintf(portstr, 16, "%i", s->port);
    LOGD_LITEWS("Connecting to %s:%s...", s->host, portstr);

    ret = -1;    //reset flag retry conncet 5 times
    while ((++retry_number < LITEWS_CONNECT_ATTEMPS) && (ret != 0))
    {
        if ((ret = mbedtls_net_connect(&ssl->net_ctx, s->host, portstr, MBEDTLS_NET_PROTO_TCP)) != 0)
        {
            LOGE_LITEWS("mbedtls_net_connect failed -%x", -ret);
        }
        if(retry_number >= LITEWS_CONNECT_ATTEMPS)
        {
            goto exit;
        }
        litews_thread_sleep(LITEWS_CONNECT_RETRY_DELAY);
    }

    LOGD_LITEWS("Socket Connected.");

    s->buffer_len = 0;
    s->socket = ssl->net_ctx.fd;


    /* 
         3. Setup stuff
    */  

     if((value = mbedtls_ssl_config_defaults(&ssl->ssl_conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
     {
         LOGE_LITEWS("mbedtls_ssl_config_defaults failed value: -0x%x", -value);
         ret = -1;
         goto exit;
     }

    /*
    memcpy(&ssl->profile, ssl->ssl_conf.cert_profile, sizeof(mbedtls_x509_crt_profile));
    ssl->profile.allowed_mds = ssl->profile.allowed_mds | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_MD5);
    mbedtls_ssl_conf_cert_profile(&ssl->ssl_conf, &ssl->profile);
    */
    
    mbedtls_ssl_conf_authmode(&ssl->ssl_conf, authmode);
    mbedtls_ssl_conf_ca_chain(&ssl->ssl_conf, &ssl->clicert, NULL);

    /*  
    if (s->client_cert && (ret = mbedtls_ssl_conf_own_cert(&ssl->ssl_conf, &ssl->clicert, &ssl->pkey)) != 0) 
    {
         DBG("[SSL]failed! mbedtls_ssl_conf_own_cert returned %d.", ret );
         goto exit;
    }
    */

    mbedtls_ssl_conf_rng(&ssl->ssl_conf, mbedtls_ctr_drbg_random, &ssl->ctr_drbg);    //zz mark
    mbedtls_ssl_conf_dbg(&ssl->ssl_conf, litews_debug, stdout);


    if ((value = mbedtls_ssl_setup(&ssl->ssl_ctx, &ssl->ssl_conf)) != 0) 
    {
        LOGE_LITEWS("mbedtls_ssl_setup() failed, value:-0x%x.", -value);
        ret = -1;
        goto exit;
    }

    if((ret = mbedtls_ssl_set_hostname(&ssl->ssl_ctx, s->host)) != 0)
    {
        LOGE_LITEWS( "failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret );
        goto exit;

    }

    mbedtls_ssl_set_bio(&ssl->ssl_ctx, &ssl->net_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

    /* 
         4. handshake
    */
    LOGD_LITEWS("Performing the SSL/TLS handshake...");

    while ((ret = mbedtls_ssl_handshake(&ssl->ssl_ctx)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            LOGE_LITEWS("[SSL]mbedtls_ssl_handshake() failed, ret:-0x%x.", -ret);
            goto exit;
        }
        litews_thread_sleep(100);
    }

    mbedtls_net_set_nonblock(&ssl->net_ctx);

    /* 
         4. Verify the server certificate
    */
    LOGD_LITEWS("Verifying peer X.509 certificate...");
    if ((flags = mbedtls_ssl_get_verify_result(&ssl->ssl_ctx)) != 0)
    {
        /* In real life, we probably want to close connection if ret != 0 */
        LOGE_LITEWS("Failed to verify peer certificate!");
        char verfy_buff[512];
        mbedtls_x509_crt_verify_info(verfy_buff, sizeof(verfy_buff), "  ! ", flags);
        LOGE_LITEWS("[SSL] %s", verfy_buff);
    }
    else 
    {
        LOGD_LITEWS("Certificate verified OK");
    }

exit:
    if(ret != 0) 
    {
        /*
        #ifdef MBEDTLS_ERROR_C
        char error_buf[100];
        mbedtls_strerror( ret, error_buf, 100 );
        LOGE_LITEWS("[SSL]Last error was: %d - %s\n\n", ret, error_buf);
        #endif
        */
        LOGE_LITEWS("[SSL]ret=0x%x.", ret);
        s->error = litews_error_new_code_descr(litews_error_code_connect_to_host, "Failed connect to host");
        LOGE_LITEWS("[SSL]%s: code:%d, error:%s", __FUNCTION__, s->error->code, s->error->description);
        s->command = COMMAND_END;

        if (s->ssl)
        {
            mbedtls_net_free(&s->ssl->net_ctx);
            //mbedtls_x509_crt_free(&s->ssl->cacert);
            mbedtls_x509_crt_free(&s->ssl->clicert);
            //mbedtls_pk_free(&s->ssl->pkey);
            mbedtls_ssl_free(&s->ssl->ssl_ctx);
            mbedtls_ssl_config_free(&s->ssl->ssl_conf);
            mbedtls_ctr_drbg_free(&s->ssl->ctr_drbg);  //zz mark
            mbedtls_entropy_free(&s->ssl->entropy);  //zz mark

            litews_free(s->ssl);
            s->ssl = NULL;
        }
        s->socket = LITEWS_INVALID_SOCKET;
    }
    else
    {
        s->command = COMMAND_SEND_HANDSHAKE;
    }

    return;

}

#else

void litews_socket_connect_to_host(litews_socket s) 
{
	struct addrinfo * result = NULL;
	struct addrinfo * p = NULL;
	litews_socket_t sock = LITEWS_INVALID_SOCKET;
	int retry_number = 0;
#if defined(LITEWS_OS_WINDOWS)
	unsigned long iMode = 0;
#endif

	result = litews_socket_connect_getaddr_info(s);
	if (!result) {
		return;
	}

	while ((++retry_number < LITEWS_CONNECT_ATTEMPS) && (sock == LITEWS_INVALID_SOCKET)) {
		for (p = result; p != NULL; p = p->ai_next) {
			sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (sock != LITEWS_INVALID_SOCKET) {
				litews_socket_set_option(sock, SO_ERROR, 1); // When an error occurs on a socket, set error variable so_error and notify process
				litews_socket_set_option(sock, SO_KEEPALIVE, 1); // Periodically test if connection is alive

				if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
                    s->received_len = 0;
					s->socket = sock;
#if defined(LITEWS_OS_WINDOWS)
					// If iMode != 0, non-blocking mode is enabled.
					iMode = 1;
					ioctlsocket(s->socket, FIONBIO, &iMode);
#else
					fcntl(s->socket, F_SETFL, O_NONBLOCK);
#endif
					break;
				}
				//LITEWS_SOCK_CLOSE(sock);
				litews_socket_close(s);
			}
		}
		if (sock == LITEWS_INVALID_SOCKET) {
			litews_thread_sleep(LITEWS_CONNECT_RETRY_DELAY);
		}
	}

	freeaddrinfo(result);

	if (s->socket == LITEWS_INVALID_SOCKET) {
#if defined(LITEWS_OS_WINDOWS)
		WSACleanup();
#endif
		s->error = litews_error_new_code_descr(litews_error_code_connect_to_host, "Failed connect to host");
		s->command = COMMAND_INFORM_DISCONNECTED;
	} else {
		s->command = COMMAND_SEND_HANDSHAKE;
	}
}

#endif

#define WAIT_TIME_MAX               100  // 80 ms
#define WAIT_TIME_MIN               5  // 5 ms

static void litews_socket_work_th_func(void * user_object) 
{
    litews_socket s = (litews_socket)user_object;
    //size_t loop_number = 0;
    int wait_time = WAIT_TIME_MIN;

    while (s->command < COMMAND_END) 
    {
        WS_PING_LOOP++;

        litews_mutex_lock(s->work_mutex);
        switch (s->command) 
        {
            case COMMAND_CONNECT_TO_HOST: 
#ifdef SUPPORT_MBEDTLS
                mbedtls_socket_connect_to_host(s);
#elif defined(SUPPORT_WOLFSSL)
                wolfssl_socket_connect_to_host(s);
#else
                litews_socket_connect_to_host(s); 
#endif
                break;

            case COMMAND_SEND_HANDSHAKE: 
#if defined(SUPPORT_MBEDTLS) || defined(SUPPORT_WOLFSSL)
                ssl_socket_send_handshake(s);
#else
                litews_socket_send_handshake(s); 
#endif
                break;

            case COMMAND_WAIT_HANDSHAKE_RESPONCE: 
#if defined(SUPPORT_MBEDTLS) || defined(SUPPORT_WOLFSSL)
                ssl_socket_wait_handshake_responce(s);
#else
                litews_socket_wait_handshake_responce(s); 
#endif
                break;

            case COMMAND_DISCONNECT: 
                litews_socket_send_disconnect(s); 
                if (s->on_disconnected)  
                {
                    s->on_disconnected(s);
                }
                break;


            case COMMAND_IDLE:
                if (0) 
                {
                    WS_PING_LOOP = 0;

                    if (s->is_connected) 
                    {
                        LOGD_LITEWS("====websocket send ping====");
                        litews_socket_send_ping(s);
                    }
                    else
                    {
                        LOGD_LITEWS("====websocket flag is false====");
                    }
                }

                if (s->is_connected) 
                {   
                    litews_bool sRet= litews_socket_idle_send(s);
                    if (sRet == litews_true)
                        wait_time = WAIT_TIME_MIN;
                    else {
                        wait_time = wait_time << 1;
                        if (wait_time > WAIT_TIME_MAX) {
                            wait_time = WAIT_TIME_MAX;
                        }
                    }
                }

                if (s->is_connected) 
                {
                    int ret = litews_socket_idle_recv(s);
                    if (ret > 0) {
                        wait_time = WAIT_TIME_MIN;
                    } 
                }
                break;
                
            default: 
                break;
        }
        
        litews_mutex_unlock(s->work_mutex);

        switch (s->command) 
        {
            case COMMAND_INFORM_CONNECTED:
            {
                LOGD_LITEWS("websocket connect OK !!");
                s->command = COMMAND_IDLE;
                if (s->on_connected) 
                {
                    s->on_connected(s);
                }
            }
            break;
            
            case COMMAND_INFORM_DISCONNECTED: 
            {
                s->command = COMMAND_END;
                litews_socket_send_disconnect(s);

                if (s->on_disconnected)  
                {
                    s->on_disconnected(s);
                }
            }
            break;
            
            case COMMAND_IDLE:
                if (s->recvd_frames) 
                {
                    litews_socket_inform_recvd_frames(s);
                }
                break;
                
            default: 
            break;
        }
        
        litews_thread_sleep(10);
        
    }
    
    LOGE_LITEWS("END SOCKET LOOP!");
    litews_socket_close(s);
    
    s->work_thread = NULL;
    litews_socket_delete(s);
    }

litews_bool litews_socket_create_start_work_thread(litews_socket s) 
{
    litews_error_delete_clean(&s->error);
    s->command = COMMAND_NONE;
    s->work_thread = litews_thread_create(&litews_socket_work_th_func, s);
    if (s->work_thread) 
    {
        s->command = COMMAND_CONNECT_TO_HOST;
        return litews_true;
    }
    return litews_false;
}

#ifdef SUPPORT_REDUCE_MEM
#else
void litews_socket_resize_received(litews_socket s, const size_t size) 
{
	void * res = NULL;
	size_t min = 0;
	
	if (size == s->received_size) 
	{
		return;
	}

	res = litews_malloc(size);
	assert(res && (size > 0));

	min = (s->received_size < size) ? s->received_size : size;
	if (min > 0 && s->received) 
	{
		AG_OS_MEMCPY(res, s->received, min);
	}
	litews_free_clean(&s->received);
	s->received = res;
	s->received_size = size;
}
#endif

#ifdef SUPPORT_WOLFSSL
void LITEWS_SOCK_CLOSE(litews_socket s)
{
    //wolfSSL_shutdown(xWolfSSL_Object);
    wolfSSL_free(xWolfSSL_Object);
    wolfSSL_CTX_free(xWolfSSL_Context);
    wolfSSL_Cleanup();
    close(s->socket);
}

#elif defined(SUPPORT_MBEDTLS)
void LITEWS_SOCK_CLOSE(litews_socket s)
{
    _litews_ssl *ssl = s->ssl;

    if(!ssl)
    {
        //return -1;
        return;
    }
    
	mbedtls_ssl_close_notify(&ssl->ssl_ctx);
    mbedtls_net_free(&ssl->net_ctx);
    //mbedtls_x509_crt_free(&ssl->cacert);
    mbedtls_x509_crt_free(&ssl->clicert);
    //mbedtls_pk_free(&ssl->pkey);
    mbedtls_ssl_free(&ssl->ssl_ctx);
    mbedtls_ssl_config_free(&ssl->ssl_conf);
    mbedtls_ctr_drbg_free(&ssl->ctr_drbg);  //zz mark
    mbedtls_entropy_free(&ssl->entropy);  //zz mark

    litews_free(ssl);
    s->ssl = NULL;

    //return 0;
    return;
}
#else
void LITEWS_SOCK_CLOSE(litews_socket s)
{
    close(s->socket);
}
#endif

void litews_socket_close(litews_socket s) 
{
#ifdef SUPPORT_REDUCE_MEM
#else
    s->received_len = 0;
#endif
    
    if (s->socket != LITEWS_INVALID_SOCKET) 
    {
#if defined(SUPPORT_MBEDTLS)||defined(SUPPORT_WOLFSSL)
        LITEWS_SOCK_CLOSE(s);
#else
        LITEWS_SOCK_CLOSE(s->socket);
#endif
        s->socket = LITEWS_INVALID_SOCKET;
    }
    s->is_connected = litews_false;
}

void litews_socket_append_recvd_frames(litews_socket s, _litews_frame * frame) 
{
	_litews_node_value frame_list_var;
	frame_list_var.object = frame;
	if (s->recvd_frames) 
	{
		litews_list_append(s->recvd_frames, frame_list_var);
	} 
	else 
	{
		s->recvd_frames = litews_list_create();
		s->recvd_frames->value = frame_list_var;
	}
}

void litews_socket_append_send_frames(litews_socket s, _litews_frame * frame) 
{
	_litews_node_value frame_list_var;
	frame_list_var.object = frame;
	if (s->send_frames) 
	{
		litews_list_append(s->send_frames, frame_list_var);
	} 
	else 
	{
		s->send_frames = litews_list_create();
		s->send_frames->value = frame_list_var;
	}
}

litews_bool litews_socket_send_text_priv(litews_socket s, const char * text) 
{
	size_t len = text ? strlen(text) : 0;
	_litews_frame * frame = NULL;

	if (len <= 0) 
	{
		return litews_false;
	}

	frame = litews_frame_create();
	frame->is_masked = litews_true;
	frame->opcode = litews_opcode_text_frame;
	litews_frame_fill_with_send_data(frame, text, len);
	litews_socket_append_send_frames(s, frame);

	return litews_true;
}

litews_bool litews_socket_send_binary_priv(litews_socket s, const char * data, size_t length, int flag) 
{
	_litews_frame * frame = NULL;
   
	frame = litews_frame_create();
	frame->is_masked = litews_true;

	if(flag == litews_frame_start)
	{
        frame->opcode = litews_opcode_binary_frame;
	}
	else if(flag == litews_frame_continue)
	{
	    frame->opcode = litews_opcode_continuation;
	}
	else if(flag == litews_frame_end)
	{
	    frame->opcode = litews_opcode_continuation;
	}
	
	litews_frame_fill_with_send_bin_data(frame, data, length, flag);
	litews_socket_append_send_frames(s, frame);

	return litews_true;
}

void litews_socket_delete_all_frames_in_list(_litews_list * list_with_frames) 
{
    _litews_frame * frame = NULL;
    _litews_node * cur = list_with_frames;
    while (cur) {
        frame = (_litews_frame *)cur->value.object;
        if (frame) {
            litews_frame_delete(frame);
        }
        cur->value.object = NULL;
        cur = cur->next;
    }
}

void litews_socket_set_option(litews_socket_t s, int option, int value) {
	setsockopt(s, SOL_SOCKET, option, (char *)&value, sizeof(int));
}

void litews_socket_check_write_error(litews_socket s, int error_num) {
#if defined(LITEWS_OS_WINDOWS)
	int socket_code = 0, code = 0;
	unsigned int socket_code_size = sizeof(int);
#else
	int socket_code = 0, code = 0;
	socklen_t socket_code_size = sizeof(socket_code);
#endif

	if (s->socket != LITEWS_INVALID_SOCKET) {
#if defined(LITEWS_OS_WINDOWS)
		if (getsockopt(s->socket, SOL_SOCKET, SO_ERROR, (char *)&socket_code, (int*)&socket_code_size) != 0) {
			socket_code = 0;
		}
#else
		if (getsockopt(s->socket, SOL_SOCKET, SO_ERROR, &socket_code, &socket_code_size) != 0) {
			socket_code = 0;
		}
#endif
	}

	code = (socket_code > 0) ? socket_code : error_num;
	if (code <= 0) {
		return;
	}
	
	switch (code) {
		// send errors
		case EACCES: //

//		case EAGAIN: // The socket is marked nonblocking and the requested operation would block
//		case EWOULDBLOCK: // The socket is marked nonblocking and the receive operation would block

		case EBADF: // An invalid descriptor was specified
		case ECONNRESET: // Connection reset by peer
		case EDESTADDRREQ: // The socket is not connection-mode, and no peer address is set
		case EFAULT: // An invalid user space address was specified for an argument
					// The receive buffer pointer(s) point outside the process's address space.
		case EINTR: // A signal occurred before any data was transmitted
					// The receive was interrupted by delivery of a signal before any data were available
		case EINVAL: // Invalid argument passed
		case EISCONN: // The connection-mode socket was connected already but a recipient was specified
		case EMSGSIZE: // The socket type requires that message be sent atomically, and the size of the message to be sent made this impossible
		case ENOBUFS: // The output queue for a network interface was full
		case ENOMEM: // No memory available
		case ENOTCONN: // The socket is not connected, and no target has been given
						// The socket is associated with a connection-oriented protocol and has not been connected
		case ENOTSOCK: // The argument sockfd is not a socket
					// The argument sockfd does not refer to a socket
		case EOPNOTSUPP: // Some bit in the flags argument is inappropriate for the socket type.
		case EPIPE: // The local end has been shut down on a connection oriented socket
		// recv errors
		case ECONNREFUSED: // A remote host refused to allow the network connection (typically because it is not running the requested service).

			s->error = litews_error_new_code_descr(litews_error_code_read_write_socket, litews_strerror(code));
			break;

		default:
			break;
	}
}

