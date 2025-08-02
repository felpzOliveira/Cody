/* date = July 27th 2025 16:15 */
#pragma once

#define AUDIO_MESSAGE_MAX_SIZE 256

#define AUDIO_STATE_BIT(n) (1 << n)

#define AUDIO_STATE_RUNNING_BIT AUDIO_STATE_BIT(0)
#define AUDIO_STATE_PLAYING_BIT AUDIO_STATE_BIT(1)

typedef enum{
    AUDIO_PLAY_MODE_SIMPLE,
    AUDIO_PLAY_MODE_REPEAT,
}AudioPlayMode;

typedef enum{
    AUDIO_CODE_PLAY,
    AUDIO_CODE_PAUSE,
    AUDIO_CODE_RESUME,
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
 * Terminates the audio subsystem. Dont just send a finish message, call
 * this instead.
 */
void TerminateAudioSystem();

/*
 * Checks if the audio system is up _right_now_.
 */
bool AudioIsRunning();

/*
 * Gets the current state of the audio system _right_now_.
 */
int AudioGetState();

/*
 * Send a request to the audio system.
 */
void AudioRequest(AudioMessage &message);

/*
 * Sends a music-add request optionally requesting for the
 * audio thread to take ownership of the file. (deleting it after use).
 */
void AudioRequestAddMusic(const char *path, int takeFile);
