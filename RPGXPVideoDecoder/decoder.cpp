#include <audioresampler.h>
#include <av.h>
#include <avutils.h>
#include <videorescaler.h>

#include "decoder.h"
#include "rgssad_bitmap.h"

DWORD WINAPI DecoderBootstrap(LPVOID lpParam);
DWORD WINAPI RenderBootstrap(LPVOID lpParam);

Decoder::Decoder(EngineAddr target) : bitmap(std::make_unique<RPGMaker::Bitmap>(target)) {
    frame_width = bitmap->GetWidth();
    frame_height = bitmap->GetHeight();

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_PANIC);
}

Decoder::~Decoder() {
    if (render_thread != NULL) {
        CloseHandle(render_thread);
        render_thread = NULL;
    }

    if (decoder_thread != NULL) {
        CloseHandle(decoder_thread);
        decoder_thread = NULL;
    }
}

s32 Decoder::GetInternalError() const {
    return internal_error.value();
}

const char* Decoder::GetInternalErrorMessage() const {
    return internal_error.message().c_str();
}

bool Decoder::IsCompleted() const {
    return is_decoder_complete.load() && is_render_complete.load();
}

ErrorCode Decoder::Setup(char* video_path) {
    // Open video file
    std::error_code err{};
    format_ctx.openInput(video_path, err);
    if (err) {
        internal_error = err;
        return ErrorCode::InternalError;
    }

    // Find all streams
    format_ctx.findStreamInfo(err);
    if (err) {
        internal_error = err;
        return ErrorCode::InternalError;
    }

    // Locate our video and audio streams
    const auto stream_count = format_ctx.streamsCount();
    for (std::size_t i = 0; i < stream_count; i++) {
        const auto stream = format_ctx.stream(i);
        if (stream.isVideo() && video_stream.stream.isNull()) {
            video_stream.stream = stream;
            video_stream.index = i;
        }
        if (stream.isAudio() && audio_stream.stream.isNull()) {
            audio_stream.stream = stream;
            audio_stream.index = i;
        }
    }

    if (video_stream.stream.isNull()) {
        return ErrorCode::FailedToFindVideoStream;
    }

    if (audio_stream.stream.isNull()) {
        return ErrorCode::FailedToFindAudioStream;
    }

    // Setup the video decoder
    vdec = av::VideoDecoderContext(video_stream.stream);
    av::Codec codec = av::findDecodingCodec(vdec.raw()->codec_id);

    vdec.setCodec(codec);
    vdec.setRefCountedFrames(true);
    vdec.open(av::Codec(), err);
    if (err) {
        internal_error = err;
        return ErrorCode::InternalError;
    }

    // Setup the audio decoder
    adec = av::AudioDecoderContext(audio_stream.stream);
    codec = av::findDecodingCodec(adec.raw()->codec_id);

    adec.setCodec(codec);
    adec.setRefCountedFrames(true);
    adec.open(av::Codec(), err);
    if (err) {
        internal_error = err;
        return ErrorCode::InternalError;
    }
    format_ctx.substractStartTime(true);

    // Setup the video rescaler to match our bitmaps resolution
    rescaler = std::make_unique<av::VideoRescaler>(
        frame_width, frame_height, AV_PIX_FMT_BGRA); // RGB maker stores buffers in BGRA

    // Setup the resampler to match our SDL setup
    resampler = std::make_unique<av::AudioResampler>(
        adec.channelLayout(), adec.sampleRate(), DecideBestTarget(adec.sampleFormat()),
        adec.channelLayout(), adec.sampleRate(), adec.sampleFormat());

    // Open an audio device for SDL
    SDL_AudioSpec spec{};
    SDL_AudioSpec recv{};
    spec.freq = adec.sampleRate();
    spec.format = DecideBestFormat(adec.sampleFormat());
    spec.channels = 2;
    spec.samples = 1024;
    spec.callback = nullptr; // TODO(ogniK): Setup callback
    spec.userdata = nullptr;

    switch (spec.format) {
    case AUDIO_U8:
        sample_width = 1;
        break;
    case AUDIO_S16:
        sample_width = 2;
        break;
    case AUDIO_S32:
    case AUDIO_F32:
        sample_width = 4;
        break;
    default:
        sample_width = 2;
    }

    audio_device = SDL_OpenAudioDevice(NULL, 0, &spec, &recv, 0);

    if (audio_device == 0) {
        return ErrorCode::FailedToOpenAudioDevice;
    }

    // Set the device state as playing so we don't need to worry about this later
    SDL_PauseAudioDevice(audio_device, 0);

    if (decoder_thread != NULL) {
        CloseHandle(decoder_thread);
        decoder_thread = NULL;
    }
    // We start the decoder before we want to start rendering to pre-prepare frames to be shown to
    // prevent choppy videos
    decoder_thread = CreateThread(NULL, NULL, DecoderBootstrap, this, NULL, NULL);

    return ErrorCode::Success;
}

SDL_AudioFormat Decoder::DecideBestFormat(av::SampleFormat format) const {
    switch (format) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
        return AUDIO_U8;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        return AUDIO_S16;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
        return AUDIO_S32;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
        return AUDIO_F32;
    default:
        return AUDIO_S16;
    }
}

av::SampleFormat Decoder::DecideBestTarget(av::SampleFormat format) const {
    switch (format) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
        return AV_SAMPLE_FMT_U8;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        return AV_SAMPLE_FMT_S16;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
        return AV_SAMPLE_FMT_S32;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
        return AV_SAMPLE_FMT_FLT;
    default:
        return AV_SAMPLE_FMT_S16;
    }
}

void Decoder::StartRender() {
    if (render_thread != NULL) {
        CloseHandle(render_thread);
        render_thread = NULL;
    }

    render_thread = CreateThread(NULL, NULL, RenderBootstrap, this, NULL, NULL);
}

void Decoder::AheadOfTimeDecoder() {
    while (true) {
        if (kill_threads.load()) {
            return;
        }

        // Whilst we need to keep decoding. If we have not fulfilled either of our decoder sizes,
        // we'll keep decoding. This means we'll actually decode more than our maximum at some
        // points. The history sizes should be considered as the LEAST amount of elements instead of
        // the maximum.
        while (VIDEO_FRAME_HISTORY.size() < VIDEO_FRAME_HISTORY_SIZE ||
               AUDIO_FRAME_HISTORY.size() < AUDIO_FRAME_HISTORY_SIZE) {
            // If we happen to be decoding, we should kill right away
            if (kill_threads.load()) {
                return;
            }

            // Decode the packet from the stream
            std::error_code err{};
            const auto pkt = format_ctx.readPacket(err);
            if (!pkt || err) {
                if (err) {
                    internal_error = err;
                    is_bad_terimination.store(true);
                }
                return;
            }

            if (pkt.streamIndex() == video_stream.index) {
                // Decode video stream
                av::VideoFrame frame = vdec.decode(pkt, err);

                // Rescale to our target resolution
                const auto out_frame = rescaler->rescale(frame, err);

                // Setup our container
                HistoryContainer container{};

                // Width * Height * sizeof(RPGMaker Color)
                container.data.resize(frame_width * frame_height * 4);

                // The timestamp of where the video is
                container.timestamp = pkt.ts();

                // Write to our histroy buffer
                std::memcpy(container.data.data(), out_frame.data(), container.data.size());
                {
                    std::lock_guard<std::shared_mutex> mutex(video_history_mutex);
                    VIDEO_FRAME_HISTORY.push_back(container);
                }
            } else if (pkt.streamIndex() == audio_stream.index) {
                // Decode audio stream
                const auto samples = adec.decode(pkt, err);
                // MessageBoxA(NULL, std::to_string(samples.size()).c_str(), NULL, NULL);
                // Push samples to be resampled into our new format
                resampler->push(samples, err);
                if (err) {
                    continue;
                }

                HistoryContainer container{};
                // Our sample format is s16 with 2 channel audio
                std::vector<u8> tmp(samples.samplesCount() * sample_width * 2);

                // Resample our audio for SDL
                auto ouSamples = av::AudioSamples::null();
                while ((ouSamples = resampler->pop(samples.samplesCount(), err))) {
                    const auto idx = ouSamples.samplesCount() * sample_width * 2;
                    std::memcpy(tmp.data() + tmp.size() - idx, ouSamples.data(), idx);
                }
                container.data.resize(tmp.size());

                // SDL requires the container buffer to be zero'd out before queuing otherwise we
                // get junk noise
                SDL_memset(container.data.data(), 0, container.data.size());

                // Mix the audio just to bring down the volume so our ears don't bleed
                SDL_MixAudioFormat(container.data.data(), tmp.data(), AUDIO_F32, tmp.size(),
                                   static_cast<s32>(static_cast<float>(SDL_MIX_MAXVOLUME) *
                                                    volume_percentage.load()));

                // Current audio timestamp
                container.timestamp = samples.pts();

                // Write to the histroy buffer
                {
                    std::lock_guard<std::shared_mutex> mutex(audio_history_mutex);
                    AUDIO_FRAME_HISTORY.push_back(container);
                }
            }
        }
    }
}

void Decoder::MarkDecoderCompleted() {
    is_decoder_complete.store(true);
}

void Decoder::MarkRenderCompleted() {
    is_render_complete.store(true);
}

void Decoder::SetVolume(float _volume_percentage) {
    volume_percentage.store(_volume_percentage);
}

bool Decoder::WasBadTermination() const {
    return is_bad_terimination.load();
}

void Decoder::Render() {
    // Get the active window so when we lose focus we can freeze the video as we're decoding on a
    // secondary thread as well as writing to memory
    game_window = GetForegroundWindow();

    while (VIDEO_FRAME_HISTORY.size() < VIDEO_FRAME_HISTORY_SIZE ||
           AUDIO_FRAME_HISTORY.size() < AUDIO_FRAME_HISTORY_SIZE) {
        Sleep(1);
    }

    using namespace std::chrono;
    start_tps = high_resolution_clock::now();

    while (true) {
        {
            std::shared_lock<std::shared_mutex> mutex(video_history_mutex);
            if (is_decoder_complete.load() && VIDEO_FRAME_HISTORY.empty()) {
                break;
            }
        }

        // Handle audio/video TPS shift when we lose focus from the window
        auto before_shift = high_resolution_clock::now();
        bool need_shift = false;
        while (GetForegroundWindow() != game_window) {
            SDL_PauseAudioDevice(audio_device, 1);
            Sleep(1);
            need_shift = true;
        }
        auto post_shift = high_resolution_clock::now();
        if (need_shift) {
            time_shift += std::chrono::duration<double>(post_shift - before_shift).count();
            SDL_PauseAudioDevice(audio_device, 0);
        }

        {
            // We manually handle mutexes here to ensure the ahead of time decoder doesn't stall
            // any longer than it should
            video_history_mutex.lock_shared();
            if (!VIDEO_FRAME_HISTORY.empty()) {
                video_history_mutex.unlock_shared();

                auto now = high_resolution_clock::now();
                real_ts = now - start_tps;

                // Write video frame to our bitmap
                {
                    std::lock_guard<std::shared_mutex> mutex(video_history_mutex);
                    auto& container = VIDEO_FRAME_HISTORY.front();

                    // Frame skipping
                    if ((container.timestamp.seconds() + time_shift) < real_ts.count()) {
                        for (auto it = VIDEO_FRAME_HISTORY.begin();
                             it != VIDEO_FRAME_HISTORY.end();) {
                            if ((it->timestamp.seconds() + time_shift) < real_ts.count()) {
                                it = VIDEO_FRAME_HISTORY.erase(it);
                            } else {
                                break;
                            }
                        }
                    }

                    if (!VIDEO_FRAME_HISTORY.empty()) {
                        container = VIDEO_FRAME_HISTORY.front();
                        // Wait till we get to the correct timestamp
                        while ((container.timestamp.seconds() + time_shift) > real_ts.count()) {
                            Sleep(1);
                            now = high_resolution_clock::now();
                            real_ts = now - start_tps;
                        }

                        // Send samples to play up to our tps
                        {
                            std::lock_guard<std::shared_mutex> mutex(audio_history_mutex);
                            if (!AUDIO_FRAME_HISTORY.empty()) {
                                for (auto it = AUDIO_FRAME_HISTORY.begin();
                                     it != AUDIO_FRAME_HISTORY.end();) {
                                    if ((it->timestamp.seconds() + time_shift) >= real_ts.count() ||
                                        AUDIO_FRAME_HISTORY.empty()) {
                                        break;
                                    } else {
                                        SDL_QueueAudio(audio_device, it->data.data(),
                                                       it->data.size());
                                        it = AUDIO_FRAME_HISTORY.erase(it);
                                    }
                                }
                            }
                        }

                        // Write video frame
                        if (!bitmap->WriteBufferFlipped(container.data.data(),
                                                        container.data.size())) {
                            // Failed to write buffer
                            std::lock_guard<std::shared_mutex> mutex_audio(audio_history_mutex);
                            VIDEO_FRAME_HISTORY.clear();
                            AUDIO_FRAME_HISTORY.clear();
                            kill_threads.store(true);
                            return;
                        }

                        // Delete decoded frame
                        VIDEO_FRAME_HISTORY.erase(VIDEO_FRAME_HISTORY.begin());
                    }
                }
            } else {
                video_history_mutex.unlock_shared();
            }
        }
    }
}

/* Bootstrap for the ahead of time video decoder */
DWORD WINAPI DecoderBootstrap(LPVOID lpParam) {
    auto* ffmpeg = static_cast<Decoder*>(lpParam);
    ffmpeg->AheadOfTimeDecoder();
    ffmpeg->MarkDecoderCompleted();
    return 0;
}

/* Bootstrap for bitmap rendering for RPG Maker XP */
DWORD WINAPI RenderBootstrap(LPVOID lpParam) {
    auto* ffmpeg = static_cast<Decoder*>(lpParam);
    ffmpeg->Render();
    ffmpeg->MarkRenderCompleted();
    return 0;
}
