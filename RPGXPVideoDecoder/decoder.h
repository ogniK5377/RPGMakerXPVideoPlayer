#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <Windows.h>

#include <audioresampler.h>
#include <av.h>
#include <avutils.h>
#include <codec.h>
#include <ffmpeg.h>
#include <packet.h>
#include <videorescaler.h>

#include <codec.h>
#include <codeccontext.h>
#include <format.h>
#include <formatcontext.h>

#include <SDL_audio.h>
#include "common_types.h"

namespace RPGMaker {
class Bitmap;
}

class Decoder {
public:
    Decoder(EngineAddr target);
    ~Decoder();

    s32 GetInternalError() const;
    const char* GetInternalErrorMessage() const;

    bool IsCompleted() const;

    ErrorCode Setup(char* video_path);
    SDL_AudioFormat DecideBestFormat(av::SampleFormat format) const;
    av::SampleFormat DecideBestTarget(av::SampleFormat format) const;
    void StartRender();

    void AheadOfTimeDecoder();
    void MarkDecoderCompleted();

    void Render();
    void MarkRenderCompleted();

    void SetVolume(float _volume_percentage);

    bool WasBadTermination() const;

private:
    struct StreamHolder {
        av::Stream stream{};
        std::size_t index{};
    };

    struct HistoryContainer {
        std::vector<u8> data{};
        av::Timestamp timestamp{};
    };

    std::shared_mutex video_history_mutex;
    std::shared_mutex audio_history_mutex;

    std::atomic<bool> kill_threads{false};
    std::atomic<bool> is_decoder_complete{false};
    std::atomic<bool> is_render_complete{false};
    std::atomic<bool> is_bad_terimination{false};

    std::unique_ptr<RPGMaker::Bitmap> bitmap;
    std::size_t frame_width{};
    std::size_t frame_height{};
    HWND game_window{};
    HANDLE decoder_thread{};
    HANDLE render_thread{};
    std::size_t sample_width{0};

    static constexpr std::size_t VIDEO_FRAME_HISTORY_SIZE = 120;
    static constexpr std::size_t AUDIO_FRAME_HISTORY_SIZE = 120;
    std::vector<HistoryContainer> VIDEO_FRAME_HISTORY;
    std::vector<HistoryContainer> AUDIO_FRAME_HISTORY;

    av::FormatContext format_ctx{};
    StreamHolder video_stream{};
    StreamHolder audio_stream{};

    av::VideoDecoderContext vdec{};
    av::AudioDecoderContext adec{};

    std::unique_ptr<av::VideoRescaler> rescaler;
    std::unique_ptr<av::AudioResampler> resampler;

    std::size_t audio_stream_pos{};
    double time_shift{};
    std::atomic<float> volume_percentage{0.1f};

    std::chrono::duration<double> real_ts{};

    SDL_AudioDeviceID audio_device{};

    std::error_code internal_error{};

    std::chrono::time_point<std::chrono::steady_clock> start_tps;
};
