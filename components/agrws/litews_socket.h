
#ifndef __LITEWS_SOCKET_H__
#define __LITEWS_SOCKET_H__ 1

/*
#define SUPPORT_MBEDTLS        
//#define SUPPORT_WOLFSSL
#define SUPPORT_REDUCE_MEM
//#define X871
#define AligenieSDK
#define LITEWS_OS_RTOS
*/

#include "aligenie_libs.h"

/*
#if defined(LITEWS_OS_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(LITEWS_OS_RTOS)
    #ifdef X871
    #include "lwip/netdb.h"
    #include "lwip/sockets.h"
    #include <sys/types.h>
    //#include <fcntl.h>
    #include <unistd.h>
    #else
    #include <netdb.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <fcntl.h>
    #include <unistd.h>
    #endif
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#endif
*/

#include <assert.h>
//#include <errno.h>

#include "litews_error.h"
#include "litews_thread.h"
#include "litews_frame.h"
#include "litews_list.h"

/*
#ifdef SUPPORT_MBEDTLS
#ifdef X871
#include "net/mbedtls/net.h"
#include "net/mbedtls/ssl.h"
#include "net/mbedtls/certs.h"
#include "net/mbedtls/entropy.h"
#include "net/mbedtls/ctr_drbg.h"
#include "net/mbedtls/debug.h"
#else
//#include "mbedtls/net.h"
//#include "../../../../../features/mbedtls/inc/mbedtls/net.h"
//#include <mbedtls/net.h>
#include "mbedtls/ssl.h"
#include "mbedtls/certs.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#endif
#endif

//#include "esp_log.h"

#ifdef SUPPORT_WOLFSSL
#include "../../wolfssl/include/wolfssl/ssl.h"
#endif
*/

#if defined(LITEWS_OS_WINDOWS)
typedef SOCKET litews_socket_t;
#define LITEWS_INVALID_SOCKET INVALID_SOCKET
#define LITEWS_SOCK_CLOSE(sock) closesocket(sock)
#else

typedef int litews_socket_t;
#define LITEWS_INVALID_SOCKET -1

#endif

#if defined(SUPPORT_MBEDTLS) || defined(SUPPORT_WOLFSSL)
#define SSL_REC_BUFFER_SIZE              6144   //websocket total receive buffer size
#define SSL_REC_ONCE_SIZE                4096   //once receive buffer

#define SSL_WEBSOCKET_SEND_BUF_LEN       512    //handshake send buffer size
#define SSL_WEBSOCKET_RECV_BUF_LEN       2048    //handshake recive buffer size
#define SSL_WEBSOCKET_UUID "OTY0OWI0YzktY2FjNy00ZjIwLTljZGEtM2EwNTZkNmUyNTQx"
#endif

#ifdef SUPPORT_MBEDTLS
typedef struct _litews_ssl_struct
{
    mbedtls_ssl_context ssl_ctx;        /* mbedtls ssl context */
    mbedtls_net_context net_ctx;        /* Fill in socket id */
    mbedtls_ssl_config ssl_conf;        /* SSL configuration */
    mbedtls_entropy_context entropy;  //zz mark
    mbedtls_ctr_drbg_context ctr_drbg;  //zz mark
    mbedtls_x509_crt_profile profile;
    //mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    //mbedtls_pk_context pkey;
} _litews_ssl;
#endif

struct litews_socket_struct 
{
    int port;
    litews_socket_t socket;
    char * scheme;
    char * host;
    char * path;

    char * sec_ws_accept; // "Sec-WebSocket-Accept" field from handshake

    litews_thread work_thread;

    int command;

    unsigned int next_message_id;

    litews_bool is_connected; // sock connected + handshake done

    void * user_object;
    litews_on_socket on_connected;
    litews_on_socket on_disconnected;
    litews_on_socket_recvd_text on_recvd_text;
    litews_on_socket_recvd_bin on_recvd_bin;

#ifdef SUPPORT_REDUCE_MEM
#else
    void * received;
    size_t received_size; // size of 'received' memory
    size_t received_len; // length of actualy readed message
#endif

    _litews_list * send_frames;
    _litews_list * recvd_frames;

    litews_error error;

    litews_mutex work_mutex;
    litews_mutex send_mutex;

#ifdef SUPPORT_WOLFSSL
    char *client_cert;
    char received_buffer[SSL_REC_BUFFER_SIZE];
    size_t buffer_size;
    size_t buffer_len;
#endif

#ifdef SUPPORT_MBEDTLS
    const char *client_cert;
    _litews_ssl *ssl;

    char received_buffer[SSL_REC_BUFFER_SIZE];
    size_t buffer_size;
    size_t buffer_len;

#endif
};

litews_bool litews_socket_process_handshake_responce(litews_socket s);

// receive raw data from socket
litews_bool litews_socket_recv(litews_socket s);

// send raw data to socket
litews_bool litews_socket_send(litews_socket s, const void * data, const size_t data_size);

_litews_frame * litews_socket_last_unfin_recvd_frame_by_opcode(litews_socket s, const litews_opcode opcode);

void litews_socket_process_bin_or_text_frame(litews_socket s, _litews_frame * frame);

void litews_socket_process_ping_frame(litews_socket s, _litews_frame * frame);

void litews_socket_process_conn_close_frame(litews_socket s, _litews_frame * frame);

void litews_socket_process_received_frame(litews_socket s, _litews_frame * frame);

int litews_socket_idle_recv(litews_socket s);

litews_bool litews_socket_idle_send(litews_socket s);

void litews_socket_wait_handshake_responce(litews_socket s);

unsigned int litews_socket_get_next_message_id(litews_socket s);

void litews_socket_send_ping(litews_socket s);

void litews_socket_send_disconnect(litews_socket s);

void litews_socket_send_handshake(litews_socket s);

struct addrinfo * litews_socket_connect_getaddr_info(litews_socket s);

void litews_socket_connect_to_host(litews_socket s);

litews_bool litews_socket_create_start_work_thread(litews_socket s);

void litews_socket_close(litews_socket s);

void litews_socket_resize_received(litews_socket s, const size_t size);

void litews_socket_append_recvd_frames(litews_socket s, _litews_frame * frame);

void litews_socket_append_send_frames(litews_socket s, _litews_frame * frame);

litews_bool litews_socket_send_text_priv(litews_socket s, const char * text);

litews_bool litews_socket_send_binary_priv(litews_socket s, const char * data, size_t length, int flag) ;

void litews_socket_inform_recvd_frames(litews_socket s);

void litews_socket_set_option(litews_socket_t s, int option, int value);

void litews_socket_delete_all_frames_in_list(_litews_list * list_with_frames);

void litews_socket_check_write_error(litews_socket s, int error_num);

void litews_socket_delete(litews_socket s);

#define COMMAND_IDLE -1
#define COMMAND_NONE 0
#define COMMAND_CONNECT_TO_HOST 1
#define COMMAND_SEND_HANDSHAKE 2
#define COMMAND_WAIT_HANDSHAKE_RESPONCE 3
#define COMMAND_INFORM_CONNECTED 4
#define COMMAND_INFORM_DISCONNECTED 5
#define COMMAND_DISCONNECT 6

#define COMMAND_END 9999



#endif
