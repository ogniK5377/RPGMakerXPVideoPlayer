# RPGMakerXPVideoPlayer

A video player for RPG Maker XP with AvCpp, FFmpeg and SDL2

## Usage

Refer to the Example Project provided in the releases tab!

## Optimization notes

Some videos might end up running a little slower than expected. To achieve the maximum performance, make sure your video is encoded to 640x480. This relieves work off the rescaler. Transformation meta-data should also be stripped from the video as this can put more work on the rescaler. From testing, the most optimal codec seems to be h264, most codecs will work but will differ on decoding speed.

## Building

The project files are built using Visual Studio 2019 with C++17. For simplicity, CMake wasn't used or any build system, and everything was setup to be used directly with visual studio.

### Setting up externals

The video decoder uses various libraries from other projects, these projects include [SDL 2](https://www.libsdl.org/), [AvCpp](https://github.com/h4tr3d/avcpp) and [FFMpeg](https://ffmpeg.org/). [AvCpp](https://github.com/h4tr3d/avcpp) need to be manually build, refer to the build guide [here](https://github.com/h4tr3d/avcpp/blob/master/README.md). Prebuilt binaries of [FFMpeg](https://ffmpeg.org/) and [SDL 2](https://www.libsdl.org/) can be used when building the video player. The windows builds for FFMpeg can be downloaded [here](https://ffmpeg.zeranoe.com/builds/) and the SDL 2 binaries and development headers can be downloaded [here](https://www.libsdl.org/download-2.0.php).

All externals should be placed in their respective folders [here](https://github.com/ogniK5377/RPGMakerXPVideoPlayer/tree/master/RPGXPVideoDecoder/externals)

### Building the project

The project is setup to only build with x86 in release mode, no other modes are setup to build since RPG Maker XP doesn't have a mechanism for properly debugging DLLs, so a debug build isn't necessary.
