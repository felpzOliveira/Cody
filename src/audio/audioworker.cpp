#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <dr_mp3.h>
#include <parallel.h>
#include <audio.h>
#include <atomic>
#include <utilities.h>
#include <vector>
#include <queue>

// TODO: Actual log
#define AudioLog(...) printf(__VA_ARGS__)

#define AUDIO_ERR_OK              0
#define AUDIO_ERR_INIT_FAILED     1

// 500 ms
#define AUDIO_CACHE_INTERVAL      500
#define AUDIO_CACHE_LOW_WATERMARK 300

#define ID3V2_MAX_MIME_SIZE       64

struct AudioTrack{
    drmp3 mp3;
    drmp3_uint64 framesRead;
    std::vector<int16_t> samples;
    SDL_AudioStream *stream;
    bool samplesConsumed;
    bool eof;
    char title[ID3V2_MAX_MIME_SIZE];
    char author[ID3V2_MAX_MIME_SIZE];
    std::vector<uint8_t> cover;
};

struct AudioController{
    SDL_AudioStream *stream;
    ConcurrentTimedQueue<AudioMessage> audioQueue;
    std::queue<AudioTrack *> trackList;
    SDL_AudioSpec spec;
    AudioPlayMode mode;
};

static std::thread audioThreadId;
static std::atomic<bool> audioThreadRunning = false;
static std::atomic<bool> audioThreadInited = false;
static std::atomic<int> audioThreadState = 0;
static AudioController gAudioController = {
    .stream = nullptr
};

/* internal thread calls */
static void _AudioInitEmptyTrack(AudioTrack *track){
    track->mp3 = {};
    track->framesRead = 0;
    track->eof = false;
    track->stream = nullptr;
}

static int _AudioInit(AudioController *controller){
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if(!SDL_Init(SDL_INIT_AUDIO)){
        AudioLog("SDL audio init failed: %s\n", SDL_GetError());
        return AUDIO_ERR_INIT_FAILED;
    }

    controller->spec.freq     = 48000;
    controller->spec.format   = SDL_AUDIO_S16;
    controller->spec.channels = 2;

    controller->stream =
              SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                        &controller->spec, nullptr, nullptr);

    if(controller->stream == nullptr){
        AudioLog("Failed to initialize audio device\n");
        return AUDIO_ERR_INIT_FAILED;
    }

    controller->mode = AUDIO_PLAY_MODE_REPEAT;
    SDL_ResumeAudioStreamDevice(controller->stream);

    return AUDIO_ERR_OK;
}

static void _AudioResetController(AudioController *controller){
    controller->stream = nullptr;
    controller->audioQueue.clear();
    controller->spec = {};
    // 10× smaller than the audio sampling interval
    // hopefully plenty to do all the required work
    // without interfering with music pacing.
    controller->audioQueue.set_interval(50);
}

static void _AudioCleanupController(AudioController *controller){
    if(controller->stream){
        SDL_DestroyAudioStream(controller->stream);
    }

    _AudioResetController(controller);
}

static void _AudioFinish(AudioController *controller){
    _AudioCleanupController(controller);
    SDL_Quit();
}

static void _AudioReadNextFrames(AudioTrack *track){
    // compute a rough N ms of track
    const uint32_t frames = track->mp3.sampleRate *
                                AUDIO_CACHE_INTERVAL / 1000;
    track->samples.resize(frames * track->mp3.channels);

    track->framesRead =
        drmp3_read_pcm_frames_s16(&track->mp3, frames, track->samples.data());

    int bytes = track->framesRead *
                    track->mp3.channels * sizeof(int16_t);
    SDL_PutAudioStreamData(track->stream, track->samples.data(), bytes);
    track->samplesConsumed = false;
}

static bool
_AudioPushTrackToStream(AudioController *controller, AudioTrack *track){
    bool ok = true;
    int max_size = track->samples.size();
    for(;;){
        int got = SDL_GetAudioStreamData(track->stream,
                        track->samples.data(), track->samples.size());

        if(got <= 0)
            break;

        int rv = SDL_PutAudioStreamData(controller->stream,
                                track->samples.data(), got);
        if(rv < 0){
            AudioLog("SDL_PutAudioStreamData failed: %s\n", SDL_GetError());
            ok = false;
        }

        audioThreadState |= AUDIO_STATE_PLAYING_BIT;
    }

    return ok;
}

static bool
_AudioHandleTrackProgression(AudioController *controller, AudioTrack *track){
    int bytesPerFrame = controller->spec.channels * sizeof(int16_t);
    int low_watermark = controller->spec.freq *
                                AUDIO_CACHE_LOW_WATERMARK / 1000;
    int enqueuedBytes = SDL_GetAudioStreamQueued(controller->stream);

    if(!track->eof){
        if(low_watermark * bytesPerFrame > enqueuedBytes){

            // if the track was never moved to stream, move it now
            if(!track->samplesConsumed){
                _AudioPushTrackToStream(controller, track);
            }

            _AudioReadNextFrames(track);

            if(track->framesRead != 0){
                _AudioPushTrackToStream(controller, track);
            }else{
                if(controller->mode == AUDIO_PLAY_MODE_REPEAT){
                    // go back to begining
                    drmp3_seek_to_pcm_frame(&track->mp3, 0);
                }else{
                    track->eof = true;
                    SDL_FlushAudioStream(controller->stream);
                }
            }
        }

        // We have stuff enqueued or in the stream
        return false;
    }

    // We reach eof, wait for queue to be empty
    return enqueuedBytes == 0;
}

static inline uint32_t syncsafe_to_u32(const uint8_t b[4]){
    return ((b[0] & 0x7F) << 21) | ((b[1] & 0x7F) << 14) | ((b[2] & 0x7F) << 7) | (b[3] & 0x7F);
}

static inline uint32_t be32(const uint8_t b[4]){
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | uint32_t(b[3]);
}

static void _AudioOpen_OnMeta(void *pUser, const drmp3_metadata *pMetadata){
    AudioTrack *track = (AudioTrack *)pUser;

    // NOTE: Why would you go into the entire trouble of implementing
    //       a mp3 decoder and not extract meta data.
    if(pMetadata->type == DRMP3_METADATA_TYPE_ID3V2){
        const void *memory = pMetadata->pRawData;
        size_t size  = pMetadata->rawDataSize;

        uint8_t *u8mem = (uint8_t *)memory;

        if(u8mem[0] != 'I' || u8mem[1] != 'D' || u8mem[2] != '3'){
            AudioLog("Invalid 'ID3' heading\n");
            return;
        }

        uint8_t ver_major = u8mem[3];
        uint8_t ver_minor = u8mem[4];
        uint8_t flags     = u8mem[5];
        uint32_t tag_size = syncsafe_to_u32(u8mem + 6);

        if((flags & 0x40) && ver_major >= 3){
            AudioLog("[Unsupported] Extended header?\n");
            return;
        }

        if(flags & 0x80){
            AudioLog("[Unsupported] Unsync?\n");
            return;
        }

        if(ver_major == 2){
            // ID3v2.2: 3-byte id, 3-byte size, no flags
            AudioLog("[Unsupported] ID3v2.2\n");
        }else{
            // ID3v2.3 / 2.4: 4-byte id, 4-byte size, 2-byte flags
            // heading starts at pos = 10
            // 4 bytes frame id + 4 bytes size + 2 bytes flags
            int pos = 0;
            uint8_t *tag = &u8mem[10];

            while(pos + 10 <= tag_size){
                const char *id = (char *)&tag[pos];
                uint32_t fsize = (ver_major == 4) ?
                                        syncsafe_to_u32(&tag[pos + 4])
                                        : be32(&tag[pos + 4]);

                pos += 10;

                if(fsize == 0 || pos + fsize > tag_size)
                    break;

                int off = pos;
                uint8_t enc = tag[off++];
                size_t mime_start = off;

                char *mime_store_ptr  = nullptr;
                size_t mime_store_cnt = 0;
                bool parse_image = false;

                if(memcmp(id, "APIC", 4) == 0){
                    parse_image = true;
                }else if(memcmp(id, "TPE1", 4) == 0){
                    mime_store_ptr = track->author;
                    memset(mime_store_ptr, 0, ID3V2_MAX_MIME_SIZE);
                }else if(memcmp(id, "TIT2", 4) == 0){
                    mime_store_ptr = track->title;
                    memset(mime_store_ptr, 0, ID3V2_MAX_MIME_SIZE);
                }

                while(off < pos + fsize && tag[off] != 0){
                    if(mime_store_ptr){
                        if(mime_store_cnt < ID3V2_MAX_MIME_SIZE)
                            mime_store_ptr[mime_store_cnt++] = tag[off];
                        else
                            AudioLog("Mime is too big\n");
                    }

                    off++;
                }

                off ++; // skip null
                if(off >= pos + fsize){
                    pos += fsize;
                    continue;
                }

                if(parse_image){
                    // image type
                    uint8_t ptype = tag[off++];
                    if(enc == 0 || enc == 3){
                        while(off < pos + fsize && tag[off] != 0)
                            off++;
                        off++;
                    }else{
                        while(off + 1 < pos + fsize && (tag[off] != 0 || tag[off+1] != 0))
                            off += 2;
                        off += 2;
                    }

                    if(off > pos + fsize){
                        pos += fsize;
                        continue;
                    }

                    // only care about front cover, i.e.: ptype = 3
                    if(track->cover.size() == 0 && ptype == 3){
                        track->cover.insert(
                            track->cover.begin(),
                            &tag[off], &tag[pos + fsize]
                        );

                        AudioLog("Got image with size [%d]\n",
                                  (int)track->cover.size());
                    }
                }

                pos += fsize;
            }
        }
    }
}

static void _AudioPause(AudioController *controller, AudioMessage *message){
    SDL_PauseAudioStreamDevice(controller->stream);
    audioThreadState &= ~AUDIO_STATE_PLAYING_BIT;
}

static void _AudioResume(AudioController *controller, AudioMessage *message){
    SDL_ResumeAudioStreamDevice(controller->stream);

    if(SDL_GetAudioStreamQueued(controller->stream) > 0)
        audioThreadState |= AUDIO_STATE_PLAYING_BIT;
}

static void _AudioPlay(AudioController *controller, AudioMessage *message){
    AudioTrack *track = nullptr;
    SDL_AudioSpec in{};
    int takeFile  = message->argument[0];
    char *mp3Path = (char *)&message->argument[1];

    AudioLog("Enqueue path= %s\n", mp3Path);

    // Minimal checks and load
    if(!FileExists(mp3Path)){
        AudioLog("File path= %s does not exist\n", mp3Path);
        return;
    }

    track = new AudioTrack;
    _AudioInitEmptyTrack(track);

    if(!drmp3_init_file_with_metadata(&track->mp3, mp3Path,
                                  _AudioOpen_OnMeta, track, nullptr))
    {
        AudioLog("Failed to initialize path= %s\n", mp3Path);
        goto __error;
    }

    // Create dedicated track stream for possible re-sampling
    in.freq     = track->mp3.sampleRate;
    in.format   = SDL_AUDIO_S16;
    in.channels = track->mp3.channels;

    track->stream = SDL_CreateAudioStream(&in, &controller->spec);
    if(track->stream == nullptr){
        AudioLog("Could not create audio stream\n");
        goto __error;
    }

    // Load a couple of frames
    _AudioReadNextFrames(track);

    if(track->framesRead == 0){
        AudioLog("Read no frames, empty/corrupted file?\n");
        goto __error;
    }

    // Store path in case of owned file
    if(takeFile){
        UNLINK(mp3Path);
    }

    AudioLog("Pushing track [%s - %s]\n", track->title, track->author);
    controller->trackList.push(track);
    return;

__error:
    if(track)
        delete track;
}

static void AudioThreadEntry(AudioController *controller){
    int rv = 0;
    bool requestedTerminate = false;
    audioThreadRunning = true;
    audioThreadState = 0;
    _AudioResetController(controller);

    rv = _AudioInit(controller);
    if(rv != AUDIO_ERR_OK)
        goto __terminate;

    audioThreadInited = true;
    audioThreadState |= AUDIO_STATE_RUNNING_BIT;

    while(!requestedTerminate){
        std::optional<AudioMessage> optMessage = controller->audioQueue.pop();
        if(optMessage.has_value()){
            AudioMessage message = optMessage.value();
            AudioLog("Got message with code {%x}\n", message.code);

            switch(message.code){
                case AUDIO_CODE_PLAY:{
                    _AudioPlay(controller, &message);
                } break;

                case AUDIO_CODE_PAUSE:{
                    _AudioPause(controller, &message);
                } break;

                case AUDIO_CODE_RESUME:{
                    _AudioResume(controller, &message);
                } break;

                case AUDIO_CODE_FINISH:{
                    requestedTerminate = true;
                } break;

                // TODO: More commands?
            }
        }

        if(controller->trackList.size() > 0 && !requestedTerminate){
            AudioTrack *currTrack = controller->trackList.front();
            if(_AudioHandleTrackProgression(controller, currTrack)){
                AudioLog("Finished track, clearing\n");
                drmp3_uninit(&currTrack->mp3);
                controller->trackList.pop();
                SDL_DestroyAudioStream(currTrack->stream);
                delete currTrack;

                audioThreadState &= ~AUDIO_STATE_PLAYING_BIT;
            }
        }
    }

__finish:
    while(!controller->trackList.empty()){
        AudioTrack *track = controller->trackList.front();
        drmp3_uninit(&track->mp3);
        SDL_DestroyAudioStream(track->stream);
        delete track;
        controller->trackList.pop();
    }

    _AudioFinish(controller);
__terminate:
    audioThreadRunning = false;
    audioThreadState = 0;
}

/* interface */
void InitializeAudioSystem(){
    if(!audioThreadRunning){
        audioThreadInited = false;
        // TODO: make this joinable instead
        audioThreadId = std::thread(AudioThreadEntry, &gAudioController);
        while(!audioThreadInited){
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void TerminateAudioSystem(){
    if(AudioIsRunning()){
        AudioMessage message;
        message.code = AUDIO_CODE_FINISH;
        AudioRequest(message);

        if(audioThreadId.joinable())
            audioThreadId.join();
    }
}

int AudioGetState(){
    return audioThreadState;
}

bool AudioIsRunning(){
    return audioThreadRunning;
}

void AudioRequest(AudioMessage &message){
    gAudioController.audioQueue.push(message);
}

void AudioRequestAddMusic(const char *path, int takeFile){
    AudioMessage message;
    message.code = AUDIO_CODE_PLAY;
    message.argument[0] = takeFile;
    int n = snprintf((char *)&message.argument[1],
                     AUDIO_MESSAGE_MAX_SIZE-1, "%s", path);
    message.argument[n+1] = 0;
    AudioRequest(message);
}
