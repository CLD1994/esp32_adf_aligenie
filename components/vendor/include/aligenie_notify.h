/*AliGenie SDK for FreeRTOS header*/
/*Ver.20180524*/

#ifndef _ALIGENIE_NOTIFY_HEADER_
#define _ALIGENIE_NOTIFY_HEADER_

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * �豸�˽ӵ����������Ѻ���ô˺��������п�����һЩ������ʾ��ָʾ�Ʋ�����
 * ע�⣺�˺�����Ҫ��ʱ��������
 */
extern void ag_notify_new_voice_msg();

/**
 * �豸�յ�URL���͵�pushָ�����ô˺�����
 * ע�⣺�˺�����Ҫ��ʱ��������
 */
extern void ag_notify_push_command(const char * const cmd);

/**
 * �豸������ɺ���ô˺�����
 * ע�⣺�˺�����Ҫ��ʱ��������
 */
extern void ag_notify_finish_init();

/**
 * �豸���յ����˳���ָ�����ô˺�����
 * ������Ϊ�����Զ��塣
 * ע�⣺�˺�����Ҫ��ʱ��������
 */
extern void ag_notify_cmd_exit();

/**
 * botid����͸��nlu�����nlu�������ָ���Ӵ˺���͸����
 * ע�⣺�˺�����Ҫ��ʱ��������
 */
extern void ag_notify_nlu_result(const char * const cmd);


/**
 * �����Բ�������ɡ�TTSָ���յ�ʱ�Ļص���ע���ʱ����TTS��δ����
 */
extern void ag_notify_voice_msg_about_to_play_finish();

/**
 * error code from AliGenie server.
 */
extern void ag_notify_server_error(int errorCode);

#ifdef __cplusplus
}
#endif

#endif /*#define _ALIGENIE_NOTIFY_HEADER_*/