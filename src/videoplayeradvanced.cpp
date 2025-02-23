/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Local Includes
#include "videoplayeradvanced.h"

// Local Includes
#include "videoservice.h"
#include "videoadvancedservice.h"

// External Includes
#include <mathutils.h>
#include <nap/assert.h>
#include <libavformat/avformat.h>
#include <nap/core.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPlayerAdvancedBase)
RTTI_END_CLASS

// nap::videoplayer run time class definition
RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPlayerAdvanced)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService &)
        RTTI_PROPERTY("Loop", &nap::VideoPlayerAdvanced::mLoop, nap::rtti::EPropertyMetaData::Default, "Loop the selected video")
        RTTI_PROPERTY("FilePath", &nap::VideoPlayerAdvanced::mFilePath, nap::rtti::EPropertyMetaData::Default | nap::rtti::EPropertyMetaData::FileLink, "Path to the video file, leave empty to not load file on init")
        RTTI_PROPERTY("Speed", &nap::VideoPlayerAdvanced::mSpeed, nap::rtti::EPropertyMetaData::Default, "Video playback speed")
RTTI_END_CLASS

//////////////////////////////////////////////////////////////////////////


namespace nap
{


    VideoPlayerAdvanced::VideoPlayerAdvanced(VideoAdvancedService& service) :
            VideoPlayerAdvancedBase(service)
    { }


    bool VideoPlayerAdvanced::hasVideo() const
    {
        return mCurrentVideo != nullptr;
    }


    int VideoPlayerAdvanced::getWidth() const
    {
        if (!hasVideo())
            return 0;

        return mCurrentVideo->getWidth();
    }


    int VideoPlayerAdvanced::getHeight() const
    {
        if (!hasVideo())
            return 0;

        return mCurrentVideo->getHeight();
    }


    double VideoPlayerAdvanced::getDuration() const
    {
        if (!hasVideo())
            return 0.0;

        return mCurrentVideo->getDuration();
    }


    float VideoPlayerAdvanced::getSpeed() const
    {
        return mSpeed;
    }


    bool VideoPlayerAdvanced::isLooping() const
    {
        return mLoop;
    }


    void VideoPlayerAdvanced::seek(double seconds)
    {
        if (!hasVideo())
            return;

        mCurrentVideo->seek(seconds);
    }


    double VideoPlayerAdvanced::getCurrentTime() const
    {
        if (!hasVideo())
            return 0.0;

        return mCurrentVideo->getCurrentTime();
    }


    bool VideoPlayerAdvanced::loadVideo(const std::string& path, utility::ErrorState& error)
    {
        // Stop playback of current video if available
        if (hasVideo())
            mCurrentVideo->stop(true);

        mCurrentVideo = nullptr;

        auto new_video_file = std::make_unique<nap::VideoFile>();
        new_video_file->mPath = path;
        new_video_file->mID = math::generateUUID();
        if(!new_video_file->init(error))
        {
            error.fail("%s: Unable to load video for file: %s", mID.c_str(), path.c_str());
            return false;
        }

        auto new_video = std::make_unique<nap::Video>(new_video_file->mPath);
        if(!new_video->init(error))
        {
            error.fail("%s: Unable to load video for file: %s", mID.c_str(), path.c_str());
            return false;
        }

        mPixelFormatHandler = utility::createVideoPixelFormatHandler(new_video_file->getPixelFormat(), mService, error);
        if(mPixelFormatHandler == nullptr)
        {
            error.fail("%s: Unable to create pixel format handler", mID.c_str());
            return false;
        }

        if(!mPixelFormatHandler->init(error))
            return false;

        if(!mPixelFormatHandler->initTextures({ mCurrentVideo->getWidth(), mCurrentVideo->getHeight() }, error))
            return false;

        //
        onPixelFormatHandlerChanged(*mPixelFormatHandler);

        // Update selection
        mCurrentVideo = new_video.get();

        // Copy properties for playback
        mCurrentVideo->mLoop  = mLoop;
        mCurrentVideo->mSpeed = mSpeed;

        mVideo = std::move(new_video);

        return true;
    }


    void VideoPlayerAdvanced::stopPlayback()
    {
        if (!hasVideo())
            return;

        mCurrentVideo->stop(true);
    }


    bool VideoPlayerAdvanced::hasAudio() const
    {
        return hasVideo() && mCurrentVideo->hasAudio();
    }


    bool VideoPlayerAdvanced::isPlaying() const
    {
        return hasVideo() && mCurrentVideo->isPlaying();
    }


    void VideoPlayerAdvanced::clearTextures()
    {
        if(hasPixelFormatHandler())
            mPixelFormatHandler->clearTextures();
    }


    bool VideoPlayerAdvanced::start(utility::ErrorState& errorState)
    {
        if(!mFilePath.empty())
        {
            utility::ErrorState error;
            if (!loadVideo(mFilePath, error))
            {
                nap::Logger::error(error.toString());
            }
        }

        // Register device
        mService.registerPlayer(*this);
        return true;
    }


    void VideoPlayerAdvanced::play(double mStartTime, bool clearTheTextures)
    {
        if(!hasVideo())
            return;

        // Clear textures and start playback
        if(clearTheTextures)
            clearTextures();

        mCurrentVideo->play(mStartTime);
    }


    void VideoPlayerAdvanced::loop(bool value)
    {
        mLoop = value;

        if(!hasVideo())
            return;

        mCurrentVideo->mLoop = mLoop;
    }


    void VideoPlayerAdvanced::setSpeed(float speed)
    {
        mSpeed = speed;

        if(!hasVideo())
            return;

        mCurrentVideo->mSpeed = speed;
    }


    void VideoPlayerAdvanced::stop()
    {
        // Unregister player
        mService.removePlayer(*this);

        // Clear all videos
        mCurrentVideo = nullptr;
    }


    void VideoPlayerAdvanced::update(double deltaTime)
    {
        // Bail if there's no selection or playback is disabled
        if (!hasVideo() || !mCurrentVideo->isPlaying())
            return;

        // Get frame and update contents
        Frame new_frame = mCurrentVideo->update(deltaTime);
        if (new_frame.isValid())
        {
            mPixelFormatHandler->update(new_frame);
        }

        // Destroy frame that was allocated in the decode thread, after it has been processed
        new_frame.free();
    }
}
