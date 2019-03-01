
#ifndef _LIB_LITEWS_LOG_H_
#define _LIB_LITEWS_LOG_H_

#ifdef __cplusplus  
extern "C" {  
#endif  

#include "aligenie_os.h"
#include "esp_log.h"


//#define LITEWS_LOG_LEVEL_DEBUG  3
/*LOG输出级别, 意义上级别高于或等于(数值上小于等于)该级别的LOG都会被输出*/
//#define LITEWS_LOG_LEVEL                   LITEWS_LOG_LEVEL_DEBUG


/*LOG TAG*/
#ifndef LITEWS_LOG_TAG
#define LITEWS_LOG_TAG "ws"
#endif

#define LOG_LITEWS(...)     ag_os_log_print(__VA_ARGS__)

#define LOGE_LITEWS(...)    ESP_LOGE(LITEWS_LOG_TAG, __VA_ARGS__)
#define LOGW_LITEWS(...)    ESP_LOGW(LITEWS_LOG_TAG, __VA_ARGS__)
#define LOGD_LITEWS(...)    ESP_LOGW(LITEWS_LOG_TAG, __VA_ARGS__)
#define LOGV_LITEWS(...)    ESP_LOGV(LITEWS_LOG_TAG, __VA_ARGS__)

#define FUNCTION_BEGIN      LOGV_LITEWS("function %s line %d begin", __FUNCTION__, __LINE__);
#define FUNCTION_END        LOGV_LITEWS("function %s line %d end", __FUNCTION__, __LINE__);

#ifdef __cplusplus  
}  
#endif  

#endif /*_LIBAGLOG_H_*/

