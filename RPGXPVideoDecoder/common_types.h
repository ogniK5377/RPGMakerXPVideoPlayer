#pragma once
#include <cstdint>
#include <cstring>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using f32 = float;
using f64 = double;

using EngineAddr = void*;

#define API_CALL extern "C" __declspec(dllexport)

enum class ErrorCode : s32 {
    Success = 0,
    VideoNotFinished = 1,
    DecoderInstanceAlreadyCreated = 2,
    DecoderNotCreated = 3,
    FileNotFound = 4,
    InvalidFile = 5,
    BitmapIsDisposed = 6,
    FailedToFindVideoStream = 7,
    FailedToFindAudioStream = 8,
    FailedToOpenAudioDevice = 9,
    InternalError = 10,
};
