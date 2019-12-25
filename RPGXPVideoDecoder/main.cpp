#include <SDL.h>
#include <Windows.h>
#include "common_types.h"
#include "decoder.h"
#include "rgssad_bitmap.h"

std::unique_ptr<Decoder> ffmpeg_decoder{};
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        SDL_Init(SDL_INIT_AUDIO);

        break;
    case DLL_PROCESS_DETACH:
        if (ffmpeg_decoder) {
            ffmpeg_decoder.reset();
        }
        SDL_AudioQuit();
        SDL_Quit();
        break;
    }
    return true;
}

API_CALL ErrorCode ViDecCreateContext(char* video_path, s32 video_volume,
                                      EngineAddr bitmap_object) {
    const DWORD file_attributes = GetFileAttributesA(video_path);
    // Make sure the file exists
    if (file_attributes == INVALID_FILE_ATTRIBUTES) {
        return ErrorCode::FileNotFound;
    }

    if (file_attributes & FILE_ATTRIBUTE_DIRECTORY) {
        return ErrorCode::InvalidFile;
    }

    const RPGMaker::Bitmap bitmap(bitmap_object);
    // Make sure our bitmap is still valid
    if (bitmap.IsDisposed()) {
        return ErrorCode::BitmapIsDisposed;
    }

    // Only allow 1 decoder instance at one time
    if (ffmpeg_decoder) {
        return ErrorCode::DecoderInstanceAlreadyCreated;
    } else {
        ffmpeg_decoder = std::make_unique<Decoder>(bitmap_object);
    }

    video_volume = max(0, min(video_volume, 128));
    ffmpeg_decoder->SetVolume(static_cast<float>(video_volume) / 128.0f);

    // Setup the decoder and start the ahead of time decoder
    return ffmpeg_decoder->Setup(video_path);
}

API_CALL ErrorCode ViDecSetVolume(s32 volume) {
    if (!ffmpeg_decoder) {
        return ErrorCode::DecoderNotCreated;
    }

    volume = max(0, min(volume, 128));

    ffmpeg_decoder->SetVolume(static_cast<float>(volume) / 128.0f);

    return ErrorCode::Success;
}

API_CALL ErrorCode ViDecCloseContext() {
    if (!ffmpeg_decoder) {
        return ErrorCode::DecoderNotCreated;
    }

    // Free up our decoder context
    ffmpeg_decoder.reset();
    return ErrorCode::Success;
}

API_CALL ErrorCode ViDecStartRender() {
    if (!ffmpeg_decoder) {
        return ErrorCode::DecoderNotCreated;
    }

    ffmpeg_decoder->StartRender();
    return ErrorCode::Success;
}

API_CALL ErrorCode ViDecGetVideoState() {
    if (!ffmpeg_decoder) {
        return ErrorCode::DecoderNotCreated;
    }

    // Return if we're still rendering the video
    if (!ffmpeg_decoder->IsCompleted()) {
        return ErrorCode::VideoNotFinished;
    }

    // Video rendering finished
    return ErrorCode::Success;
}

API_CALL s32 ViDecGetInternalError() {
    if (!ffmpeg_decoder) {
        return 0;
    }
    return ffmpeg_decoder->GetInternalError();
}

API_CALL const char* ViDecGetInternalErrorMessage() {
    if (!ffmpeg_decoder) {
        return "Decoder instance doesn't exist";
    }
    return ffmpeg_decoder->GetInternalErrorMessage();
}

API_CALL s32 ViDecWasBadTermination() {
    if (!ffmpeg_decoder) {
        return 0;
    }
    return ffmpeg_decoder->WasBadTermination() ? 1 : 0;
}
