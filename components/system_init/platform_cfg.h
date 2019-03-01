#ifndef _URANUS_PLATFORM_CFG_H_
#define _URANUS_PLATFORM_CFG_H_



#if CONFIG_PLATFORM_CODEC_ES7148

#define AUDIO_IIS_BCK   (26)
#define AUDIO_IIS_WS    (25)
#define AUDIO_IIS_TX    (22)
#define AUDIO_IIS_RX    (35)

#elif CONFIG_PLATFORM_CODEC_ES8388

#define AUDIO_IIS_BCK   (5)
#define AUDIO_IIS_WS    (25)
#define AUDIO_IIS_TX    (26)
#define AUDIO_IIS_RX    (35)

#endif


#define URANUS_AUDIO_HAL_VOL_DEFAULT    (50)


#endif // _URANUS_PLATFORM_CFG_H_


