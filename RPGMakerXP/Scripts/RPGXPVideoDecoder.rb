class ViDec
    ErrorCode = {
        'Success' => 0,
        'VideoNotFinished' => 1,
        'DecoderInstanceAlreadyCreated' => 2,
        'DecoderNotCreated' => 3,
        'FileNotFound' => 4,
        'InvalidFile' => 5,
        'BitmapIsDisposed' => 6,
        'FailedToFindVideoStream' => 7,
        'FailedToFindAudioStream' => 8,
        'FailedToOpenAudioDevice' => 9,
        'InternalError' => 10,
    }

    ViDecCreateContext = Win32API.new('RPGXPVideoDecoder.dll', 'ViDecCreateContext', 'pip', 'i')
    ViDecCloseContext = Win32API.new('RPGXPVideoDecoder.dll', 'ViDecCloseContext', '', 'i')
    ViDecStartRender = Win32API.new('RPGXPVideoDecoder.dll', 'ViDecStartRender', '', 'i')
    ViDecGetVideoState = Win32API.new('RPGXPVideoDecoder.dll', 'ViDecGetVideoState', '', 'i')
    ViDecSetVolume = Win32API.new('RPGXPVideoDecoder.dll', 'ViDecSetVolume', 'i', 'i')

    ViDecWasBadTermination = Win32API.new('RPGXPVideoDecoder.dll', 'ViDecWasBadTermination', '', 'i')
    ViDecGetInternalError = Win32API.new('RPGXPVideoDecoder.dll', 'ViDecGetInternalError', '', 'i')
    ViDecGetInternalErrorMessage = Win32API.new('RPGXPVideoDecoder.dll', 'ViDecGetInternalErrorMessage', '', 'p')
    
    def convert_error(err)
        if err == ErrorCode['Success']
            return "Successful operation"
        elsif err == ErrorCode['VideoNotFinished']
            return "Video still decoding"
        elsif err == ErrorCode['DecoderInstanceAlreadyCreated']
            return "Attempted to create secondary video decoder"
        elsif err == ErrorCode['DecoderNotCreated']
            return "Decoder has not been created"
        elsif err == ErrorCode['FileNotFound']
            return "File does not exist"
        elsif err == ErrorCode['BitmapIsDisposed']
            return "Bitmap is disposed"
        elsif err == ErrorCode['FailedToFindVideoStream']
            return "Video file has no video streams"
        elsif err == ErrorCode['FailedToFindAudioStream']
            return "Video file has no audio streams"
        elsif err == ErrorCode['FailedToOpenAudioDevice']
            return "Failed to open an audio device"
        elsif err == ErrorCode['InternalError']
            return "An internal error has occured"
        end
    end

    def initialize(video_file, volume=0.1)
        video_file = video_file.delete!("\n")
        # print(video_file)
        @video_plane = Sprite.new
        @video_plane.bitmap = Bitmap.new(640, 480)
        err = ViDecCreateContext.call(video_file, (volume * 128.0).floor.to_i, @video_plane.bitmap.object_id << 1)
        # If a user presses F12, another instance can exist. We'll just clean it up and recreate
        if err == ErrorCode['DecoderInstanceAlreadyCreated']
            ViDecCloseContext.call()
            err = ViDecCreateContext.call(video_file, (volume * 128.0).floor.to_i, @video_plane.bitmap.object_id << 1)
        end

        if err != ErrorCode['Success']
            print("Failed to create video context\n" + convert_error(err))
            $scene = Scene_Map.new
        end
    end

    def main
        Graphics.transition
        err = ViDecStartRender.call()
        if err == ErrorCode['Success']
            loop do
                Graphics.update
                err = ViDecGetVideoState.call()
                if err == ErrorCode['Success']
                    $scene = Scene_Map.new
                elsif err != ErrorCode['VideoNotFinished']
                    print("Failed to get video state\n" + convert_error(err))
                    $scene = Scene_Map.new
                end
                break if $scene != self
            end
        else
            print("Failed to start renderer\n" + convert_error(err))
            $scene = Scene_Map.new
        end

        if ViDecWasBadTermination.call() == 1
            print("Decoder failed!\nInternal error code: " + ViDecGetInternalError.call().to_s + "\nMessage: " + ViDecGetInternalErrorMessage.call())
        end

        err = ViDecCloseContext.call()
        if err != ErrorCode['Success']
            print("Failed to cleanup context\n" + convert_error(err))
        end
        Graphics.update
        Graphics.freeze
        @video_plane.dispose
    end
end
