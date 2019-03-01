
#include "litewebsocket.h"
#include "litews_socket.h"
#include "litews_memory.h"
#include "litews_string.h"
#include <assert.h>
#include "litews_log.h"
#include "aligenie_os.h"
#include "aligenie_ws.h"

#if !defined(LITEWS_OS_WINDOWS)
#include <signal.h>
#endif

// public
litews_bool litews_socket_connect(litews_socket socket) 
{
	const char * params_error_msg = NULL;
	if (!socket) {
		return litews_false;
	}

	litews_error_delete_clean(&socket->error);

	if (socket->port <= 0) 
	{
		params_error_msg = "No URL port provided";
	}
	if (!socket->scheme) 
	{
		params_error_msg = "No URL scheme provided";
	}
	if (!socket->host) 
	{
		params_error_msg = "No URL host provided";
	}
	if (!socket->path) 
	{
		params_error_msg = "No URL path provided";
	}
	if (!socket->on_disconnected) 
	{
		params_error_msg = "No on_disconnected callback provided";
	}

#ifdef SUPPORT_REDUCE_MEM
#else
    socket->received_len = 0;
#endif
    
	if (params_error_msg) 
	{
		socket->error = litews_error_new_code_descr(litews_error_code_missed_parameter, params_error_msg);
		return litews_false;
	}
	return litews_socket_create_start_work_thread(socket);
}

void litews_socket_disconnect_and_release(litews_socket socket) {
	if (!socket) {
		LOGE_LITEWS("SOCKET abnomral");
		return;
	}
	
	litews_mutex_lock(socket->work_mutex);

	litews_socket_delete_all_frames_in_list(socket->send_frames);
	litews_list_delete_clean(&socket->send_frames);

	if (socket->is_connected) { // connected in loop
		LOGD_LITEWS("send socket command COMMAND DISCONNECT");
		socket->command = COMMAND_DISCONNECT;
		litews_mutex_unlock(socket->work_mutex);
	} else if (socket->work_thread) { // disconnected in loop
		LOGD_LITEWS("send socket command COMMAND END");
		socket->command = COMMAND_END;
		litews_mutex_unlock(socket->work_mutex);
	} else if (socket->command != COMMAND_END) {
		// not in loop
		LOGD_LITEWS("socket command != COMMAND END, delete socket");
		litews_mutex_unlock(socket->work_mutex);
		litews_socket_delete(socket);
	}
}

litews_bool litews_socket_send_text(litews_socket socket, const char * text) 
{
	litews_bool r = litews_false;
	if (socket) 
	{
		litews_mutex_lock(socket->send_mutex);
		r = litews_socket_send_text_priv(socket, text);
		litews_mutex_unlock(socket->send_mutex);
	}
	return r;
}

litews_bool litews_socket_send_binary(litews_socket socket, const unsigned char * data, int length, int flag) 
{
	litews_bool r = litews_false;
	if (socket) 
	{
		litews_mutex_lock(socket->send_mutex);
		r = litews_socket_send_binary_priv(socket, (const char *)data, (size_t)length, flag);
		litews_mutex_unlock(socket->send_mutex);
	}
	return r;
}

#if !defined(LITEWS_OS_WINDOWS)
void litews_socket_handle_sigpipe(int signal_number) {
	printf("\nliblitews handle sigpipe %i", signal_number);
	(void)signal_number;
	return;
}
#endif

#define STRING_I(s) #s
#define TO_STRING(s) STRING_I(s)

void litews_socket_check_info(const char * info) {
	assert(info);
	(void)info;
}

litews_socket litews_socket_create(void) 
{
	litews_socket s = (litews_socket)litews_malloc_zero(sizeof(struct litews_socket_struct));
	if (!s) 
	{
		return NULL;
	}
	
    if(s == NULL)
    {
        LOGE_LITEWS("WebSocket failed to malloce litews_socket");
		return s;
    }
    
#if (!defined(LITEWS_OS_WINDOWS)&&!defined(LITEWS_OS_RTOS))
	signal(SIGPIPE, litews_socket_handle_sigpipe);
#endif

	s->port = -1;
	s->socket = LITEWS_INVALID_SOCKET;
	s->command = COMMAND_NONE;
	s->work_mutex = litews_mutex_create_recursive();
	s->send_mutex = litews_mutex_create_recursive();
	static const char * info = "liblitews ver: " TO_STRING(LITEWS_VERSION_MAJOR) "." TO_STRING(LITEWS_VERSION_MINOR) "." TO_STRING(LITEWS_VERSION_PATCH) "\n";
	litews_socket_check_info(info);
	return s;
}

void litews_socket_delete(litews_socket s) 
{
	litews_socket_close(s);

	litews_string_delete_clean(&s->sec_ws_accept);

    #ifdef SUPPORT_REDUCE_MEM
    #else
	litews_free_clean(&s->received);
	s->received_size = 0;
	s->received_len = 0;
	#endif

	litews_socket_delete_all_frames_in_list(s->send_frames);
	litews_list_delete_clean(&s->send_frames);
	litews_socket_delete_all_frames_in_list(s->recvd_frames);
	litews_list_delete_clean(&s->recvd_frames);

	litews_string_delete_clean(&s->scheme);
	litews_string_delete_clean(&s->host);
	litews_string_delete_clean(&s->path);
    litews_string_delete_clean(&s->client_cert);

	litews_string_delete_clean(&s->sec_ws_accept);

	litews_error_delete_clean(&s->error);

    #ifdef SUPPORT_REDUCE_MEM
    #else
	litews_free_clean(&s->received);
	#endif
	
	litews_socket_delete_all_frames_in_list(s->send_frames);
	litews_list_delete_clean(&s->send_frames);
	litews_socket_delete_all_frames_in_list(s->recvd_frames);
	litews_list_delete_clean(&s->recvd_frames);

	litews_mutex_delete(s->work_mutex);
	litews_mutex_delete(s->send_mutex);

	litews_free(s);
    s = NULL;
}

void litews_socket_set_url(litews_socket socket,
						const char * scheme,
						const char * host,
						const int port,
						const char * path) {
	if (socket) {
		litews_string_delete(socket->scheme);
		socket->scheme = litews_string_copy(scheme);
		litews_string_delete(socket->host);
		socket->host = litews_string_copy(host);
		litews_string_delete(socket->path);
		socket->path = litews_string_copy(path);
		socket->port = port;
	}
}

void litews_socket_set_scheme(litews_socket socket, const char * scheme) {
	if (socket) {
		litews_string_delete(socket->scheme);
		socket->scheme = litews_string_copy(scheme);
	}
}

const char * litews_socket_get_scheme(litews_socket socket) {
	return socket ? socket->scheme : NULL;
}

void litews_socket_set_host(litews_socket socket, const char * host) {
	if (socket) {
		litews_string_delete(socket->host);
		socket->host = litews_string_copy(host);
	}
}

const char * litews_socket_get_host(litews_socket socket) {
	return socket ? socket->host : NULL;
}

void litews_socket_set_path(litews_socket socket, const char * path) {
	if (socket) {
		litews_string_delete(socket->path);
		socket->path = litews_string_copy(path);
	}
}

const char * litews_socket_get_path(litews_socket socket) {
	return socket ? socket->path : NULL;
}

#if defined(SUPPORT_MBEDTLS) || defined(SUPPORT_WOLFSSL) 
void litews_socket_set_server_cert(litews_socket socket, const char *server_cert)
{
    if(socket)
    {
        #ifdef X871
        litews_string_delete((char *)socket->client_cert);
        #else
        litews_string_delete(socket->client_cert);
        #endif
        socket->client_cert = litews_string_copy(server_cert);
    }
}
#endif

void litews_socket_set_port(litews_socket socket, const int port) {
	if (socket) {
		socket->port = port;
	}
}

int litews_socket_get_port(litews_socket socket) {
	return socket ? socket->port : -1;
}

/*
unsigned int _litews_socket_get_receive_buffer_size(litews_socket_t socket) {
	unsigned int size = 0;
#if defined(LITEWS_OS_WINDOWS)
	int len = sizeof(unsigned int);
	if (getsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *)&size, &len) == -1) { 
		size = 0;
	}
#else
	socklen_t len = sizeof(unsigned int);
	if (getsockopt(socket, SOL_SOCKET, SO_RCVBUF, &size, &len) == -1) { 
		size = 0; 
	}
#endif
	return size;
}

unsigned int litews_socket_get_receive_buffer_size(litews_socket socket) {
	_litews_socket * s = (_litews_socket *)socket;
	if (!s) { 
		return 0; 
	}
	if (s->socket == LITEWS_INVALID_SOCKET) { 
		return 0; 
	}
	return _litews_socket_get_receive_buffer_size(s->socket);
}
*/

litews_error litews_socket_get_error(litews_socket socket) {
	return socket ? socket->error : NULL;
}

void litews_socket_set_user_object(litews_socket socket, void * user_object) {
	if (socket) {
		socket->user_object = user_object;
	}
}

void * litews_socket_get_user_object(litews_socket socket) {
	return socket ? socket->user_object : NULL;
}

void litews_socket_set_on_connected(litews_socket socket, litews_on_socket callback) {
	if (socket) {
		socket->on_connected = callback;
	}
}

void litews_socket_set_on_disconnected(litews_socket socket, litews_on_socket callback) {
	if (socket) {
		socket->on_disconnected = callback;
	}
}

void litews_socket_set_on_received_text(litews_socket socket, litews_on_socket_recvd_text callback) {
	if (socket) {
		socket->on_recvd_text = callback;
	}
}

void litews_socket_set_on_received_bin(litews_socket socket, litews_on_socket_recvd_bin callback) {
	if (socket) {
		socket->on_recvd_bin = callback;
	}
}

litews_bool litews_socket_is_connected(litews_socket socket) {
	litews_bool r = litews_false;
	if (socket) 
	{
		litews_mutex_lock(socket->send_mutex);
		r = socket->is_connected;
		litews_mutex_unlock(socket->send_mutex);
	}
	return r;
}

