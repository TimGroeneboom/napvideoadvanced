#pragma once

#include <nap/device.h>

#include "videopixelformathandler.h"

namespace nap
{
    /**
     * Base class for advanced video players. Advanced Video players have a pixel format handler to deal with
     * different video frame formats.
     */
    class NAPAPI VideoPlayerAdvancedBase : public Device
    {
    RTTI_ENABLE(Device)
    friend class VideoAdvancedService;
    public:
        /**
         * Constructor
         * @param service reference to the video service
         */
        VideoPlayerAdvancedBase(VideoAdvancedService& service);

        /**
         * The video player pixel format handler
         * @return reference to the pixel format handler
         */
        VideoPixelFormatHandlerBase &getPixelFormatHandler() { return *mPixelFormatHandler; }

        bool hasPixelFormatHandler() const { return mPixelFormatHandler != nullptr; }

        // Properties
        int mNumThreads = 0;	///< Property: 'NumThreads' number of threads to use for decoding. 0 means automatic.

        // Signals
        Signal<VideoPixelFormatHandlerBase&> onPixelFormatHandlerChanged;	///< Signal that is emitted when the pixel format handler changes
    protected:
        /**
         * Called by the video service to update the video player
         * @param deltaTime the time in seconds between calls
         */
        virtual void update(double deltaTime) = 0;

        // Reference to the video service
        VideoAdvancedService &mService;

        // Pixel format handler
        std::unique_ptr<VideoPixelFormatHandlerBase> mPixelFormatHandler;
    };
}