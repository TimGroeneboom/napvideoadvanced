/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Local Includes
#include "threadedvideoplayer.h"

// Local Includes
#include "videoservice.h"
#include "videoadvancedservice.h"

// External Includes
#include <mathutils.h>
#include <nap/assert.h>
#include <libavformat/avformat.h>
#include <nap/core.h>


RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::ThreadedVideoPlayer)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService &)
        RTTI_PROPERTY("Loop", &nap::ThreadedVideoPlayer::mLoop, nap::rtti::EPropertyMetaData::Default, "Loop the selected video")
        RTTI_PROPERTY("FilePath", &nap::ThreadedVideoPlayer::mFilePath, nap::rtti::EPropertyMetaData::Default | nap::rtti::EPropertyMetaData::FileLink, "Path to the video file, leave empty to not load file on init")
        RTTI_PROPERTY("Speed", &nap::ThreadedVideoPlayer::mSpeed, nap::rtti::EPropertyMetaData::Default, "Video playback speed")
RTTI_END_CLASS

//////////////////////////////////////////////////////////////////////////


namespace nap
{
    struct ThreadedVideoPlayer::Impl
    {
    public:
        moodycamel::ConcurrentQueue<Frame> mFrames;
    };

    ThreadedVideoPlayer::ThreadedVideoPlayer(VideoAdvancedService& service) :
            VideoPlayerAdvancedBase(service)
    { }


    ThreadedVideoPlayer::~ThreadedVideoPlayer()
    {
        assert(mCurrentVideo == nullptr);
    }


    int ThreadedVideoPlayer::getWidth() const
    {
        if (!mVideoLoaded)
            return 0;

        return mVideoSize.x;
    }


    int ThreadedVideoPlayer::getHeight() const
    {
        if (!mVideoLoaded)
            return 0;

        return mVideoSize.y;
    }


    double ThreadedVideoPlayer::getDuration() const
    {
        if (!mVideoLoaded)
            return 0.0;

        return mDuration;
    }


    float ThreadedVideoPlayer::getSpeed() const
    {
        return mSpeed;
    }


    bool ThreadedVideoPlayer::isLooping() const
    {
        return mLoop;
    }


    bool ThreadedVideoPlayer::isPlaying() const
    {
        return mPlaying;
    }


    bool ThreadedVideoPlayer::hasAudio() const
    {
        return mHasAudio;
    }


    void ThreadedVideoPlayer::seek(double seconds)
    {
        if (!mVideoLoaded)
            return;

        enqueueWorkThreadTask([this, seconds]()
        {
            if(mCurrentVideo != nullptr)
                mCurrentVideo->seek(seconds);
        });
    }


    double ThreadedVideoPlayer::getCurrentTime() const
    {
        if (!mVideoLoaded)
            return 0.0;

        return mCurrentTime;
    }


    void ThreadedVideoPlayer::loadVideo(const std::string& path)
    {
        mVideoLoaded = false;

        // Stop playback of current video if available
        enqueueWorkThreadTask([this, path]()
        {
            utility::ErrorState error;

            if(mCurrentVideo!= nullptr)
                mCurrentVideo->stop(true);

            mCurrentVideo = nullptr;

            auto new_video_file = std::make_unique<nap::VideoFile>();
            new_video_file->mPath = path;
            new_video_file->mID = math::generateUUID();
            if(!new_video_file->init(error))
            {
                nap::Logger::error("%s: Unable to load video for file: %s", mID.c_str(), path.c_str());
                return;
            }

            auto new_video = std::make_unique<nap::Video>(new_video_file->mPath, mNumThreads);
            if(!new_video->init(error))
            {
                nap::Logger::error("%s: Unable to load video for file: %s", mID.c_str(), path.c_str());
                return;
            }

            // Update selection
            mCurrentVideo = new_video.get();

            // Copy properties for playback
            mCurrentVideo->mLoop  = mLoop;
            mCurrentVideo->mSpeed = mSpeed;

            glm::vec2 size = { mCurrentVideo->getWidth(), mCurrentVideo->getHeight() };

            mVideo = std::move(new_video);

            double duration = mCurrentVideo->getDuration();
            bool has_audio = mCurrentVideo->hasAudio();
            int pix_fmt = new_video_file->getPixelFormat();

            enqueueMainThreadTask([this, duration, size, has_audio, pix_fmt]()
            {
                utility::ErrorState error;

                auto pixel_format_handler = utility::createVideoPixelFormatHandler(pix_fmt, mService, error);
                if(pixel_format_handler == nullptr)
                {
                    nap::Logger::error("%s: Unable to create pixel format handler", mID.c_str());

                    enqueueWorkThreadTask([this]()
                    {
                        mCurrentVideo = nullptr;
                        mVideo = nullptr;
                    });

                    return;
                }

                if(!pixel_format_handler->init(error))
                {
                    nap::Logger::error("%s: Unable to initialize pixel format handler", mID.c_str());

                    enqueueWorkThreadTask([this]()
                    {
                        mCurrentVideo = nullptr;
                        mVideo = nullptr;
                    });

                    return;
                }

                if(!pixel_format_handler->initTextures(size, error))
                {
                    nap::Logger::error("%s: Unable to initialize pixel format handler", mID.c_str());

                    enqueueWorkThreadTask([this]()
                    {
                        mCurrentVideo = nullptr;
                        mVideo = nullptr;
                    });

                    return;
                }

                mPixelFormatHandler = std::move(pixel_format_handler);
                onPixelFormatHandlerChanged(*mPixelFormatHandler);

                mVideoSize = size;
                mDuration = duration;
                mHasAudio = has_audio;
                mVideoLoaded = true;

                if(mPlaying)
                {
                    double start_time = mStartTime;
                    enqueueWorkThreadTask([this, start_time]()
                    {
                        mCurrentVideo->play(start_time);
                    });
                }
            });
        });
    }


    void ThreadedVideoPlayer::clearTextures()
    {
        if(hasPixelFormatHandler())
            mPixelFormatHandler->clearTextures();
    }


    bool ThreadedVideoPlayer::start(utility::ErrorState& errorState)
    {
        mImpl = std::make_unique<Impl>();

        if(!mFilePath.empty())
        {
            loadVideo(mFilePath);
        }

        mRunning = true;
        mThread = std::thread(&ThreadedVideoPlayer::onWork, this);

        // Register device
        mService.registerPlayer(*this);
        return true;
    }


    void ThreadedVideoPlayer::stopPlayback()
    {
        mPlaying = false;
    }

    void ThreadedVideoPlayer::play(double startTime, bool clearTheTextures)
    {
        mPlaying = true;
        mStartTime = startTime;

        // Clear textures and start playback
        if(clearTheTextures)
            clearTextures();

        if(!mVideoLoaded)
            return;

        enqueueWorkThreadTask([this]()
        {
            if(mCurrentVideo!= nullptr)
                mCurrentVideo->play(mStartTime);
        });
    }


    void ThreadedVideoPlayer::loop(bool value)
    {
        mLoop = value;

        enqueueWorkThreadTask([this, value]()
        {
            if(mCurrentVideo!= nullptr)
                mCurrentVideo->mLoop = value;
        });
    }


    void ThreadedVideoPlayer::setSpeed(float speed)
    {
        mSpeed = speed;

        enqueueWorkThreadTask([this, speed]()
        {
            if(mCurrentVideo!= nullptr)
                mCurrentVideo->mSpeed = speed;
        });
    }


    void ThreadedVideoPlayer::stop()
    {
        // Unregister player
        mService.removePlayer(*this);

        enqueueWorkThreadTask([this]()
        {
            // Clear all videos
            mCurrentVideo = nullptr;
            mRunning = false;
        });

        if(mThread.joinable())
            mThread.join();
    }


    void ThreadedVideoPlayer::onWork()
    {
        SteadyTimeStamp time_stamp = SteadyClock::now();
        while(mRunning)
        {
            if(mWorkThreadJobs.size_approx()>0)
            {
                Task task;
                while(mWorkThreadJobs.try_dequeue(task))
                    task();
            }

            // Calculate frame duration seconds
            SteadyTimeStamp current_time = SteadyClock::now();
            double delta_time = std::chrono::duration<double>(current_time - time_stamp).count();
            time_stamp = current_time;

            if(mCurrentVideo!= nullptr)
            {
                Frame frame = mCurrentVideo->update(delta_time);
                if(frame.isValid())
                    mImpl->mFrames.enqueue(frame);
                else
                    frame.free();

                double current_time_video = mCurrentVideo->getCurrentTime();
                bool is_playing = mCurrentVideo->isPlaying();
                enqueueMainThreadTask([this, current_time_video, is_playing]()
                {
                    mCurrentTime = current_time_video;
                    mPlaying = is_playing;
                });
            }

            // 100 fps = 10ms
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }


    void ThreadedVideoPlayer::update(double deltaTime)
    {
        if(mMainThreadJobs.size_approx()>0)
        {
            Task task;
            while(mMainThreadJobs.try_dequeue(task))
                task();
        }

        Frame frame;
        while(mImpl->mFrames.try_dequeue(frame))
        {
            if(frame.isValid() && mImpl->mFrames.size_approx() == 0)
                mPixelFormatHandler->update(frame);

            frame.free();
        }
    }
}
