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

        enqueueWorkTask([this, seconds]()
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
        // current video is not loaded
        // if a video is loaded will be stopped and unloaded in the next cycle of the worker thread
        mVideoLoaded = false;

        enqueueWorkTask([this, path]()
        {
            utility::ErrorState error;

            // stop current video
            if(mCurrentVideo!= nullptr)
                mCurrentVideo->stop(true);

            // delete current video
            mCurrentVideo = nullptr;

            // Load video and initialize
            auto new_video_file = std::make_unique<nap::VideoFile>();
            new_video_file->mPath = path;
            new_video_file->mID = math::generateUUID();
            if(!new_video_file->init(error))
            {
                nap::Logger::error("%s: Unable to load video for file: %s", mID.c_str(), path.c_str());
                return;
            }

            // VideoFile has valid path & pixel format
            // Proceed to load video
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

            // copy some properties to the main thread
            double duration = mCurrentVideo->getDuration(); // get duration
            bool has_audio = mCurrentVideo->hasAudio(); // check if video has audio
            int pix_fmt = new_video_file->getPixelFormat(); // get pixel format

            // proceed creating pixel format handler on main thread if necessary
            enqueueMainTask([this, duration, size, has_audio, pix_fmt]()
            {
                // error state
                utility::ErrorState error;

                // pixel handler unique & raw pointer
                std::unique_ptr<VideoPixelFormatHandlerBase> new_pixel_format_handler = nullptr;
                VideoPixelFormatHandlerBase* pixel_format_handler_ptr = nullptr;

                // determine if we need to create a new pixel format handler
                // either the current pixel format handler is null or the pixel format is different
                // in that case, we need to create a new pixel format handler
                bool create_new_pixel_format_handler = false;
                if(mPixelFormatHandler!= nullptr)
                {
                    rtti::TypeInfo pixel_format_handler_type = mPixelFormatHandler->get_type();
                    if(utility::getVideoPixelFormatHandlerType(pix_fmt, pixel_format_handler_type, error))
                    {
                        if(pixel_format_handler_type != mPixelFormatHandler->get_type())
                            create_new_pixel_format_handler = true;
                    }
                }else
                {
                    create_new_pixel_format_handler = true;
                }

                // if we need to create a new pixel format handler proceed doing so
                if(create_new_pixel_format_handler)
                {
                    // create the new pixel handler and initialize it, if it fails, stop the video on worker thread
                    // and delete it
                    new_pixel_format_handler = utility::createVideoPixelFormatHandler(pix_fmt, mService, error);
                    if(new_pixel_format_handler == nullptr)
                    {
                        nap::Logger::error("%s: Unable to create pixel format handler", mID.c_str());

                        enqueueWorkTask([this]()
                        {
                            mCurrentVideo = nullptr;
                            mVideo = nullptr;
                        });

                        return;
                    }

                    // initialize the pixel format handler
                    // if it fails, stop the video on worker thread and delete it
                    if(!new_pixel_format_handler->init(error))
                    {
                        nap::Logger::error("%s: Unable to initialize pixel format handler", mID.c_str());

                        enqueueWorkTask([this]()
                        {
                            mCurrentVideo = nullptr;
                            mVideo = nullptr;
                        });

                        return;
                    }

                    // get the raw pointer of the new pixel format handler
                    pixel_format_handler_ptr = new_pixel_format_handler.get();
                }else
                {
                    // get the raw pointer of the current pixel format handler
                    pixel_format_handler_ptr = mPixelFormatHandler.get();
                }

                // initialize the textures of the pixel format handler, this will delete and create new textures
                // if the size of the video has changed
                // if it fails, stop the video on worker thread and delete it
                if(!pixel_format_handler_ptr->initTextures(size, error))
                {
                    nap::Logger::error("%s: Unable to initialize pixel format handler", mID.c_str());

                    enqueueWorkTask([this]()
                    {
                        mCurrentVideo = nullptr;
                        mVideo = nullptr;
                    });

                    return;
                }

                // if we created a new pixel format handler, move ownership and notify any listeners (like the render component)
                // that the pixel format handler has changed
                if(create_new_pixel_format_handler)
                {
                    mPixelFormatHandler = std::move(new_pixel_format_handler);
                    onPixelFormatHandlerChanged(*mPixelFormatHandler);
                }

                // copy some properties to the main thread
                mVideoSize = size;
                mDuration = duration;
                mHasAudio = has_audio;
                mVideoLoaded = true;

                // start playback if necessary
                if(mPlaying)
                {
                    double start_time = mStartTime;
                    enqueueWorkTask([this, start_time]()
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

        enqueueWorkTask([this]()
        {
            if(mCurrentVideo!= nullptr)
                mCurrentVideo->play(mStartTime);
        });
    }


    void ThreadedVideoPlayer::loop(bool value)
    {
        mLoop = value;

        enqueueWorkTask([this, value]()
        {
            if(mCurrentVideo!= nullptr)
                mCurrentVideo->mLoop = value;
        });
    }


    void ThreadedVideoPlayer::setSpeed(float speed)
    {
        mSpeed = speed;

        enqueueWorkTask([this, speed]()
        {
            if(mCurrentVideo!= nullptr)
                mCurrentVideo->mSpeed = speed;
        });
    }


    void ThreadedVideoPlayer::stop()
    {
        // Unregister player
        mService.removePlayer(*this);

        // stop video playback on worker thread
        enqueueWorkTask([this]()
        {
            // Clear all videos
            mCurrentVideo = nullptr;
            mRunning = false;
        });

        // wait for worker thread to finish
        while(!mWorkDone)
        {
            mUpdateWorker = true;
            mWorkSignal.notify_one();
        }

        // join worker thread
        if(mThread.joinable())
            mThread.join();
    }


    void ThreadedVideoPlayer::onWork()
    {
        mWorkDone = false;
        SteadyTimeStamp time_stamp = SteadyClock::now();
        while(mRunning)
        {
            // Update worker
            mUpdateWorker = false;

            // Execute queued tasks
            if(mWorkThreadTasks.size_approx() > 0)
            {
                Task task;
                while(mWorkThreadTasks.try_dequeue(task))
                    task();
            }

            // Calculate frame duration seconds
            SteadyTimeStamp current_time = SteadyClock::now();
            double delta_time = std::chrono::duration<double>(current_time - time_stamp).count();
            time_stamp = current_time;

            // Update video
            if(mCurrentVideo!= nullptr)
            {
                // Update video and get frame
                // if frame is valid, enqueue it to the main thread for processing
                Frame frame = mCurrentVideo->update(delta_time);
                if(frame.isValid())
                    mImpl->mFrames.enqueue(frame);
                else
                    frame.free();

                // Update current time and playing state on main thread
                double current_time_video = mCurrentVideo->getCurrentTime();
                bool is_playing = mCurrentVideo->isPlaying();
                enqueueMainTask([this, current_time_video, is_playing]()
                {
                    mCurrentTime = current_time_video;
                    mPlaying = is_playing;
                });
            }

            // wait for update signal coming from the main thread
            std::unique_lock<std::mutex> lock(mMutex);
            mWorkSignal.wait(lock, [this] { return mUpdateWorker.load() || !mRunning.load(); });
        }

        mWorkDone = true;
    }


    void ThreadedVideoPlayer::update(double deltaTime)
    {
        // Execute queued tasks queued from the worker thread
        if(mMainThreadTasks.size_approx() > 0)
        {
            Task task;
            while(mMainThreadTasks.try_dequeue(task))
                task();
        }

        // Process new frames
        Frame frame;
        while(mImpl->mFrames.try_dequeue(frame))
        {
            // only process last valid frame
            // this is to avoid processing frames that are not in sync with the main thread
            if(frame.isValid() && mImpl->mFrames.size_approx() == 0)
                mPixelFormatHandler->update(frame);

            frame.free();
        }

        // keep worker thread in lockstep with main thread
        // signal worker thread to update
        mUpdateWorker = true;
        mWorkSignal.notify_one();
    }
}
