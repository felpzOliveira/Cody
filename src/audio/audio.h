/* date = July 27th 2025 16:15 */
#pragma once

#define AUDIO_MESSAGE_MAX_SIZE 256

typedef enum{
    AUDIO_PLAY_MODE_SIMPLE,
    AUDIO_PLAY_MODE_REPEAT,
}AudioPlayMode;

typedef enum{
    AUDIO_CODE_PLAY,

    AUDIO_CODE_FINISH,
}AudioMessageCode;

struct AudioMessage{
    AudioMessageCode code;
    uint8_t argument[AUDIO_MESSAGE_MAX_SIZE];
};

/*
 * Initializes the audio subsystem. All audio is handled in a separated
 * thread. This only launches the thread in case it is not running.
 */
void InitializeAudioSystem();

/*
 * Send a request to the audio system.
 */
void AudioRequest(AudioMessage &message);
