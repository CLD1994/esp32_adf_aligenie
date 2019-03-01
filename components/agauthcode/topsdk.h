#ifndef TopSDKC_h
#define TopSDKC_h

#ifdef __cplusplus
# define TOP_CPP_START extern "C" {
# define TOP_CPP_END }
#else
# define TOP_CPP_START
# define TOP_CPP_END
#endif

//TOP_CPP_START

typedef struct SinlgeMap {
    unsigned char ite;
    char size;
    unsigned short total_length;
    char** keys;
    char** values;
}top_map;

/*request for execure*/
typedef struct TopRequest {
    long timestamp; 
    char* url; 
    char* apiName;
    top_map* httpHeaders;
    top_map* params; /*normal params*/
    top_map* files;  /*file params*/
}TopRequest,*pTopRequest;

typedef struct TopResponse {
    int code;
    char* msg;
    char* subCode;
    char* subMsg;
    char* requestId;
    top_map* results;
    char* bytes;
    int len;
} TopResponse,*pTopResponse;

typedef struct ResultItem{
    char* key;
    char* value;
}ResultItem,*pResultItem;

typedef struct TopResponseIterator {
    int cur_index;
    pTopResponse pResult;
}TopResponseIterator,*pTopResponseIterator;

typedef struct TaobaoClient {
    char* url;
    char* appkey;
    char* appsecret;
}TaobaoClient,*pTaobaoClient;

/*Top Request*/
pTopRequest alloc_top_request();
void destroy_top_request(pTopRequest pt);

/*add extra http params to request*/
int add_httpheader_param(pTopRequest pt,const char* key, const char* value);
/*add api normal params to request*/
int add_param(pTopRequest pt,const char* key, const char* value);
/*add api files params to request*/
int add_file_param(pTopRequest pt,const char* key, const char* value);
/*set api names for request*/
int set_api_name(pTopRequest pt,const char* name);


/*Top Response*/
pTopResponse alloc_top_response();

void destroy_top_response(pTopResponse pt);

/*init a iterator for response*/
pTopResponseIterator init_response_iterator(pTopResponse pResult);
/*init a response item for iterator*/
pResultItem alloc_result_item();
/*return 0 if has more items, result hold on pResultItem*/
int parseNext(pTopResponseIterator ite,pResultItem resultItem);

void destroy_result_item(pResultItem pResult);

void destroy_response_iterator(pTopResponseIterator ite);

void destroy_taobao_client(pTaobaoClient pClient);


/*top client*/
pTaobaoClient alloc_taobao_client(const char* url,const char* appkey,const char* secret);
/*execute request for response (sync)
if the request don't nedd session , put NULL*/
TopResponse* top_execute(pTaobaoClient pClient,pTopRequest request,char* session);

/*GLOBAL CONFIG*/

/*try retrt http request when curl failed .default value is 0*/
void set_retry_times(int retry);

/*default value is 50 ,unit:ms*/
void set_retry_sleep_times(int sleep_time);

/*default value is 15 unit:s*/
void set_http_time_out(int timeout);

/*set cap path*/
void set_capath(char* path);

//TOP_CPP_END

#endif
