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
        RTTI_PROPERTY("VideoFiles", &nap::VideoPlayerAdvanced::mVideoFiles, nap::rtti::EPropertyMetaData::Embedded, "All video files")
        RTTI_PROPERTY("VideoIndex", &nap::VideoPlayerAdvanced::mVideoIndex, nap::rtti::EPropertyMetaData::Default, "Selected video file index")
        RTTI_PROPERTY("Speed", &nap::VideoPlayerAdvanced::mSpeed, nap::rtti::EPropertyMetaData::Default, "Video playback speed")
RTTI_END_CLASS

//////////////////////////////////////////////////////////////////////////


namespace nap
{
    VideoPlayerAdvancedBase::VideoPlayerAdvancedBase(VideoAdvancedService& service) :
            mService(service)
    { }


    VideoPlayerAdvanced::VideoPlayerAdvanced(VideoAdvancedService& service) :
            VideoPlayerAdvancedBase(service)
    { }


    const nap::VideoFile& VideoPlayerAdvanced::getFile() const
    {
        assert(mCurrentVideoIndex < mVideoFiles.size());
        return *mVideoFiles[mCurrentVideoIndex];
    }


    const nap::Video& VideoPlayerAdvanced::getVideo() const
    {
        NAP_ASSERT_MSG(mCurrentVideo != nullptr, "No video selected");
        return *mCurrentVideo;
    }


    nap::Video& VideoPlayerAdvanced::getVideo()
    {
        NAP_ASSERT_MSG(mCurrentVideo != nullptr, "No video selected");
        return *mCurrentVideo;
    }


    bool VideoPlayerAdvanced::selectVideo(int index, utility::ErrorState& error)
    {
        // Update video index, bail if it's the same as we have currently selected
        NAP_ASSERT_MSG(!mVideos.empty(), "No video contexts available, call start() before video selection");
        int new_idx = math::clamp<int>(index, 0, mVideos.size() - 1);
        if (mVideos[new_idx].get() == mCurrentVideo)
            return true;

        // Stop playback of current video if available
        if (mCurrentVideo != nullptr)
            mCurrentVideo->stop(true);

        // Update selection
        mCurrentVideo = mVideos[new_idx].get();
        mCurrentVideoIndex = new_idx;

        // Store new width and height
        float vid_x = mCurrentVideo->getWidth();
        float vid_y = mCurrentVideo->getHeight();

        // Copy properties for playback
        mCurrentVideo->mLoop  = mLoop;
        mCurrentVideo->mSpeed = mSpeed;

        // Check if textures need to be generated, this is the case when there are none,
        // or when the dimensions have changed
        if (!error.check(vid_x == mTexture->getWidth() && vid_y == mTexture->getHeight(),
                         "Texture dimensions do not match video dimensions"))
            return false;

        return true;
    }


    void VideoPlayerAdvanced::clearTextures()
    {
    }


    bool VideoPlayerAdvanced::start(utility::ErrorState& errorState)
    {
        // Ensure there's at least 1 video
        if (!errorState.check(mVideoFiles.size() > 0, "Playlist is empty"))
            return false;

        // Create all the unique video objects
        mVideos.clear();
        for (const auto& file : mVideoFiles)
        {
            // Create video and initialize
            std::unique_ptr<nap::Video> new_video = std::make_unique<nap::Video>(file->mPath);
            if (!new_video->init(errorState))
            {
                errorState.fail("%s: Unable to load video for file: %s", mID.c_str(), file->mPath.c_str());
                return false;
            }
            mVideos.emplace_back(std::move(new_video));
        }

        // Now select video, creates textures if required
        if (!selectVideo(mVideoIndex, errorState))
            return false;

        // Register device
        mService.registerPlayer(*this);
        return true;
    }


    void VideoPlayerAdvanced::play(double mStartTime, bool clearTheTextures)
    {
        // Clear textures and start playback
        if(clearTheTextures)
            clearTextures();

        getVideo().play(mStartTime);
    }


    void VideoPlayerAdvanced::loop(bool value)
    {
        mLoop = value;
        getVideo().mLoop = mLoop;
    }


    void VideoPlayerAdvanced::setSpeed(float speed)
    {
        mSpeed = speed;
        getVideo().mSpeed = speed;
    }


    void VideoPlayerAdvanced::stop()
    {
        // Unregister player
        mService.removePlayer(*this);

        // Clear all videos
        mVideos.clear();
        mCurrentVideo = nullptr;
        mCurrentVideoIndex = 0;
    }


    void VideoPlayerAdvanced::update(double deltaTime)
    {
        // Bail if there's no selection or playback is disabled
        if (mCurrentVideo == nullptr || !mCurrentVideo->isPlaying())
            return;

        // Get frame and update contents
        Frame new_frame = mCurrentVideo->update(deltaTime);
        if (new_frame.isValid())
        {
            // Copy data into texture
            assert(mTexture != nullptr);
            mTexture->update(new_frame.mFrame->data[0], mTexture->getWidth(), mTexture->getHeight(), new_frame.mFrame->linesize[0], ESurfaceChannels::RGBA);
        }

        // Destroy frame that was allocated in the decode thread, after it has been processed
        new_frame.free();
    }
}
