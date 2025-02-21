/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Local Includes
#include "videofile.h"
#include "video.h"

// External Includes
#include <nap/device.h>
#include <nap/resourceptr.h>
#include <nap/numeric.h>
#include <texture.h>

namespace nap
{
    // Forward Declares
    class VideoAdvancedService;

    class NAPAPI VideoPlayerAdvancedBase : public Device
    {
    RTTI_ENABLE(Device)
    friend class VideoAdvancedService;
    public:
        VideoPlayerAdvancedBase(VideoAdvancedService& service);
    protected:
        virtual void update(double deltaTime) = 0;

        VideoAdvancedService& mService;
    };

    class NAPAPI VideoPlayerAdvanced : public VideoPlayerAdvancedBase
    {
    RTTI_ENABLE(VideoPlayerAdvancedBase)
        friend class VideoAdvancedService;
    public:

        // Constructor
        VideoPlayerAdvanced(VideoAdvancedService& service);

        /**
         * @return currently selected video file.
         */
        const VideoFile& getFile() const;

        /**
         * @return selected video context
         */
        const Video& getVideo() const;

        /**
         * @return selected video context
         */
        Video& getVideo();

        /**
         * @return selected video index
         */
        int getIndex() const										{ return mCurrentVideoIndex; }

        /**
         * @return total number of available videos to choose from
         */
        int getCount() const										{ return static_cast<int>(mVideos.size()); }

        /**
         * Loads a new video to play, returns false if selection fails.
         * The 'VideoChanged' signal is emitted on success.
         * Call play() afterwards to start playback of the video.
         *
         * ~~~~~{.cpp}
         *	utility::ErrorState error;
         *	if (!mVideoPlayer->selectVideo(new_selection, error))
         *		nap::Logger::error(error.toString());
         *	else
         *		mVideoPlayer->play();
         * ~~~~~
         *
         * A new set of YUV textures is generated IF the new video has different dimensions or is the first to be selected.
         * The old set of textures will be destroyed immediately and will be invalid after this call.
         *
         * @param index index of the video to load.
         * @param error contains the error if changing the video fails.
         * @return if video selection succeeded or failed
         */
        bool selectVideo(int index, utility::ErrorState& error);

        /**
          * Starts playback of the current video at the given offset in seconds.
         * @param startTime The offset in seconds to start the video at.
         * @param clearTextures if the old textures should be cleared before starting playback.
         */
        void play(double startTime = 0.0, bool clearTextures = true);

        /**
         * Stops playback of the current video.
         */
        void stopPlayback()											{ getVideo().stop(true); }

        /**
         * Check if the currently loaded video is playing.
         * @return If the video is currently playing.
         */
        bool isPlaying() const										{ return getVideo().isPlaying(); }

        /**
         * If the video re-starts after completion.
         * @param value if the video re-starts after completion.
         */
        void loop(bool value);

        /**
         * @return if the current video is looping
         */
        bool isLooping() const										{ return mLoop; }

        /**
         * Changes the playback speed of the player.
         * @param speed new playback speed, 1.0f = default speed.
         */
        void setSpeed(float speed);

        /**
         * @return current video playback speed
         */
        float getSpeed() const										{ return mSpeed; }

        /**
         * Seeks within the video to the time provided. This can be called while playing.
         * @param seconds: the time offset in seconds in the video.
         */
        void seek(double seconds)									{ getVideo().seek(seconds); }

        /**
         * @return The current playback position in seconds.
         */
        double getCurrentTime() const								{ return getVideo().getCurrentTime(); }

        /**
         * @return The duration of the video in seconds.
         */
        double getDuration() const									{ return getVideo().getDuration(); }

        /**
         * @return Width of the video, in pixels.
         */
        int getWidth() const										{ return getVideo().getWidth(); }

        /**
         * @return Height of the video, in pixels.
         */
        int getHeight() const										{ return getVideo().getHeight(); }

        /**
         * @return Whether this video has an audio stream.
         */
        bool hasAudio() const										{ return getVideo().hasAudio(); }

        /**
         * Starts the device.
         * @param errorState contains the error if the device can't be started
         * @return if the device started
         */
        virtual bool start(utility::ErrorState& errorState) override;

        /**
         * Stops the device
         */
        virtual void stop() override;

        /**
         * @return if there is a video selected.
         */
        bool hasSelection() const								{ return mCurrentVideo != nullptr; }

        std::vector<nap::ResourcePtr<VideoFile>> mVideoFiles;	///< Property: 'Files' All video file links
        nap::uint mVideoIndex = 0;								///< Property: 'Index' Selected video index
        bool mLoop = false;										///< Property: 'Loop' if the selected video loops
        float mSpeed = 1.0f;									///< Property: 'Speed' video playback speed
    protected:
        /**
         * Update textures, can only be called by the video service
         */
        void update(double deltaTime) override;
    private:
        /**
         * Clear output textures to black
         */
        void clearTextures();

        int	mCurrentVideoIndex = 0;								///< Current selected video index.
        nap::Video* mCurrentVideo = nullptr;					///< Current selected video context
        bool mTexturesCreated = false;							///< If the textures have been created
        std::vector<std::unique_ptr<nap::Video>> mVideos;		///< All the actual videos
    };

    // Object creator
    using HapPlayerObjectCreator = rtti::ObjectCreator<VideoPlayerAdvanced, VideoAdvancedService>;
}
