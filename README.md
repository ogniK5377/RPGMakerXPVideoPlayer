# RPGMakerXPVideoPlayer
A video player for RPG Maker XP with AvCpp, FFmpeg and SDL2

## Usage
Refer to the Example Project provided in the releases tab!

## Optimization notes
Some videos might end up running a little slower than expected. To achieve the maximum performance, make sure your video is encoded to 640x480. This relieves work off the rescaler. Transformation meta-data should also be stripped from the video as this can put more work on the rescaler. From testing, the most optimal codec seems to be h264, most codecs will work but will differ on decoding speed.
