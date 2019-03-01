
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ag_notify_new_voice_msg() {

}

void ag_notify_push_command(const char * const cmd) {

}

void ag_notify_finish_init(){

 }
void ag_notify_cmd_exit(){

}

void ag_update_devicemodule(int errorcode)
{
    
}

void ag_notify_voice_msg_about_to_play_finish()
{

}

void ag_notify_nlu_result(const char * const cmd)
{

}

void OSTaskMSleep(int msec) 
{ 
    vTaskDelay(pdMS_TO_TICKS(msec));
}

void ag_notify_server_error(int errorcode)
{
	
}

