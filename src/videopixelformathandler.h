#pragma once

#include <nap/resource.h>
#include <video.h>
#include <texture.h>
#include <materialinstance.h>

namespace nap
{
    // Forward declares
    class VideoAdvancedService;

    /**
     * Base class for video pixel format handlers. Video pixel format handlers are used to handle different video frame formats.
     *
     * Also, a RenderVideoAdvancedComponentInstance uses a VideoPixelFormatHandler to render video frames.
     */
    class NAPAPI VideoPixelFormatHandlerBase : public Resource
    {
        friend class RenderVideoAdvancedComponentInstance;

        RTTI_ENABLE(Resource)
    public:
        /**
         * Constructor
         * @param service reference to the video service
         */
        explicit VideoPixelFormatHandlerBase(VideoAdvancedService& service);

        /**
         * Initializes the materials
         * @param errorState reference to the error state containing the error message on failure
         * @return true if the materials were initialized correctly
         */
        bool init(utility::ErrorState& errorState) override = 0;

        /**
         * Initializes the textures, called by the video player, can be called multiple times
         * @param size the size of the textures
         * @param errorState reference to the error state containing the error message on failure
         * @return true if the textures were initialized correctly
         */
        virtual bool initTextures(const glm::vec2& size, utility::ErrorState& errorState) = 0;

        /**
         * Clears the textures
         */
        virtual void clearTextures() = 0;

        /**
         * Updates the textures with the new video frame
         * @param frame the video frame to update
         */
        virtual void update(Frame& frame) = 0;
    protected:
        UniformMat4Instance* ensureUniform(const std::string& uniformName, utility::ErrorState& error);

        Sampler2DInstance* ensureSampler(const std::string& samplerName, utility::ErrorState& error);

        VideoAdvancedService& mService;

        MaterialInstance			mMaterialInstance;								///< The MaterialInstance as created from the resource.
        MaterialInstanceResource	mMaterialInstanceResource;						///< Resource used to initialize the material instance
        UniformMat4Instance*		mModelMatrixUniform = nullptr;					///< Model matrix uniform in the material
        UniformMat4Instance*		mProjectMatrixUniform = nullptr;				///< Projection matrix uniform in the material
        UniformMat4Instance*		mViewMatrixUniform = nullptr;					///< View matrix uniform in the material
        UniformStructInstance*		mMVPStruct = nullptr;							///< model view projection struct
        glm::mat4x4					mModelMatrix;									///< Computed model matrix, used to scale plane to fit target bounds
    };

    //////////////////////////////////////////////////////////////////////////
    //// RGBA Pixel Format Handler
    //////////////////////////////////////////////////////////////////////////

    /**
     * Video pixel format handler for RGBA pixel format.
     */
    class NAPAPI VideoPixelFormatRGBAHandler final : public VideoPixelFormatHandlerBase
    {
        RTTI_ENABLE(VideoPixelFormatHandlerBase)
    public:
        /**
         * Constructor
         * @param service reference to the video service
         */
        explicit VideoPixelFormatRGBAHandler(VideoAdvancedService& service);

        /**
         * Initializes the materials
         * @param errorState reference to the error state containing the error message on failure
         * @return true if the materials were initialized correctly
         */
        bool init(utility::ErrorState& errorState) override;

        /**
         * Initializes the textures, called by the video player, can be called multiple times
         * @param size the size of the textures
         * @param errorState reference to the error state containing the error message on failure
         * @return true if the textures were initialized correctly
         */
        bool initTextures(const glm::vec2& size, utility::ErrorState& errorState) override;

        /**
         * Clears the textures
         */
        void clearTextures() override;

        /**
         * Updates the textures with the new video frame
         * @param frame the video frame to update
         */
        void update(Frame& frame) override;
    private:
        std::unique_ptr<Texture2D> mTexture;    ///< Texture used to render the video frame
        Sampler2DInstance* mSampler = nullptr;  ///< Sampler used to sample the texture in the material
    };

    using VideoPixelFormatRGBAHandlerObjectCreator = rtti::ObjectCreator<VideoPixelFormatRGBAHandler, VideoAdvancedService>;

    //////////////////////////////////////////////////////////////////////////
    //// YUV 8 Pixel Format Handler
    //////////////////////////////////////////////////////////////////////////

    /**
     * Video pixel format handler for YUV 8 pixel format.
     */
    class NAPAPI VideoPixelFormatYUV8Handler final : public VideoPixelFormatHandlerBase
    {
        RTTI_ENABLE(VideoPixelFormatHandlerBase)
    public:
        /**
         * Constructor
         * @param service reference to the video service
         */
        explicit VideoPixelFormatYUV8Handler(VideoAdvancedService& service);

        /**
         * Initializes the materials
         * @param errorState reference to the error state containing the error message on failure
         * @return true if the materials were initialized correctly
         */
        bool init(utility::ErrorState& errorState) override;

        /**
         * Initializes the textures, called by the video player, can be called multiple times
         * @param size the size of the textures
         * @param errorState reference to the error state containing the error message on failure
         * @return true if the textures were initialized correctly
         */
        bool initTextures(const glm::vec2& size, utility::ErrorState& errorState) override;

        /**
         * Clears the textures
         */
        void clearTextures() override;

        /**
         * Updates the textures with the new video frame
         * @param frame the video frame to update
         */
        void update(Frame& frame) override;
    private:
        std::unique_ptr<Texture2D> mYTexture;   ///< Y texture used to render the video frame
        std::unique_ptr<Texture2D> mUTexture;   ///< U texture used to render the video frame
        std::unique_ptr<Texture2D> mVTexture;   ///< V texture used to render the video frame
        Sampler2DInstance* mYSampler = nullptr; ///< Y sampler used to sample the Y texture in the material
        Sampler2DInstance* mUSampler = nullptr; ///< U sampler used to sample the U texture in the material
        Sampler2DInstance* mVSampler = nullptr; ///< V sampler used to sample the V texture in the material
    };

    using VideoPixelFormatYUV8HandlerObjectCreator = rtti::ObjectCreator<VideoPixelFormatYUV8Handler, VideoAdvancedService>;

    //////////////////////////////////////////////////////////////////////////
    //// YUV 16 Pixel Format Handler
    //////////////////////////////////////////////////////////////////////////

    /**
     * Video pixel format handler for YUV 16 pixel format.
     */
    class NAPAPI VideoPixelFormatYUV16Handler final : public VideoPixelFormatHandlerBase
    {
    RTTI_ENABLE(VideoPixelFormatHandlerBase)
    public:
        /**
         * Constructor
         * @param service reference to the video service
         */
        explicit VideoPixelFormatYUV16Handler(VideoAdvancedService& service);

        /**
         * Initializes the materials
         * @param errorState reference to the error state containing the error message on failure
         * @return true if the materials were initialized correctly
         */
        bool init(utility::ErrorState& errorState) override;

        /**
         * Initializes the textures, called by the video player, can be called multiple times
         * @param size the size of the textures
         * @param errorState reference to the error state containing the error message on failure
         * @return true if the textures were initialized correctly
         */
        bool initTextures(const glm::vec2& size, utility::ErrorState& errorState) override;

        /**
         * Clears the textures
         */
        void clearTextures() override;

        /**
         * Updates the textures with the new video frame
         * @param frame the video frame to update
         */
        void update(Frame& frame) override;
    private:
        std::unique_ptr<Texture2D> mYTexture;   ///< Y texture used to render the video frame
        std::unique_ptr<Texture2D> mUTexture;   ///< U texture used to render the video frame
        std::unique_ptr<Texture2D> mVTexture;   ///< V texture used to render the video frame
        Sampler2DInstance* mYSampler = nullptr; ///< Y sampler used to sample the Y texture in the material
        Sampler2DInstance* mUSampler = nullptr; ///< U sampler used to sample the U texture in the material
        Sampler2DInstance* mVSampler = nullptr; ///< V sampler used to sample the V texture in the material
    };

    using VideoPixelFormatYUV16HandlerObjectCreator = rtti::ObjectCreator<VideoPixelFormatYUV16Handler, VideoAdvancedService>;
}