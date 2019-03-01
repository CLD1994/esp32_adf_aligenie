
#include "litewebsocket.h"
#include "litews_thread.h"
#include "litews_memory.h"
//#include "litews_common.h"
#include "litews_socket.h"
#include <assert.h>
#include "litews_log.h"
#include "aligenie_os.h"

#include "freertos/timers.h"

typedef AG_TASK_T rtos_pthread_t;
typedef AG_TIMER_T rtos_timer_t;


struct litews_thread_struct 
{
    litews_thread_funct thread_function;
    void * user_object;
    rtos_pthread_t thread;
    rtos_timer_t timer;
};

typedef struct _litews_threads_joiner_struct 
{
    litews_thread thread;
    litews_mutex mutex;
} _litews_threads_joiner;

static _litews_threads_joiner * _threads_joiner = NULL;


static void litews_threads_joiner_create_ifneed(void) 
{
    if (_threads_joiner) 
    {
        return;
    }
    _threads_joiner = (_litews_threads_joiner *)litews_malloc_zero(sizeof(_litews_threads_joiner));
    _threads_joiner->mutex = litews_mutex_create_recursive();
}


void litews_threads_joiner_free_ifneed(void)
{
    LOGD_LITEWS("%s", __FUNCTION__);
    if (!_threads_joiner)
    {
        return;
    }
    litews_mutex_delete(_threads_joiner->mutex);
    litews_free(_threads_joiner);
    _threads_joiner = NULL;
}

static void * litews_thread_func_priv(void * some_pointer)
{
    litews_thread t = (litews_thread)some_pointer;

    rtos_pthread_t *thread = &t->thread;
    t->thread_function(t->user_object);

    ag_os_timer_stop(&t->timer);
    ag_os_timer_destroy(&t->timer);

    //t->thread = NULL;
    litews_free(t);
    t = NULL;

    litews_threads_joiner_free_ifneed();
    ag_os_task_destroy(thread);
    return NULL;
}

static void * litews_timer_func_priv(void * TimerHandle)
{
    litews_thread t = (litews_thread)pvTimerGetTimerID(TimerHandle);
    litews_socket s = (litews_socket)(t->user_object);

    if (s->command == COMMAND_IDLE 
        && s->is_connected == litews_true) 
    {
        litews_socket_send_ping(s);
        litews_socket_idle_send(s);
    }
    
    return NULL;
}


litews_thread litews_thread_create(litews_thread_funct thread_function, void * user_object)
{
    litews_thread t = NULL;

    if (!thread_function) 
    {
        return NULL;
    }

    litews_threads_joiner_create_ifneed();
    t = (litews_thread)litews_malloc_zero(sizeof(struct litews_thread_struct));
    t->user_object = user_object;               //socket param
    t->thread_function = thread_function;       //thread function

    /* create timer to send ping message */
    ag_os_timer_create(&t->timer, 
                        20*1000, 
                        true, 
                        litews_timer_func_priv, 
                        (void *)t);
    
    ag_os_timer_start(&t->timer);
    
    ag_os_task_create(&t->thread, 
                        "ws_thread", 
                        litews_thread_func_priv, 
                        AG_TASK_PRIORITY_NORMAL, 
                        1024*10, 
                        (void *)t);
                        
    return t;
}


void litews_thread_sleep(const unsigned int millisec) 
{
    ag_os_task_mdelay(millisec);
}

litews_mutex litews_mutex_create_recursive(void) 
{
    AG_MUTEX_T mutex = NULL;
    ag_os_task_mutex_init(&mutex);
    return mutex;
}

void litews_mutex_lock(litews_mutex mutex) 
{
	if (mutex) 
	{
        ag_os_task_mutex_lock(&mutex);
	}
}

void litews_mutex_unlock(litews_mutex mutex) 
{
	if (mutex) 
	{
        ag_os_task_mutex_unlock(&mutex);
	}
}

void litews_mutex_delete(litews_mutex mutex) 
{
	if (mutex) 
	{
        ag_os_task_mutex_delete(&mutex);
	}
}

