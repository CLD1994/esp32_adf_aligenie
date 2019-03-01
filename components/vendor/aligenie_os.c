/*Aligenie SDK OS porting interfaces*/
/*Ver.20150509*/

#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <nvs.h>

#include "aligenie_libs.h"
#include "aligenie_os.h"
#include <freertos/FreeRTOS.h>
#include "freertos/event_groups.h"
#include <freertos/queue.h>
// #include "aglog.h"
#include "esp_system.h"
#include "esp_log.h"



#define LOG_TAG "AliGenie"


typedef unsigned char uint8;
typedef unsigned int uint32;

/************************* MEMORY *************************/

/*return: free heap size in bytes*/
int ag_os_get_free_heap_size()
{
    int size = xPortGetFreeHeapSize();
    return size;
}

/************************* CONFIG *************************/


/**
 * Read sdk config.
 * This config is necessary for Aligenie to run.
 * Config data need to be burned-in by factory.
 * And these data should be able to be read through this function from aligenie sdk.
 *
 * params
 *     buf: data read. Max length is AG_OS_SDK_CONFIG_MAX_LENGTH
 *
 * return
 *     zero or a possitive value for read bytes,
 *     or a negative value for fail.
 *
 * ATTENTION: If it returns fail, sdk will retry forever, because if not, this device cannot be used.
 */
int ag_os_read_sdk_config(char * buf)
{
    strcpy(buf, "mFapKOmK fvYZkOLh 638A1B4AD2E2F330C4ABAF94EBF64CC2 AI_Test ESP32 AG_KIDS_STORY FreeRTOS EsBsb3DbEVviFXQooXbQ3vX8Q0pfEQXoEpX06v8HpF");
    return 1;
}
/************************* SDCARD *************************/

/************************ FW ver **************************/

/**
 * get firmware version, this version will be sent to ali server.
 *
 * Recommend format: 
 *     1.0.0-R-20180605.1118
 *
 * return:
 *     ptr of char, to a version string.
 *     Can be a string constant, but DO NOT RETURN NULL.
 */

int ag_os_get_firmware_version(char * firmware_version)
{
    static char version_flash[47+1] = {0};
    
    int ret = ag_os_flash_read(KEY_FIRMWARE_VERSION, version_flash, sizeof(version_flash));
    if(ret == ESP_ERR_NVS_NOT_FOUND) 
    {
        ESP_LOGW(LOG_TAG, "No valid firmware version stored before......");
        goto update;
    } 
    else if(ret == ESP_OK) 
    {
        if(strcmp(FIRMWARE_VERSION, version_flash) > 0) {
            ESP_LOGW(LOG_TAG, "Current version is bigger than flash version......");
            goto update;
        }
        else {
            version_flash[strlen(FIRMWARE_VERSION)] = '\0';
            strcpy(firmware_version, version_flash);
            return 0;
        } 
    }
    else {
        strcpy(firmware_version, "0.0.0-I-197000101.0000");
        return ret;
    }

update:
    ag_os_flash_write(KEY_FIRMWARE_VERSION, FIRMWARE_VERSION, strlen(FIRMWARE_VERSION));
    strcpy(firmware_version, FIRMWARE_VERSION);
    return 0;
}

/********************* Encryption *************************/
static void md5_result_dump(uint8_t *result, uint8_t length)
{
    uint8_t i;

    for (i = 0; i < length; i++) {
        if (i % 16 == 0) {
            printf("\r\n");
        }

        printf(" %02x ", result[i]);
    }
    printf("\r\n");

}

static void sha_result_dump(uint8_t *result, uint8_t length)
{
#ifdef HAL_SHA_DEBUG
    uint8_t i;

    for (i = 0; i < length; i++) {
        if (i % 16 == 0) {
            printf("\r\n");
        }

        printf(" %02x ", result[i]);
    }
    printf("\r\n");

#endif

}

 
/************************* FLASH **************************/
#include "nvs.h"
#define NVS_HANDLE_INVALID      (0xDEADBEEF)
static nvs_handle handle_agsdk = NVS_HANDLE_INVALID;

int ag_os_flash_init()
{
    esp_err_t ret;

    ret = nvs_open("agsdk", NVS_READWRITE, &handle_agsdk);
    if(ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to open agsdk NVS flash with result[%d]", ret);
        return ret;
    }
    return ret;
}

/** 
 * params outval: DO NOT add \0 at outval[len]
 * return 
 *   success: 0
 *   fail: a negative value
 */
int ag_os_flash_read(const char * const key, const char * outval, size_t len)
{
    esp_err_t ret;

    if(handle_agsdk == NVS_HANDLE_INVALID) {
        ret = ag_os_flash_init();
        if(ret != ESP_OK){
            return ret;
        }
    }

    size_t get_len = len + 1;
    
    return nvs_get_str(handle_agsdk, key, outval, &get_len);
}

/**
 * return
 *   success: 0
 *   fail: a negative value
 */
int ag_os_flash_write(const char * const key, const char * const inval, size_t len)
{
    esp_err_t ret;

    if(handle_agsdk == NVS_HANDLE_INVALID) {
        ret = ag_os_flash_init();
        if(ret != ESP_OK){
            return ret;
        }
    }

    return nvs_set_str(handle_agsdk, key, inval);
}

/** 
 * params outval: DO NOT add \0 at outval[len]
 * return 
 *   success: 0
 *   fail: a negative value
 */
int ag_os_flash_read_integer(const char * const key, int * outval)
{
    esp_err_t ret;

    if(handle_agsdk == NVS_HANDLE_INVALID) {
        ret = ag_os_flash_init();
        if(ret != ESP_OK){
            return ret;
        }
    }
    
    return nvs_get_i32(handle_agsdk, key, outval);
}

/**
 * return
 *   success: 0
 *   fail: a negative value
 */
int ag_os_flash_write_integer(const char * const key, int inval)
{
    esp_err_t ret;

    if(handle_agsdk == NVS_HANDLE_INVALID) {
        ret = ag_os_flash_init();
        if(ret != ESP_OK){
            return ret;
        }
    }

    return nvs_set_i32(handle_agsdk, key, inval);
}


/************************* TASK ***************************/
int ag_os_task_create(AG_TASK_T *task_handler,
                             const char *name,
                             ag_os_task_runner func,
                             AG_TASK_PRIORITY_E priority,
                             int max_stack_size,
                             void * arg)
{
    BaseType_t ret;
    UBaseType_t prio = priority;

    ret = xTaskCreate((TaskFunction_t)func, name, max_stack_size/sizeof(portSTACK_TYPE), arg, prio, (TaskHandle_t)(*task_handler));
    if(pdPASS != ret)
        ESP_LOGE(LOG_TAG, "Failed to create task[%s] ... ", name);
    else
        ESP_LOGW(LOG_TAG, "Success to creat task[%s], threadId=%p, priority=%d, stack size=%d",name,*task_handler, prio, max_stack_size);

    return (pdPASS == ret)? AG_OS_TASK_RET_OK : AG_OS_TASK_RET_FAIL;

}

int ag_os_task_destroy(AG_TASK_T * task_handler)
{
    TaskHandle_t handle =  xTaskGetCurrentTaskHandle();

    if(task_handler == NULL)
        vTaskDelete(handle);
    else
        vTaskDelete(*task_handler);

    return 0;
}

/*mutex lock*/
void ag_os_task_mutex_init(AG_MUTEX_T *mutex)
{
    xSemaphoreHandle lock = xSemaphoreCreateMutex();
    *mutex = lock;
}

void ag_os_task_mutex_lock(AG_MUTEX_T *mutex)
{
    BaseType_t ret;

    if(*mutex == NULL) {
        ESP_LOGE(LOG_TAG, "mutex handle error for lock");
        return;
    }
        
    ret = xSemaphoreTake((SemaphoreHandle_t)(*mutex), portMAX_DELAY);
    if(pdTRUE != ret)
        ESP_LOGE(LOG_TAG, "mutex lock error : mutexHandle=%p", *mutex);
}

void ag_os_task_mutex_unlock(AG_MUTEX_T *mutex)
{
    BaseType_t ret;

    if(*mutex == NULL) {
        ESP_LOGE(LOG_TAG, "mutex handle error for unlock");
        return;
    }
        
    ret = xSemaphoreGive((SemaphoreHandle_t)(*mutex));
    
    if(pdTRUE != ret)
        ESP_LOGE(LOG_TAG, "mutex unlock fail : mutexHandle=%p", *mutex);
}

void ag_os_task_mutex_delete(AG_MUTEX_T * mutex)
{
    if(*mutex == NULL) {
        ESP_LOGE(LOG_TAG, "mutex handle error for delete");
        return;
    }

    vSemaphoreDelete((SemaphoreHandle_t)(*mutex));
}

/*cond lock*/
void ag_os_task_cond_init(AG_COND_T * cond)
{
    BaseType_t ret;

    if(*cond == NULL) {
        ESP_LOGE(LOG_TAG, "ag_os_task_cond_init error");
        return;
    }

    *cond = xSemaphoreCreateBinary();
    ret = (*cond != NULL) ? 0 : -1;
    if (-1 == ret)
        ESP_LOGE(LOG_TAG, "condition init fail\n");

}
void ag_os_task_cond_wait(AG_COND_T * cond)
{
    BaseType_t ret;

    if(*cond == NULL) {
        ESP_LOGE(LOG_TAG, "ag_os_task_cond_wait error");
        return;
    }

    ret = xSemaphoreTake(*cond, portMAX_DELAY);
    if (pdTRUE != ret)
        ESP_LOGE(LOG_TAG, "condition wait error : condHandle=%p\n", *cond);
}
void ag_os_task_cond_signal(AG_COND_T * cond)
{
    BaseType_t ret;

    if(*cond == NULL) {
        ESP_LOGE(LOG_TAG, "ag_os_task_cond_signal error");
        return;
    }

    ret = xSemaphoreGive(*cond);
    if (pdTRUE != ret)
        ESP_LOGE(LOG_TAG, "condition signal fail : condHandle=%p\n", *cond);

}

/* task delay */
void ag_os_task_mdelay(uint32_t time_ms)
{
    vTaskDelay(pdMS_TO_TICKS(time_ms));
}

/*queue*/

int ag_os_queue_create(AG_QUEUE_T * handler, const char * name, uint32_t message_size, uint32_t number_of_messages)
{
    QueueHandle_t q_id = NULL;

    q_id = xQueueCreate(number_of_messages, message_size);
    *handler = (AG_QUEUE_T)q_id;
    return AG_OS_QUEUE_RET_OK;

}
int ag_os_queue_push_to_back(AG_QUEUE_T * handler, void * msg,  uint32_t timeout_ms)
{
    BaseType_t ret = 0;

    ret = xQueueSendToBack((QueueHandle_t)(*handler), msg, timeout_ms);
    if (pdFAIL != ret) {
        return AG_OS_QUEUE_RET_OK;
    } else {
        return AG_OS_QUEUE_RET_FAIL;
    }
}
int ag_os_queue_push_to_front(AG_QUEUE_T * handler, void * msg,  uint32_t timeout_ms)
{
    BaseType_t ret = 0;

    ret = xQueueSendToFront((QueueHandle_t)(*handler), msg, timeout_ms);
    if (pdFAIL != ret) {
        return AG_OS_QUEUE_RET_OK;
    } else {
        return AG_OS_QUEUE_RET_FAIL;
    }
}
int ag_os_queue_pop(AG_QUEUE_T * handler, void * msg,  uint32_t timeout_ms)
{
    BaseType_t ret = 0;

    ret = xQueueReceive((QueueHandle_t)(*handler), msg, timeout_ms);
    if (pdFAIL != ret) {
        return AG_OS_QUEUE_RET_OK;
    } else {
        return AG_OS_QUEUE_RET_FAIL;
    }
}
int ag_os_queue_destroy(AG_QUEUE_T * handler)
{
    BaseType_t ret = 0;
    vQueueDelete((QueueHandle_t)(*handler));
    return AG_OS_QUEUE_RET_OK;
}

#include "freertos/timers.h"

/**************************timer***********************/

int ag_os_timer_create(AG_TIMER_T * handler, uint32_t time_interval_ms, bool is_auto_reset, ag_os_timer_callback func, void * arg)
{
    *handler = xTimerCreate("agtimer",
                            pdMS_TO_TICKS(time_interval_ms), 
                            is_auto_reset,
                            arg,
                            func);

    if (*handler == NULL) {
        ESP_LOGE(LOG_TAG, "ag_os_timer_create create fail.\r\n");
        return AG_OS_TIMER_FAIL;
    }
    else
    {
        ESP_LOGE(LOG_TAG, "ag_os_timer_create create success.\r\n");
        return AG_OS_TIMER_OK;
    }
}
int ag_os_timer_start(AG_TIMER_T * handler)
{
    BaseType_t ret = 0;
    ret = xTimerStart(*handler, 0);
    if (pdFAIL != ret) {
        return AG_OS_TIMER_OK;
    } else {
        return AG_OS_TIMER_FAIL;
    }
}
int ag_os_timer_stop(AG_TIMER_T * handler)
{
    BaseType_t ret = 0;
    ret = xTimerStop(*handler, 0);
    if (pdFAIL != ret) {
        return AG_OS_TIMER_OK;
    } else {
        return AG_OS_TIMER_FAIL;
    }
}
int ag_os_timer_destroy(AG_TIMER_T * handler)
{
    BaseType_t ret = 0;
    if (handler != NULL) {
        ret=xTimerDelete(*handler, 0);
        *handler = NULL;
    }
    if (pdFAIL != ret) {
        return AG_OS_TIMER_OK;
    } else {
        return AG_OS_TIMER_FAIL;
    }
}

 AG_OS_AUTO_VAD_TYPE_E ag_os_get_auto_vad_type(){
    return AG_OS_AUTO_VAD_OFF;
 }

 bool ag_os_is_guest_enabled() {
    return false;
 }
 
int ag_os_get_mac_address(char * mac) 
{
    unsigned char efuse_mac[6];
    esp_read_mac(efuse_mac, ESP_MAC_WIFI_STA);
    int len = sprintf(mac, "%02x:%02X:%02X:%02x:%02X:%02X", efuse_mac[0], efuse_mac[1], 
                      efuse_mac[2], efuse_mac[3], efuse_mac[4], efuse_mac[5]);
    mac[len] = '\0';
    return 0;
}

int ag_os_get_mac_address_raw(char * mac) 
{
    unsigned char efuse_mac[6];
    esp_read_mac(efuse_mac, ESP_MAC_WIFI_STA);
    int len = sprintf(mac, "uranus_%02x%02X%02X%02x%02X%02X", efuse_mac[0], efuse_mac[1], 
                      efuse_mac[2], efuse_mac[3], efuse_mac[4], efuse_mac[5]);
    mac[len] = '\0';
    return 0;
}

 
void *AG_OS_MALLOC(unsigned int len) {
    void *data = NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    data = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    data = malloc(len);
#endif
    return data;
}


void *AG_OS_CALLOC(size_t len, unsigned int size) {
    void *data = NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    data = heap_caps_calloc(len, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    data = calloc (len, size);
#endif
    return data;
}


void *AG_OS_REALLOC(void *ptr, unsigned int newsize) {
    void *data = NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    data = heap_caps_realloc(ptr, newsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    data = realloc (ptr, newsize);
#endif
    return data;
}

void AG_OS_FREE(void * ptr)
{
    if(ptr){
 #if CONFIG_SPIRAM_BOOT_INIT       
        heap_caps_free(ptr); 
 #else
        vPortFree(ptr);
 #endif
    }  
    ptr = NULL;
}

#define AG_OS_MEMSET(ptr, value, len)  memset(ptr, value, len)
#define AG_OS_MEMCPY(dest, src, len)   memcpy(dest, src, len)


/************************* CONFIG *************************/
unsigned int ag_os_get_feature_config() {
    return AG_FEATURE_VOICE_MESSAGE | AG_FEATURE_VOLUME_CONTROL;
}

/************************* SDCARD *************************/
/**
 * get file stat from sdcard.
 *
 * Params filename NOT INCLUDE the path prefix of "sdcard" node.
 *
 * For example:
 *     There is a directory named "system" in the SD card root,
 *     and a file "a.txt" in that directory,
 *     the parameter of filename will be: "system/a.txt"
 *
 * retrun
 *     zero or a possitive value for read bytes,
 *     or a negative value for fail.
 */

int ag_os_read_sdcard_file(const char * filename, char * buf, int bufLen)
{
    return 0;
}

/********************* Encryption *************************/

/**
 * Calculate MD5
 * return 0 for success, other for fail.
 */
extern void ag_md5bin(const void* dat, size_t len, unsigned char out[16]);

int ag_os_generate_md5(unsigned char *out, const unsigned char *data, unsigned int datalen) {
    int ret = 0;
    ag_md5bin(data, datalen, out);
    return ret;
}

/**
 * Calculate sha256
 *
 * Params
 *     out: encrypt result
 *     data: source data
 *     key: sha256 encrypt key
 *
 * return
 *     0 for success,
 *     other for fail.
 */
#include "crypto/common.h"
#include "crypto/sha256.h"

int ag_os_generate_hmac_sha256(unsigned char *out, const unsigned char *data, unsigned int datalen, const unsigned char *key, unsigned int keylen) {
    int ret = 0;
    hmac_sha256(key, keylen, data, datalen, out);
    return ret;
}

/************************* LOG ****************************/

/************************* TASK ***************************/
bool ag_os_queue_is_empty(AG_QUEUE_T *handler) {
    return xQueueIsQueueEmptyFromISR (*handler);
}

bool ag_os_queue_is_full(AG_QUEUE_T *handler) {
    return xQueueIsQueueFullFromISR (*handler);
}

/*device control*/
int ag_os_device_standby() {
    return 0;
}

int ag_os_device_shutdown() {
    return 0;
}

/* AG log */
static AG_OS_LOGLEVEL_E _ag_log_level = AG_OS_LOGLEVEL_NONE;

void ag_os_log_set_level(AG_OS_LOGLEVEL_E level) {
    _ag_log_level = level;
}

void ag_os_log_print(const char *tag,
                     AG_OS_LOGLEVEL_E loglevel,
                     const char* format,
                     ...) 
{
#if 0
    va_list ap;

    va_start(ap, format);
    vprintf (format, ap);
    va_end(ap);
    printf ("\r\n");
#endif
}

/************************** TIME **************************/
#include <sys/time.h>
#include <time.h>

/* AG timer APIs*/
#define UTC_CHINA_OFFSET_SECONDS (8*3600)

/**
 * same as standard time_t time(time_t *t)
 */
time_t ag_os_get_time(time_t *t) {
    time_t t_ret = time (NULL);
    //t_ret += UTC_CHINA_OFFSET_SECONDS;
    if (t) {
        *t = t_ret;
    }
    return t_ret;
}

/**
 * get local time, same as struct tm *localtime(const time_t *timep)
 */
struct tm *ag_os_get_localtime(const time_t *timep) 
{
    time_t t_ret = time (NULL);    
    return localtime(&t_ret);
}

/**
 * get UTC time, same as struct tm *gmtime(const time_t *timep)
 */
struct tm *ag_os_get_gmtime(const time_t *timep) 
{
    time_t t_ret = time (NULL);
    
    return gmtime(&t_ret);
}


/**
 * same as int gettimeofday(struct timeval *restrict tp, void *restrict tzp)
 * param tzp will always be NULL for now.
 */
int ag_os_gettimeofday(struct timeval *restrict tp, void *restrict tzp) 
{
    gettimeofday (tp, tzp);
    return 0;
}

int rtos_create_thread(AG_TASK_T *thread, AG_TASK_PRIORITY_E priority, const char *name, ag_os_task_runner function, int stack_size, void *arg) {
    /* Limit priority to default lib priority */
    if (priority > configMAX_PRIORITIES - 1) {
        priority = configMAX_PRIORITIES - 1;
    }
    if (pdPASS == xTaskCreate (function, name, (unsigned short) (stack_size / sizeof (portSTACK_TYPE)), arg, priority, thread)) {
        return 0;
    } else {
        return -1;
    }
}

/************************* TASK ***************************/
int ag_os_eventgroup_create(AG_EVENTGROUP_T *handler)
{
    *handler = xEventGroupCreate();
    return 0;
}

int ag_os_eventgroup_delete(AG_EVENTGROUP_T *handler)
{
    vEventGroupDelete(*handler);
    return 0;
}

int ag_os_eventgroup_set_bits(AG_EVENTGROUP_T *handler, int bits)
{
    return xEventGroupSetBits(*handler, bits);
}

int ag_os_eventgroup_clear_bits(AG_EVENTGROUP_T *handler, int bits)
{
     return xEventGroupClearBits(*handler, bits);
}
    
int ag_os_eventgroup_wait_bits(AG_EVENTGROUP_T *handler, int bits, int isClearOnExit, int isWaitForAllBits, int waitMs)
{
    return xEventGroupWaitBits(*handler, bits, isClearOnExit, isWaitForAllBits, pdMS_TO_TICKS(waitMs));
}


//#endif /*_ALIGENIE_PORTING_OS_HEADER_*/
