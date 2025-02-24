/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Local Includes
#include "videoplayeradvanced.h"

// External Includes
#include <component.h>
#include <rendercomponent.h>
#include <nap/resourceptr.h>
#include <rendertexture2d.h>
#include <planemesh.h>
#include <rendertarget.h>
#include <color.h>
#include <materialinstance.h>
#include <renderablemesh.h>

namespace nap
{
    // Forward Declares
    class RenderVideoAdvancedComponentInstance;

    class NAPAPI RenderVideoAdvancedComponent : public RenderableComponent
    {
    RTTI_ENABLE(RenderableComponent)
    DECLARE_COMPONENT(RenderVideoAdvancedComponent, RenderVideoAdvancedComponentInstance)
    public:
        ResourcePtr<VideoPlayerAdvancedBase>		mVideoPlayer = nullptr;								///< Property: 'VideoPlayer' the video player to render to texture
        ResourcePtr<RenderTexture2D>	            mOutputTexture = nullptr;							///< Property: 'OutputTexture' the RGB8 texture to render output to
        ERasterizationSamples			            mRequestedSamples = ERasterizationSamples::One;		///< Property: 'Samples' The number of samples used during Rasterization. For better results enable 'SampleShading'
        RGBAColor8						            mClearColor = { 255, 255, 255, 255 };				///< Property: 'ClearColor' the color that is used to clear the render target
    };


    class NAPAPI RenderVideoAdvancedComponentInstance : public RenderableComponentInstance
    {
    RTTI_ENABLE(RenderableComponentInstance)
    public:
        RenderVideoAdvancedComponentInstance(EntityInstance& entity, Component& resource);

        /**
         * Initializes the component based on resource.
         * @param errorState contains the error if initialization fails.
         * @return if initialization succeeded.
         */
        virtual bool init(utility::ErrorState& errorState) override;

        /**
         * Called by the Render Service. Only orthographic cameras are supported.
         */
        virtual bool isSupported(nap::CameraComponentInstance& camera) const override;

        /**
         * Returns the rendered RGB video texture.
         * @return the rendered RGB video texture.
         */
        Texture2D& getOutputTexture();

        /**
         * Renders the output of a nap::VideoPlayer directly to texture.
         * This components converts the YUV textures, generated by the nap::VideoPlayer, into an RGB texture.
         * Call this in your application render() call, in between nap::RenderService::beginHeadlessRecording() and
         * nap::RenderService::endHeadlessRecording(). Do not call this function outside
         * of a headless recording pass, ie: when rendering to a window.
         * Alternatively, you can use the render service to render this component, see onDraw()
         */
        void draw();

    protected:
        /**
         * Draws the video frame full screen to the currently active render target,
         * when the view matrix = identity.
         * @param renderTarget the target to render to.
         * @param commandBuffer the currently active command buffer.
         * @param viewMatrix often the camera world space location
         * @param projectionMatrix often the camera projection matrix
        */
        virtual void onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) override;

    private:
        VideoPlayerAdvancedBase*	mPlayer = nullptr;								///< Video player to render
        RenderTexture2D*			mOutputTexture = nullptr;						///< Texture currently bound by target
        RGBColorFloat				mClearColor = { 0.0f, 0.0f, 0.0f };				///< Target Clear Color
        RenderTarget				mTarget;										///< Target video is rendered into
        PlaneMesh					mPlane;											///< Plane that is rendered
        RenderableMesh				mRenderableMesh;								///< Valid Plane / Material combination
        RenderService*				mRenderService = nullptr;						///< Pointer to the render service
        bool						mDirty = true;									///< If the model matrix needs to be re-computed
        bool                        mValid = false;                                 ///< If the component is valid

        void onPixelFormatHandlerChanged(VideoPixelFormatHandlerBase& pixelFormatHandler);
        Slot<VideoPixelFormatHandlerBase&> mPixelFormatHandlerChangedSlot = { this, &RenderVideoAdvancedComponentInstance::onPixelFormatHandlerChanged };

    };
}
