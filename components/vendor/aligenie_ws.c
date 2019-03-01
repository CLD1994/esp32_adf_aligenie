
#include <string.h>

#include "aligenie_ws.h"
#include "litewebsocket.h"
#include "librws.h"
#include "esp_log.h"

// #include "aglog.h"

static litews_socket g_litews_socket = NULL;
static AG_WS_CALLBACKS_T *ag_ws_cb;
static const char* LOG_TAG = "ag_ws";

static void _ag_ws_on_connected(litews_socket socket)
{
    (*ag_ws_cb->cb_on_connect)();
}
static void _ag_ws_on_disconnected(litews_socket socket)
{
    (*ag_ws_cb->cb_on_disconnect)();
}

static void websocket_on_recv_text(litews_socket socket, const char * text, const unsigned int length)
{
    (*ag_ws_cb->cb_on_recv_text)((char *)text, (unsigned int)length);
}

static void websocket_on_recv_bin(litews_socket socket, const void *data, const unsigned int length, int flag)
{
    switch(flag)
    {
        case litews_frame_start:
        {
            (*ag_ws_cb->cb_on_recv_bin)((void *)data, (unsigned int)length, AG_WS_BIN_DATA_START);
            break;
        }

        case litews_frame_continue:
        {
            (*ag_ws_cb->cb_on_recv_bin)((void *)data, (unsigned int)length, AG_WS_BIN_DATA_CONTINUE);
            break;
        }

        case litews_frame_end:
        {
            (*ag_ws_cb->cb_on_recv_bin)((void *)data, (unsigned int)length, AG_WS_BIN_DATA_FINISH);
            break;
        }
    }
    
}

int32_t ag_ws_connect(AG_WS_CONNECT_INFO_T * info)
{
    if (g_litews_socket != NULL) {
        ESP_LOGE(LOG_TAG, "g_litews_socket != NULL, just wait...\n");
        return (litews_true == litews_false);
    }

    if (!info->schema || !info->server|| !info->path)
    {
        ESP_LOGE(LOG_TAG,  "%s: Invalid parameter(s).", __FUNCTION__);
        return AG_WS_RET_ERROR;
    }

    if (g_litews_socket)
    {
        ESP_LOGW(LOG_TAG,  "%s: WebSocket is not closed. Close forcely", __FUNCTION__);
        g_litews_socket = NULL;
    }
  
    g_litews_socket = litews_socket_create();
    if (g_litews_socket == NULL)
    {
        ESP_LOGE(LOG_TAG,  "%s: Creat WebSocket Fail.", __FUNCTION__);
        return AG_WS_RET_ERROR;
    }

    litews_socket_set_scheme(g_litews_socket, info->schema);
    litews_socket_set_host(g_litews_socket, info->server);
    litews_socket_set_port(g_litews_socket, info->port);
    litews_socket_set_path(g_litews_socket, info->path);

    if(info->schema && strcmp(info->schema, "wss") == 0) {
        if (info->cacert) {
            litews_socket_set_server_cert(g_litews_socket, info->cacert);
        }
    }

    ag_ws_cb = info->callbacks;
    litews_socket_set_on_connected(g_litews_socket, _ag_ws_on_connected);
    litews_socket_set_on_disconnected(g_litews_socket, _ag_ws_on_disconnected);
    litews_socket_set_on_received_text(g_litews_socket, websocket_on_recv_text);
    litews_socket_set_on_received_bin(g_litews_socket, websocket_on_recv_bin);

    //connect
    rws_bool ret= litews_socket_connect(g_litews_socket);

    return (ret==rws_true) ? AG_WS_RET_OK : AG_WS_RET_ERROR;
}

int32_t ag_ws_disconnect()
{
    litews_socket_disconnect_and_release(g_litews_socket);
    g_litews_socket = NULL;
    return litews_true;
}

void ag_set_socket2null()
{
    if (g_litews_socket != NULL)
        g_litews_socket = NULL;
}

int32_t ag_ws_send_text(char * text, uint32_t len)
{
    return litews_true == litews_socket_send_text(g_litews_socket, text);
}


int32_t ag_ws_send_binary(void * data, uint32_t length, AG_WS_BIN_DATA_TYPE_T type)
{
    
    litews_frame_status_t status;
    switch(type){
        case AG_WS_BIN_DATA_START:
            status = litews_frame_start;
            break;
        case AG_WS_BIN_DATA_CONTINUE:
            status = litews_frame_continue;
            break;
        case AG_WS_BIN_DATA_FINISH:
            status = litews_frame_end;
            break;
        default:
            status = litews_frame_one;
            break;
    }
    return litews_true == litews_socket_send_binary(g_litews_socket, data, length, status);
    //return litews_true;
}


AG_WS_STATUS_E ag_ws_get_connection_status()
{
    if(litews_socket_is_connected(g_litews_socket))
    {
        return AG_WS_STATUS_CONNECTED;
    }
    else
    {
        return AG_WS_STATUS_DISCONNECTED;
    }
}

