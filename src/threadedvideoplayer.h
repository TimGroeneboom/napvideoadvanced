/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Local Includes
#include "videofile.h"
#include "video.h"
#include "videoplayeradvanced.h"
#include "videopixelformathandler.h"
#include "concurrentqueue.h"

// External Includes
#include <nap/device.h>
#include <nap/resourceptr.h>
#include <nap/numeric.h>
#include <texture.h>

namespace nap
{
    // Forward Declares
    class VideoAdvancedService;

    class NAPAPI ThreadedVideoPlayer final : public VideoPlayerAdvancedBase
    {
    RTTI_ENABLE(VideoPlayerAdvancedBase)
        friend class VideoAdvancedService;
    public:

        // Constructor
        ThreadedVideoPlayer(VideoAdvancedService& service);

        virtual ~ThreadedVideoPlayer();

        /**
         * Starts playback of the current video at the given offset in seconds.
         * @param startTime The offset in seconds to start the video at.
         * @param clearTextures if the old textures should be cleared before starting playback.
         */
        void play(double startTime = 0.0, bool clearTextures = true);

        /**
         * Stops playback of the current video.
         */
        void stopPlayback();

        /**
         * Check if the currently loaded video is playing.
         * @return If the video is currently playing.
         */
        bool isPlaying() const;

        /**
         * If the video re-starts after completion.
         * @param value if the video re-starts after completion.
         */
        void loop(bool value);

        /**
         * @return if the current video is looping
         */
        bool isLooping() const;

        /**
         * Changes the playback speed of the player.
         * @param speed new playback speed, 1.0f = default speed.
         */
        void setSpeed(float speed);

        /**
         * @return current video playback speed
         */
        float getSpeed() const;

        /**
         * Seeks within the video to the time provided. This can be called while playing.
         * @param seconds: the time offset in seconds in the video.
         */
        void seek(double seconds);

        /**
         * @return The current playback position in seconds.
         */
        double getCurrentTime() const;

        /**
         * @return The duration of the video in seconds.
         */
        double getDuration() const;

        /**
         * @return Width of the video, in pixels.
         */
        int getWidth() const;

        /**
         * @return Height of the video, in pixels.
         */
        int getHeight() const;

        /**
         * @return Whether this video has an audio stream.
         */
        bool hasAudio() const;

        /**
         * Starts the device.
         * @param errorState contains the error if the device can't be started
         * @return if the device started
         */
        virtual bool start(utility::ErrorState& errorState) override;

        /**
         * Stops the device, don't call this directly, use stopPlayback() instead.
         */
        virtual void stop() override;

        /**
         * Load a video from a file path.
         * @param filePath The path to the video file.
         */
        void loadVideo(const std::string& filePath);

        std::string mFilePath;									///< Property: 'FilePath' Path to the video file, leave empty to not load a video on init
        bool mLoop = false;										///< Property: 'Loop' if the selected video loops
        float mSpeed = 1.0f;									///< Property: 'Speed' video playback speed
    protected:
        /**
         * Update textures, can only be called by the video service
         */
        void update(double deltaTime) override;
    private:
        using Task = std::function<void()>;
        /**
         * Clear output textures to black
         */
        void clearTextures();

        bool mVideoLoaded = false;								///< If a video is currently loaded

        std::atomic_bool mRunning = false;							///< If the video is currently playing
        std::thread mThread;										///< Thread for video playback
        void onWork();

        void enqueueMainThreadTask(const Task& task){ mMainThreadJobs.enqueue(task); }

        void enqueueWorkThreadTask(const Task& task){ mWorkThreadJobs.enqueue(task); }

        moodycamel::ConcurrentQueue<Task> mWorkThreadJobs;	///< Work queue for the thread
        moodycamel::ConcurrentQueue<Task> mMainThreadJobs;	///< Work queue for the main thread

        struct Impl;
        std::unique_ptr<Impl> mImpl;

        nap::Video* mCurrentVideo = nullptr;					///< Current selected video context
        std::unique_ptr<nap::Video> mVideo;		                ///< The actual video

        double mCurrentTime = 0.0;								///< Current playback time in seconds
        double mDuration = 0.0;									///< Duration of the video in seconds
        glm::vec2 mVideoSize = glm::vec2(0.0f);					///< Size of the video in pixels
        bool mPlaying = false;									///< If the video is currently playing
        double mStartTime = 0.0;					            ///< Start time of the video in seconds
        bool mHasAudio = false;									///< If the video has an audio stream
    };

    // Object creator
    using ThreadedVideoPlayerObjectCreator = rtti::ObjectCreator<ThreadedVideoPlayer, VideoAdvancedService>;
}
