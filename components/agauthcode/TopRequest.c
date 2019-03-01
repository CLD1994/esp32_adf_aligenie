#include <stdlib.h>
#include <string.h>
#include "topsdk.h"
#include <ctype.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "rom/md5_hash.h"

static const char *TAG = "TOPreq";

void checksumMd5(unsigned char const* buf, size_t len, unsigned char *md5_result) 
{
    struct MD5Context context;
    unsigned char digest[16] = {0};

    MD5Init(&context);
    MD5Update(&context, buf, len);
    MD5Final(digest, &context);

    for (int i = 0; i < 16; ++i) {
        sprintf((char*)&md5_result[i*2], "%02X", (unsigned int)digest[i]);
    }
    md5_result[32] ='\0';
}

top_map* alloc_map(){
    top_map* m = (top_map*)malloc(sizeof(top_map));
    m->ite = 0;
    m->size = 16;
    m->total_length= 0;
    m->keys = malloc(sizeof(char*) * m->size);
    m->values = malloc(sizeof(char*) * m->size);
    return m;
}

int insert_map_nosign(top_map* m,const char* key, const char* value, size_t val_len,char sign) {
    
    char * tk,*tv;
    size_t len;
    if(!key || !value || !m)
        return -1;

    len = strlen(key);
    tk = (char*) malloc(sizeof(char)*len+1);
    memmove(tk, key, len);
    tk[len] = '\0';
    
    tv = (char*) malloc(sizeof(char)*val_len+1);
    memmove(tv, value, val_len);
    tv[val_len] = '\0';

    m->total_length += len+val_len;
    
    m->keys[m->ite] = tk;
    m->values[m->ite] = tv;
    m->ite = m->ite + 1;
    
    return m->ite;
}

int insert_map(top_map* m,const char* key, const char* value){
    if(key == NULL || value == NULL){
        return -1;
    }
    return insert_map_nosign(m,key,value,strlen(value),1);
}

void destrop_map(top_map* m){
    if(m){
        int i ;
        for(i = 0; i < m->ite; i++){
            free(m->keys[i]);
            free(m->values[i]);
        }
        free(m->keys);
        free(m->values);
        free(m);
    }
}

void quiksort_(top_map* m,int low,int high)
{
    int i = low;
    int j = high;
    char** keys = m->keys;
    char** values = m->values;
    char* tempKey = keys[i];
    char* tempValue = values[i];
    if( low < high)
    {
        while(i < j)
        {
            while((strcmp(keys[j], tempKey) >= 0) && (i < j))
            {
                j--;
            }
            keys[i] = keys[j];
            values[i] = values[j];
            
            while((strcmp(keys[i], tempKey) < 0) && (i < j))
            {
                i++;
            }
            keys[j]= keys[i];
            values[j] = values[i];
        }
        keys[i] = tempKey;
        values[i] = tempValue;
        
        quiksort_(m,low,i-1);
        quiksort_(m,j+1,high);
    }
    else
    {
        return;
    }
}

void quiksort_by_key(top_map* m){
    quiksort_(m,0,m->ite - 1);
}

pTopRequest alloc_top_request(){
    pTopRequest request = (pTopRequest)malloc(sizeof(TopRequest));
    request->httpHeaders = NULL;
    request->params = NULL;
    request->files = NULL;
    request->apiName = NULL;
    request->url = NULL;
    return request;
}

void destroy_top_request(pTopRequest pt){
    if(pt){
        if(pt->params)
            destrop_map(pt->params);
        if(pt->files)
            destrop_map(pt->files);
        if(pt->httpHeaders)
            destrop_map(pt->httpHeaders);
        if(pt->apiName)
            free(pt->apiName);
        free(pt);
    }
}

int add_param(pTopRequest pt,const char* key, const char* value){
    if(!pt)
        return 1;
    if(!pt->params)
        pt->params = alloc_map();
    insert_map(pt->params, key, value);
    return 0;
}

#include <time.h>
extern struct tm* ag_os_get_localtime(const time_t *timep);

int compeleteSystemParams(pTopRequest pRequest, char* appkey, char* appsecret, char* session, unsigned char* sign)
{
    char* buffer;
    int i;
    unsigned long total_length = 0;
    struct tm* timeinfo = NULL;
    time_t sec;

    if(sign == NULL)
        return -1;
  
    char timestamps[32]; 
    timeinfo = ag_os_get_localtime(NULL);
    strftime(timestamps, sizeof(timestamps), "%F %T", timeinfo);
    
    add_param(pRequest, "timestamp", timestamps);
    add_param(pRequest, "format", "json");
    add_param(pRequest, "sign_method", "md5");
    add_param(pRequest, "session", session);
    add_param(pRequest, "v", "2.0");
    add_param(pRequest, "app_key", appkey);
    add_param(pRequest, "partner_id", "topsdkcpp");    
    
    quiksort_by_key(pRequest->params);
    total_length = pRequest->params->total_length + 2*strlen(appsecret) + 1;

    buffer = (char*)calloc(total_length, sizeof(char));
    if(buffer == NULL)
    {
        ESP_LOGE(TAG, "Malloc failed for Toprequest");
    }
    
    strcat(buffer, appsecret);
    for(i = 0; i != pRequest->params->ite ; i ++ )
    {
        strcat(buffer,pRequest->params->keys[i]);
        strcat(buffer,pRequest->params->values[i]);
    }
    strcat(buffer, appsecret);

    checksumMd5((unsigned char*)buffer, strlen(buffer), sign);

    free(buffer);
    return 0;
}

//https://www.rosettacode.org/wiki/URL_encoding#C
 
/* caller responsible for memory */
char* url_encoder_html_tables_init()
{
    int i;
    char *html5 = NULL;
    
#define SIZE 256

    html5 = (char*)calloc(SIZE, sizeof(char));
    for (i = 0; i < SIZE; i++)
    {
        //rfc3986[i] = isalnum( i) || i == '~' || i == '-' || i == '.' || i == '_' ? i : 0;
        html5[i] = isalnum( i) || i == '*' || i == '-' || i == '.' || i == '_' ? i : (i == ' ') ? '+' : 0;
    }

    return html5;
}

char *url_encode(char *table, char *s, char *enc)
{
    for (; *s; s++)
    {
        if(table[(unsigned char)*s]) 
            sprintf(enc, "%c", table[(unsigned char)*s]);
        else 
            sprintf(enc, "%%%02X", *s);
        
        while (*++enc);
    }
    
    *++enc = "\0";
    return(enc);
}

pTopRequest gen_url_sign(char* schema, char* userid, char* appkey, char* appsecret, unsigned char* sign)
{
    pTopRequest pRequest = alloc_top_request();
   
    add_param(pRequest, "method",  "taobao.ailab.aicloud.top.device.authcode.get");
    add_param(pRequest, "schema",  schema);
    add_param(pRequest, "user_id", userid);
    add_param(pRequest, "utd_id",  "V944n0HrnZ4DAAtOStvvDat7");
    add_param(pRequest, "ext",     "{\"appType\":\"IOS\",\"appVersion\":\"1.0\"}");
    add_param(pRequest, "appkey",  appkey);

    compeleteSystemParams(pRequest, appkey, appsecret, NULL, sign);

    return pRequest;
}


char* jsonParseAuthCode(const char* authcode_get_response)
{
    char* authcode = NULL;

    cJSON *recv_obj = cJSON_Parse(authcode_get_response);
    if (recv_obj == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Protocal parse error before: %s\n", error_ptr);
        }
        goto fail;
    }

    char *formattedStr = cJSON_Print(recv_obj);
    if(formattedStr == NULL) {
        ESP_LOGE(TAG, "Protocal parse failed to print formattedStr\n");
    } else {
        ESP_LOGE(TAG, "Response_Buf:\n%s\n", formattedStr);
        cJSON_free(formattedStr);
    }

    if(recv_obj) {
        cJSON *top_api = cJSON_GetObjectItem(recv_obj, "ailab_aicloud_top_device_authcode_get_response");
        cJSON * cmd_obj = cJSON_GetObjectItem(top_api, "model"); 

        if(cmd_obj != NULL) {
            char *model = cmd_obj->valuestring;
            if(model == NULL) {
                ESP_LOGE(TAG, "Receive model is NULL !\n");
                goto fail;
            }

            authcode = strdup(model);
        } 
    }

fail:
    cJSON_Delete(recv_obj);
    return authcode;
}

char* genAuthoCodeUrl(const char *appkey, const char *userid, char* appsecret, char* schema)
{
    unsigned char sign[32+1] = {0};
    char url_encoded[256] = {0};
    char *html5 = NULL;
    char *url = NULL;
    pTopRequest pRequest = NULL;
    int len = 0;
    char* httpurl = "http://gw.api.taobao.com/router/rest";

    url = (char*)calloc(512, sizeof(char));
    html5 = url_encoder_html_tables_init();

    pRequest = gen_url_sign(schema, userid, appkey, appsecret, sign);
    len = sprintf(url, "%s?sign=%s", httpurl, sign);

    for(short k=0; k<pRequest->params->ite; k++) 
    {
        url_encode(html5, pRequest->params->values[k], url_encoded);
        len += sprintf(&url[len], "&%s=%s", pRequest->params->keys[k], url_encoded);
    }
    free(html5);
    return url;
}

char *getAuthCodeFromTop(const char *appkey, const char *userid, const char* appsecret, const char* schema)
{
    char *authcode = NULL;
    char data[256] = {0};
    char *url = genAuthoCodeUrl(appkey, userid, appsecret, schema);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&config);

    if (esp_http_client_open(http_client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Error open http request to authcode from TOP");
        goto _exit;
    }

    esp_http_client_fetch_headers(http_client);

    int read_len = esp_http_client_read(http_client, data, 256);
    if (read_len <= 0) {
        ESP_LOGE(TAG, "Read length error");
    }
    data[read_len] = '\0';

    authcode = jsonParseAuthCode(data);
    if (authcode == NULL) {
        ESP_LOGE(TAG, "Filed to get authcode from TOP");
    }

    _exit:
    vPortFree(url);
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
    return authcode;
}


