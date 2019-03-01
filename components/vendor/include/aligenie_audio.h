#ifndef _ALIGENIE_PORTING_PLAYER_HEADER_
#define _ALIGENIE_PORTING_PLAYER_HEADER_

#ifdef __cplusplus
extern "C"
{
#endif


#include "aligenie_sdk.h"

/**
 * Maybe different EQ or gain could be used
 */
typedef enum {
    AG_AUDIO_RECORD_TYPE_RECOGNIZE, /* for asr */
    AG_AUDIO_RECORD_TYPE_MESSAGE,   /* for human-hear */
}AG_AUDIO_RECORD_TYPE_E;

/**
 * Local prompt res type
 */
typedef enum {
/*01*/    AG_LOCAL_PROMPT_ASR_EMPTY = 1,          //ASRʶ����Ϊ�գ���ʾ��˵һ��
/*02*/    AG_LOCAL_PROMPT_FORMAT_NOT_SUPPORT,     //��������֧�ֵ�ý���ʽ
/*03*/    AG_LOCAL_PROMPT_PLAYER_ERROR,           //����������
/*04*/    AG_LOCAL_PROMPT_NETWORK_DISCONNECTED,   //û����������
/*05*/    AG_LOCAL_PROMPT_PLAY_DOMAIN_MUSIC,      //���򲥷ţ�����
/*06*/    AG_LOCAL_PROMPT_PLAY_DOMAIN_STORY,      //���򲥷ţ�����
/*07*/    AG_LOCAL_PROMPT_PLAY_DOMAIN_SINOLOGY,   //���򲥷ţ���ѧ
/*08*/    AG_LOCAL_PROMPT_PLAY_DOMAIN_ENGLISH,    //���򲥷ţ�Ӣ��
/*09*/    AG_LOCAL_PROMPT_PLAY,                   //���Ű�����ʾ��
/*10*/    AG_LOCAL_PROMPT_PAUSE,                  //���Ű�����ʾ��
/*11*/    AG_LOCAL_PROMPT_PREVIOUS,               //��һ����ʾ��
/*12*/    AG_LOCAL_PROMPT_NEXT,                   //��һ����ʾ��
/*13*/    AG_LOCAL_PROMPT_RECORD_START,           //¼����ʼ��ʾ��
/*14*/    AG_LOCAL_PROMPT_RECORD_STOP,            //¼��������ʾ��
/*15*/    AG_LOCAL_PROMPT_AI_KEY_TOO_SHORT,       //AI�����¼�̫����ʾ
/*16*/    AG_LOCAL_PROMPT_PLAY_FAVORITE,          //play favorite list
/*17*/    AG_LOCAL_PROMPT_NOTHING_PLAYING,        //������ͣ������ʱû�����ڲ��Ż��ѱ���ͣ������
/*18*/    AG_LOCAL_PROMPT_SEEK_UNSUPPORTED,       //��֧�ֿ�����˵ı�����ʾ��
}AG_LOCAL_PROMPT_TYPE_E;

/**
 * Aligenie tts format.
 * Currently, mp3 format only.
 */
typedef enum {
    AG_AUDIO_TTS_FORMAT_MP3,
}AG_TTS_FORMAT_E;

/**
 * This function will be called while Aligenie has nothing to do.
 * Maybe play some local music, or just do nothing.
 */
extern void ag_audio_idle();

/**
 * play an url or a filepath.
 * Get info->url, and play it.
 * return:
 * 0: player init and/or playlist add success
 * other: error.
 */
extern int ag_audio_url_play(const AG_AUDIO_INFO_T * const info);

/* return: 0 for player-control send success, other for error. */
extern int ag_audio_url_pause();

/* return: 0 for player-control send success, other for error. */
extern int ag_audio_url_resume();

/* return: 0 for player-control send success, other for error. */
extern int ag_audio_url_stop();

/**
 * This function would be called only when url (or filepath) media is playing.
 * if it is called when playing prompt or url, just return an error.
 *
 * params
 *     outval: current progress of currently playing media, in seconds.
 *
 * return:
 *     0: success,
 *     other: error.
 */
extern int ag_audio_get_url_play_progress(const int * outval);

/**
 * get total length of current playing media, in seconds.
 */
extern int ag_audio_get_url_play_duration(const int * outval);

/* params seconds: target progress in seconds.*/
/* return: 0 for player-control send success, other for error. */
extern int ag_audio_url_seek_progress(int seconds);

extern AG_PLAYER_STATUS_E ag_audio_get_player_status();

/**
 * This call should BLOCK the caller if playing is started successfully.
 * Unblock caller when this prompt playing is finished or interrupted.
 *
 * IMPORTANT NOTES:
 * Calling this function should not clean up the resource of currently playing URL media.
 * In other words, url media play is just paused when playing prompt,
 * when prompt playing is finished or interrupted, Aligenie can still resume last url play.
 *
 * return:
 *     0 : finished(for block call)
 *     -1: interrupted (for block call)
 *     other: error.
 */
extern int ag_audio_play_prompt(char * url);

/**
 * play a local prompt indicated by type.
 * !!!BLOCK CALLER!!
 */
extern int ag_audio_play_local_prompt(AG_LOCAL_PROMPT_TYPE_E type);

/*
 * This func will only be called from ANOTHER TASK, different from the one
 * who is calling ag_audio_play_prompt.
 * Interrupt blocked call of ag_audio_play_prompt
 * If ag_audio_play_prompt is not a block call, this function should
 * return: 0 for success, other for error.
 */
extern int ag_audio_stop_prompt();
/*
 * params format will always be AG_AUDIO_TTS_FORMAT_MP3 for now.
 * return: 0 for success, other for error.
 */
extern int ag_audio_tts_play_start(AG_TTS_FORMAT_E format);
/*
 * params
 *     inData: A segment of mp3 data.
 *     dataLen: length of this segment.
 * return: 0 for success, other for error.
 */
extern int ag_audio_tts_play_put_data(const void * const inData, int dataLen);

/**
 * block caller and wait for tts play done
 * return 0 for play finished, return -1 for aborted.
 */
extern int ag_audio_tts_play_wait_finish();

/**
 * abort tts playing immediately and de-block caller of ag_audio_tts_play_wait_finish immediately.
 * return: 0 for successfully aborted. other for failed.
 */
extern int ag_audio_tts_play_abort();

/****************************RECORDER****************************/

typedef enum {
    AG_RECORDER_DATA_TYPE_PCM,
    AG_RECORDER_DATA_TYPE_SPEEX,
    AG_RECORDER_DATA_TYPE_OGG,
} AG_RECORDER_DATA_TYPE_E;

extern AG_RECORDER_DATA_TYPE_E ag_audio_get_record_data_type();

/* return: 0 for recorder-control send success, other for error. */
extern int ag_audio_record_start(AG_AUDIO_RECORD_TYPE_E type);

/* return: length of data read for success, zero or negative value for error.*/
extern int ag_audio_record_get_data(const void * outData, int maxLen);

/* return: 0 for recorder-control send success, other for error. */
extern int ag_audio_record_stop();
/****************************************************************/

/*****************************VOLUME*****************************/

/*
 * params:
 *     volume: integer value with range [0, 100]
 * return:
 *      0 for success, other for error.
 */
extern int ag_audio_set_volume(int volume);

/*
 * return:
 *      [0, 100] for current volume
 *      negative value for error
 */
extern int ag_audio_get_volume();
/****************************************************************/

/*
 * return:
 *      0 for success, other for error.
 */
extern int ag_audio_vad_start();

#ifdef __cplusplus
}
#endif

#endif /*_ALIGENIE_PORTING_PLAYER_HEADER_*/

