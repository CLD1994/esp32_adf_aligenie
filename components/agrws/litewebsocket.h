
#ifndef __LIBLITEWS_H__
#define __LIBLITEWS_H__ 1


#include <stdio.h>


#define LITEWS_VERSION_MAJOR 1
#define LITEWS_VERSION_MINOR 2
#define LITEWS_VERSION_PATCH 4

typedef enum _litews_frame_status
{
	litews_frame_start=0x0, //multi frame for first frame
	litews_frame_continue, //multi frame for continue frame
	litews_frame_end, //multi frame for last frame
	litews_frame_one, //singal frame
} litews_frame_status_t;


// extern
#if defined(__cplusplus) || defined(_cplusplus)
#define LITEWS_EXTERN extern "C"
#else
#define LITEWS_EXTERN extern
#endif


// attribute
#if defined(__GNUC__)
#if (__GNUC__ >= 4)
#if defined(__cplusplus) || defined(_cplusplus)
#define LITEWS_ATTRIB __attribute__((visibility("default")))
#else
#define LITEWS_ATTRIB __attribute__((visibility("default")))
#endif
#endif
#endif


// check attrib and define empty if not defined
#if !defined(LITEWS_ATTRIB)
#define LITEWS_ATTRIB
#endif


// dll api
#if defined(LITEWS_OS_WINDOWS)
#if defined(LITEWS_BUILD)
#define LITEWS_DYLIB_API __declspec(dllexport)
#else
#define LITEWS_DYLIB_API __declspec(dllimport)
#endif
#endif

// check dll api and define empty if not defined
#if !defined(LITEWS_DYLIB_API)
#define LITEWS_DYLIB_API
#endif


// combined lib api
#define LITEWS_API(return_type) LITEWS_EXTERN LITEWS_ATTRIB LITEWS_DYLIB_API return_type

// error
typedef enum _litews_error_code 
{
	litews_error_code_none = 0,
	litews_error_code_missed_parameter,
	litews_error_code_send_handshake,
	litews_error_code_parse_handshake,
	litews_error_code_read_write_socket,
	litews_error_code_connect_to_host,
	/**
	 @brief Connection was closed by endpoint.
	 Reasons: an endpoint shutting down, an endpoint having received a frame too large, or an
	 endpoint having received a frame that does not conform to the format expected by the endpoint.
	 */
	litews_error_code_connection_closed,

} litews_error_code;


// types

/**
 @brief Boolean type as unsigned byte type.
 */
typedef unsigned char litews_bool;
#define litews_true 1
#define litews_false 0


/**
 @brief Type of all public objects.
 */
typedef void* litews_handle;


/**
 @brief Socket handle.
 */
typedef struct litews_socket_struct * litews_socket;


/**
 @brief Error object handle.
 */
typedef struct litews_error_struct * litews_error;


/**
 @brief Mutex object handle.
 */
typedef litews_handle litews_mutex;


/**
 @brief Thread object handle.
 */
typedef struct litews_thread_struct * litews_thread;


/**
 @brief Callback type of thread function.
 @param user_object User object provided during thread creation.
 */
typedef void (*litews_thread_funct)(void * user_object);


/**
 @brief Callback type of socket object.
 @param socket Socket object.
 */
typedef void (*litews_on_socket)(litews_socket socket);


/**
 @brief Callback type on socket receive text frame.
 @param socket Socket object.
 @param text Pointer to reseived text.
 @param length Received text lenght without null terminated char.
 */
typedef void (*litews_on_socket_recvd_text)(litews_socket socket, const char * text, const unsigned int length);


/**
 @brief Callback type on socket receive binary frame.
 @param socket Socket object.
 @param data Received binary data.
 @param length Received binary data lenght.
 */
typedef void (*litews_on_socket_recvd_bin)(litews_socket socket, const void * data, const unsigned int length, int flag);


// socket

/**
 @brief Create new socket.
 @return Socket handler or NULL on error.
 */
LITEWS_API(litews_socket) litews_socket_create(void);


/**
 @brief Set socket connect URL.
 @param socket Socket object.
 @param scheme Connect URL scheme, "http" or "ws"
 @param scheme Connect URL host, "echo.websocket.org"
 @param scheme Connect URL port.
 @param scheme Connect URL path started with '/' character, "/" - for empty, "/path"
 @code
 litews_socket_set_url(socket, "http", "echo.websocket.org", 80, "/");
 litews_socket_set_url(socket, "ws", "echo.websocket.org", 80, "/");
 @endcode
 */
LITEWS_API(void) litews_socket_set_url(litews_socket socket,
								 const char * scheme,
								 const char * host,
								 const int port,
								 const char * path);

/**
 @brief Set socket connect URL scheme string.
 @param socket Socket object.
 @param scheme Connect URL scheme, "http" or "ws"
 @code
 litews_socket_set_scheme(socket, "http");
 litews_socket_set_scheme(socket, "ws");
 @endcode
 */
LITEWS_API(void) litews_socket_set_scheme(litews_socket socket, const char * scheme);


/**
 @brief Get socket connect URL scheme string.
 @param socket Socket object.
 @return Connect URL cheme or null.
 */
LITEWS_API(const char *) litews_socket_get_scheme(litews_socket socket);


/**
 @brief Set socket connect URL scheme string.
 @param socket Socket object.
 @param scheme Connect URL host, "echo.websocket.org"
 @code
 litews_socket_set_host(socket, "echo.websocket.org");
 @endcode
 */
LITEWS_API(void) litews_socket_set_host(litews_socket socket, const char * host);


/**
 @brief Get socket connect URL host string.
 @param socket Socket object.
 @return Connect URL host or null.
 */
LITEWS_API(const char *) litews_socket_get_host(litews_socket socket);


/**
 @brief Set socket connect URL port.
 @param socket Socket object.
 @param scheme Connect URL port.
 @code
 litews_socket_set_port(socket, 80);
 @endcode
 */
LITEWS_API(void) litews_socket_set_port(litews_socket socket, const int port);

/**
 @brief Set server certificate for ssl connection.
 @param socket Socket object.
 @param server_cert Server certificate.
 @param server_cert_len The length of the server certificate caculated by sizeof.
 */

LITEWS_API(void) litews_socket_set_server_cert(litews_socket socket, const char *server_cert);

/**
 @brief Get socket connect URL port.
 @param socket Socket object.
 @return Connect URL port or 0.
 */
LITEWS_API(int) litews_socket_get_port(litews_socket socket);


/**
 @brief Set socket connect URL path string.
 @param socket Socket object.
 @param scheme Connect URL path started with '/' character, "/" - for empty, "/path"
 @code
 litews_socket_set_path(socket, "/"); // empty path
 litews_socket_set_path(socket, "/path"); // some path
 @endcode
 */
LITEWS_API(void) litews_socket_set_path(litews_socket socket, const char * path);


/**
 @brief Get socket connect URL path string.
 @param socket Socket object.
 @return Connect URL path or null.
 */
LITEWS_API(const char *) litews_socket_get_path(litews_socket socket);


/**
 @brief Get socket last error object handle.
 @param socket Socket object.
 @return Last error object handle or null if no error.
 */
LITEWS_API(litews_error) litews_socket_get_error(litews_socket socket);


/**
 @brief Start connection.
 @detailed This method can generate error object.
 @param socket Socket object.
 @return litews_true - all params exists and connection started, otherwice litews_false.
 */
LITEWS_API(litews_bool) litews_socket_connect(litews_socket socket);


/**
 @brief Disconnect socket.
 @detailed Cleanup prev. send messages and start disconnection sequence.
 SHOULD forget about this socket handle and don't use it anymore.
 @warning Don't use this socket object handler after this command.
 @param socket Socket object.
 */
LITEWS_API(void) litews_socket_disconnect_and_release(litews_socket socket);


/**
 @brief Check is socket has connection to host and handshake(sucessfully done).
 @detailed Thread safe getter.
 @param socket Socket object.
 @return trw_true - connected to host and handshacked, otherwice litews_false.
 */
LITEWS_API(litews_bool) litews_socket_is_connected(litews_socket socket);


/**
 @brief Send text to connect socket.
 @detailed Thread safe method.
 @param socket Socket object.
 @param text Text string for sending.
 @return litews_true - socket and text exists and placed to send queue, otherwice litews_false.
 */
LITEWS_API(litews_bool) litews_socket_send_text(litews_socket socket, const char * text);


/**
 @brief Send text to connect socket.
 @detailed Thread safe method.
 @param socket Socket object.
 @param data Binary for sending.
 @param data_size Binary data size
 @param flag , for opcode control , define in litews_frame_status_t
 @return litews_true - socket and text exists and placed to send queue, otherwice litews_false.
 */
LITEWS_API(litews_bool) litews_socket_send_binary(litews_socket socket, const unsigned char * data , int data_size , int flag);


/**
 @brief Set socket user defined object pointer for identificating socket object.
 @param socket Socket object.
 @param user_object Void pointer to user object.
 */
LITEWS_API(void) litews_socket_set_user_object(litews_socket socket, void * user_object);


/**
 @brief Get socket user defined object.
 @param socket Socket object.
 @return User defined object pointer or null.
 */
LITEWS_API(void *) litews_socket_get_user_object(litews_socket socket);


LITEWS_API(void) litews_socket_set_on_connected(litews_socket socket, litews_on_socket callback);


LITEWS_API(void) litews_socket_set_on_disconnected(litews_socket socket, litews_on_socket callback);


LITEWS_API(void) litews_socket_set_on_received_text(litews_socket socket, litews_on_socket_recvd_text callback);


LITEWS_API(void) litews_socket_set_on_received_bin(litews_socket socket, litews_on_socket_recvd_bin callback);


/**
 @return 0 - if error is empty or no error, otherwice error code.
 */
LITEWS_API(int) litews_error_get_code(litews_error error);


/**
 @return 0 - if error is empty or no error, otherwice HTTP error.
 */
LITEWS_API(int) litews_error_get_http_error(litews_error error);


/**
 @brief Get description of the error object.
 */
LITEWS_API(const char *) litews_error_get_description(litews_error error);


// mutex

/**
 @brief Creates recursive mutex object.
 */
LITEWS_API(litews_mutex) litews_mutex_create_recursive(void);


/**
 @brief Lock mutex object.
 */
LITEWS_API(void) litews_mutex_lock(litews_mutex mutex);


/**
 @brief Unlock mutex object.
 */
LITEWS_API(void) litews_mutex_unlock(litews_mutex mutex);



/**
 @brief Unlock mutex object.
 */
LITEWS_API(void) litews_mutex_delete(litews_mutex mutex);


// thread

/**
 @brief Create thread object that start immidiatelly.
 */
LITEWS_API(litews_thread) litews_thread_create(litews_thread_funct thread_function, void * user_object);


/**
 @brief Pause current thread for a number of milliseconds.
 */
LITEWS_API(void) litews_thread_sleep(const unsigned int millisec);

#endif
