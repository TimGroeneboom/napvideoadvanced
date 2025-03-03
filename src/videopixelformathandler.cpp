#include "videopixelformathandler.h"
#include "videoadvancedservice.h"
#include "videorgbashader.h"
#include "renderglobals.h"

#include <video.h>
#include <nap/core.h>
#include <renderservice.h>
#include <videoshader.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include "libswresample/swresample.h"
}

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPixelFormatHandlerBase)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPixelFormatRGBAP8Handler)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService&, int)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPixelFormatYUV420P8Handler)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService&, int)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPixelFormatYUV444P16Handler)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService&, int)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPixelFormatYUV420P16Handler)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService&, int)
RTTI_END_CLASS

namespace nap
{
    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatHandlerBase
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatHandlerBase::VideoPixelFormatHandlerBase(VideoAdvancedService& service, int pixelFormat) :
            mService(service), mPixelFormat(pixelFormat)
    { }


    nap::UniformMat4Instance* VideoPixelFormatHandlerBase::ensureUniform(const std::string& uniformName, utility::ErrorState& error)
    {
        assert(mMVPStruct != nullptr);
        auto* found_uniform = mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniformName);
        if (!error.check(found_uniform != nullptr,
                         "unable to find uniform: %s in material: %s", uniformName.c_str(),
                         mMaterialInstance.getMaterial().mID.c_str()))
            return nullptr;
        return found_uniform;
    }


    nap::Sampler2DInstance* VideoPixelFormatHandlerBase::ensureSampler(const std::string& samplerName, utility::ErrorState& error)
    {
        auto* found_sampler = mMaterialInstance.getOrCreateSampler<Sampler2DInstance>(samplerName);
        if (!error.check(found_sampler != nullptr,
                         "unable to find sampler: %s in material: %s", samplerName.c_str(),
                         mMaterialInstance.getMaterial().mID.c_str()))
            return nullptr;
        return found_sampler;
    }


    bool VideoPixelFormatHandlerBase::init(utility::ErrorState& errorState)
    {
        // Extract render service
        auto* render_service = mService.getCore().getService<RenderService>();
        assert(render_service != nullptr);

        // Get video material
        Material* video_material = getOrCreateMaterial(errorState);
        if (!errorState.check(video_material != nullptr, "unable to get or create video material"))
            return false;

        // Create resource for the video material instance
        mMaterialInstanceResource.mBlendMode = EBlendMode::Opaque;
        mMaterialInstanceResource.mDepthMode = EDepthMode::NoReadWrite;
        mMaterialInstanceResource.mMaterial  = video_material;

        // Initialize video material instance, used for rendering video
        if (!mMaterialInstance.init(*render_service, mMaterialInstanceResource, errorState))
            return false;

        // Ensure the mvp struct is available
        mMVPStruct = mMaterialInstance.getOrCreateUniform(uniform::mvpStruct);
        if (!errorState.check(mMVPStruct != nullptr, "Unable to find uniform MVP struct: %s in material: %s",
                              uniform::mvpStruct, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        // Get all matrices
        mModelMatrixUniform = ensureUniform(uniform::modelMatrix, errorState);
        mProjectMatrixUniform = ensureUniform(uniform::projectionMatrix, errorState);
        mViewMatrixUniform = ensureUniform(uniform::viewMatrix, errorState);

        if (mModelMatrixUniform == nullptr || mProjectMatrixUniform == nullptr || mViewMatrixUniform == nullptr)
            return false;

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatRGBAP8Handler
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatRGBAP8Handler::VideoPixelFormatRGBAP8Handler(VideoAdvancedService& service, int pixelFormat) :
            VideoPixelFormatHandlerBase(service, pixelFormat)
    { }


    bool VideoPixelFormatRGBAP8Handler::init(utility::ErrorState& errorState)
    {
        if(!VideoPixelFormatHandlerBase::init(errorState))
            return false;

        // Initialize texture with dummy data
        if (!initTextures({1, 1}, errorState))
            return false;

        // Get sampler inputs to update from video material
        mSampler = ensureSampler(uniform::videorgba::sampler::RGBASampler, errorState);
        mSampler->setTexture(*mTexture);

        if (!errorState.check(mSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::videorgba::sampler::RGBASampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        return true;
    }


    bool VideoPixelFormatRGBAP8Handler::initTextures(const glm::vec2& size, utility::ErrorState& errorState)
    {
        if(mTexture == nullptr || mTexture->getWidth() != static_cast<uint32_t>(size.x) || mTexture->getHeight() != static_cast<uint32_t>(size.y))
        {
            // Create texture description
            SurfaceDescriptor tex_description;
            tex_description.mWidth = static_cast<uint32_t>(size.x);
            tex_description.mHeight = static_cast<uint32_t>(size.y);
            tex_description.mColorSpace = EColorSpace::Linear;
            tex_description.mDataType = ESurfaceDataType::BYTE;
            tex_description.mChannels = ESurfaceChannels::RGBA;

            mTexture = std::make_unique<Texture2D>(mService.getCore());
            mTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mTexture->init(tex_description, false, 0, errorState))
                return false;
        }

        if(mSampler!= nullptr)
            mSampler->setTexture(*mTexture);

        return true;
    }


    Material* VideoPixelFormatRGBAP8Handler::getOrCreateMaterial(utility::ErrorState& errorState)
    {
        return mService.getCore().getService<RenderService>()->getOrCreateMaterial<VideoRGBAShader>(errorState);
    }


    void VideoPixelFormatRGBAP8Handler::clearTextures()
    {
        if(!mTexture)
            return;

        std::vector<uint8_t> y_default_data(mTexture->getHeight() * mTexture->getWidth() * 4, 0);
        mTexture->update(y_default_data.data(), mTexture->getWidth(), mTexture->getHeight(), mTexture->getWidth() * 4, ESurfaceChannels::RGBA);
    }


    void VideoPixelFormatRGBAP8Handler::update(Frame& frame)
    {
        // Copy data into texture
        assert(mTexture != nullptr);
        mTexture->update(frame.mFrame->data[0], mTexture->getWidth(), mTexture->getHeight(), frame.mFrame->linesize[0], ESurfaceChannels::RGBA);
    }

    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatYUV420P8Handler
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatYUV420P8Handler::VideoPixelFormatYUV420P8Handler(VideoAdvancedService& service, int pixelFormat) :
            VideoPixelFormatHandlerBase(service, pixelFormat)
    { }


    bool VideoPixelFormatYUV420P8Handler::init(utility::ErrorState& errorState)
    {
        if(!VideoPixelFormatHandlerBase::init(errorState))
            return false;

        // Initialize texture with dummy data
        if (!initTextures({2, 2}, errorState))
            return false;

        // Get sampler inputs to update from video material
        mYSampler = ensureSampler(uniform::video::sampler::YSampler, errorState);
        mUSampler = ensureSampler(uniform::video::sampler::USampler, errorState);
        mVSampler = ensureSampler(uniform::video::sampler::VSampler, errorState);

        if (!errorState.check(mYSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::YSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mUSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::USampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mVSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::VSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        mYSampler->setTexture(*mYTexture);
        mUSampler->setTexture(*mUTexture);
        mVSampler->setTexture(*mVTexture);

        return true;
    }


    bool VideoPixelFormatYUV420P8Handler::initTextures(const glm::vec2& size, utility::ErrorState& errorState)
    {
        if(mYTexture == nullptr || mYTexture->getWidth() != static_cast<uint32_t>(size.x) || mYTexture->getHeight() != static_cast<uint32_t>(size.y))
        {
            // Create texture description
            SurfaceDescriptor tex_description;
            tex_description.mWidth = static_cast<uint32_t>(size.x);
            tex_description.mHeight = static_cast<uint32_t>(size.y);
            tex_description.mColorSpace = EColorSpace::Linear;
            tex_description.mDataType = ESurfaceDataType::BYTE;
            tex_description.mChannels = ESurfaceChannels::R;

            // Create Y Texture
            mYTexture = std::make_unique<Texture2D>(mService.getCore());
            mYTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mYTexture->init(tex_description, false, 0, errorState))
                return false;

            // Update dimensions for U and V texture
            float uv_x = size.x * 0.5f;
            float uv_y = size.y * 0.5f;
            tex_description.mWidth  = static_cast<uint32_t>(uv_x);
            tex_description.mHeight = static_cast<uint32_t>(uv_y);

            // Create U
            mUTexture = std::make_unique<Texture2D>(mService.getCore());
            mUTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mUTexture->init(tex_description, false, 0, errorState))
                return false;

            // Create V Texture
            mVTexture = std::make_unique<Texture2D>(mService.getCore());
            mVTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mVTexture->init(tex_description, false, 0, errorState))
                return false;
        }

        if(mYSampler!= nullptr)
            mYSampler->setTexture(*mYTexture);

        if(mUSampler!= nullptr)
            mUSampler->setTexture(*mUTexture);

        if(mVSampler!= nullptr)
            mVSampler->setTexture(*mVTexture);

        return true;
    }


    void VideoPixelFormatYUV420P8Handler::clearTextures()
    {
        if(!mYTexture)
            return;

        auto vid_x  = static_cast<float>(mYTexture->getWidth());
        auto vid_y  = static_cast<float>(mYTexture->getHeight());
        float uv_x = vid_x * 0.5f;
        float uv_y = vid_y * 0.5f;

        // YUV420p to RGB conversion uses an 'offset' value of (-0.0625, -0.5, -0.5) in the shader.
        // This means that initializing the YUV planes to zero does not actually result in black output.
        // To fix this, we initialize the YUV planes to the negative of the offset
        std::vector<uint8_t> y_default_data(static_cast<size_t>(vid_x * vid_y), 16);

        // Initialize UV planes
        std::vector<uint8_t> uv_default_data(static_cast<size_t>(uv_x * uv_y), 127);

        mYTexture->update(y_default_data.data(), mYTexture->getWidth(), mYTexture->getHeight(), mYTexture->getWidth(), ESurfaceChannels::R);
        mUTexture->update(uv_default_data.data(), mUTexture->getWidth(), mUTexture->getHeight(), mUTexture->getWidth(), ESurfaceChannels::R);
        mVTexture->update(uv_default_data.data(), mVTexture->getWidth(), mVTexture->getHeight(), mVTexture->getWidth(), ESurfaceChannels::R);
    }


    void VideoPixelFormatYUV420P8Handler::update(Frame& frame)
    {
        // Copy data into texture
        // Copy data into texture
        assert(mYTexture != nullptr);
        mYTexture->update(frame.mFrame->data[0], mYTexture->getWidth(), mYTexture->getHeight(), frame.mFrame->linesize[0], ESurfaceChannels::R);
        mUTexture->update(frame.mFrame->data[1], mUTexture->getWidth(), mUTexture->getHeight(), frame.mFrame->linesize[1], ESurfaceChannels::R);
        mVTexture->update(frame.mFrame->data[2], mVTexture->getWidth(), mVTexture->getHeight(), frame.mFrame->linesize[2], ESurfaceChannels::R);
    }


    Material* VideoPixelFormatYUV420P8Handler::getOrCreateMaterial(utility::ErrorState& errorState)
    {
        return mService.getCore().getService<RenderService>()->getOrCreateMaterial<VideoShader>(errorState);
    }

    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatYUV444P16Handler
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatYUV444P16Handler::VideoPixelFormatYUV444P16Handler(VideoAdvancedService& service, int pixelFormat) :
            VideoPixelFormatHandlerBase(service, pixelFormat)
    { }


    bool VideoPixelFormatYUV444P16Handler::init(utility::ErrorState& errorState)
    {
        if (!VideoPixelFormatHandlerBase::init(errorState))
            return false;

        // Initialize texture with dummy data
        if (!initTextures({2, 2}, errorState))
            return false;

        // Get sampler inputs to update from video material
        mYSampler = ensureSampler(uniform::video::sampler::YSampler, errorState);
        mUSampler = ensureSampler(uniform::video::sampler::USampler, errorState);
        mVSampler = ensureSampler(uniform::video::sampler::VSampler, errorState);

        if (!errorState.check(mYSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::YSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mUSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::USampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mVSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::VSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        mYSampler->setTexture(*mYTexture);
        mUSampler->setTexture(*mUTexture);
        mVSampler->setTexture(*mVTexture);

        return true;
    }


    bool VideoPixelFormatYUV444P16Handler::initTextures(const glm::vec2& size, utility::ErrorState& errorState)
    {
        if(mYTexture == nullptr || mYTexture->getWidth() != static_cast<int>(size.x) || mYTexture->getHeight() != static_cast<int>(size.y))
        {
            // Create texture description
            SurfaceDescriptor tex_description;
            tex_description.mWidth = static_cast<int>(size.x);
            tex_description.mHeight = static_cast<int>(size.y);
            tex_description.mColorSpace = EColorSpace::Linear;
            tex_description.mDataType = ESurfaceDataType::USHORT;
            tex_description.mChannels = ESurfaceChannels::R;

            // Create Y Texture
            mYTexture = std::make_unique<Texture2D>(mService.getCore());
            mYTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mYTexture->init(tex_description, false, 0, errorState))
                return false;

            // Update dimensions for U and V texture
            float uv_x = size.x;
            float uv_y = size.y;
            tex_description.mWidth  = static_cast<uint32_t>(uv_x);
            tex_description.mHeight = static_cast<uint32_t>(uv_y);

            // Create U
            mUTexture = std::make_unique<Texture2D>(mService.getCore());
            mUTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mUTexture->init(tex_description, false, 0, errorState))
                return false;

            // Create V Texture
            mVTexture = std::make_unique<Texture2D>(mService.getCore());
            mVTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mVTexture->init(tex_description, false, 0, errorState))
                return false;
        }

        if(mYSampler!= nullptr)
            mYSampler->setTexture(*mYTexture);

        if(mUSampler!= nullptr)
            mUSampler->setTexture(*mUTexture);

        if(mVSampler!= nullptr)
            mVSampler->setTexture(*mVTexture);

        return true;
    }


    void VideoPixelFormatYUV444P16Handler::clearTextures()
    {
        if(!mYTexture)
            return;

        auto vid_x  = static_cast<float>(mYTexture->getWidth());
        auto vid_y  = static_cast<float>(mYTexture->getHeight());
        float uv_x = vid_x;
        float uv_y = vid_y;

        // YUV420p to RGB conversion uses an 'offset' value of (-0.0625, -0.5, -0.5) in the shader.
        // This means that initializing the YUV planes to zero does not actually result in black output.
        // To fix this, we initialize the YUV planes to the negative of the offset
        std::vector<uint8_t> y_default_data(static_cast<size_t>(vid_x * vid_y * 2), 0);

        // Initialize UV planes
        std::vector<uint8_t> uv_default_data(static_cast<size_t>(uv_x * uv_y * 2), 0);

        mYTexture->update(y_default_data.data(), mYTexture->getWidth(), mYTexture->getHeight(), mYTexture->getWidth() * 2, ESurfaceChannels::R);
        mUTexture->update(uv_default_data.data(), mUTexture->getWidth(), mUTexture->getHeight(), mUTexture->getWidth() * 2, ESurfaceChannels::R);
        mVTexture->update(uv_default_data.data(), mVTexture->getWidth(), mVTexture->getHeight(), mVTexture->getWidth() * 2, ESurfaceChannels::R);
    }


    void VideoPixelFormatYUV444P16Handler::update(Frame& frame)
    {
        // Copy data into texture
        // Copy data into texture
        assert(mYTexture != nullptr);
        mYTexture->update(frame.mFrame->data[0], mYTexture->getWidth(), mYTexture->getHeight(), frame.mFrame->linesize[0], ESurfaceChannels::R);
        mUTexture->update(frame.mFrame->data[1], mUTexture->getWidth(), mUTexture->getHeight(), frame.mFrame->linesize[1], ESurfaceChannels::R);
        mVTexture->update(frame.mFrame->data[2], mVTexture->getWidth(), mVTexture->getHeight(), frame.mFrame->linesize[2], ESurfaceChannels::R);
    }


    Material* VideoPixelFormatYUV444P16Handler::getOrCreateMaterial(utility::ErrorState& errorState)
    {
        return mService.getCore().getService<RenderService>()->getOrCreateMaterial<VideoShader>(errorState);
    }

    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatYUV420P16Handler
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatYUV420P16Handler::VideoPixelFormatYUV420P16Handler(VideoAdvancedService& service, int pixelFormat) :
            VideoPixelFormatHandlerBase(service, pixelFormat)
    { }


    bool VideoPixelFormatYUV420P16Handler::init(utility::ErrorState& errorState)
    {
        if(!VideoPixelFormatHandlerBase::init(errorState))
            return false;

        // Initialize texture with dummy data
        if (!initTextures({2, 2}, errorState))
            return false;

        // Get sampler inputs to update from video material
        mYSampler = ensureSampler(uniform::video::sampler::YSampler, errorState);
        mUSampler = ensureSampler(uniform::video::sampler::USampler, errorState);
        mVSampler = ensureSampler(uniform::video::sampler::VSampler, errorState);

        if (!errorState.check(mYSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::YSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mUSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::USampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mVSampler != nullptr, "Unable to find sampler: %s in material: %s",
                              uniform::video::sampler::VSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        mYSampler->setTexture(*mYTexture);
        mUSampler->setTexture(*mUTexture);
        mVSampler->setTexture(*mVTexture);

        return true;
    }


    bool VideoPixelFormatYUV420P16Handler::initTextures(const glm::vec2& size, utility::ErrorState& errorState)
    {
        if(mYTexture == nullptr || mYTexture->getWidth() != static_cast<uint32_t>(size.x) || mYTexture->getHeight() != static_cast<uint32_t>(size.y))
        {
            // Create texture description
            SurfaceDescriptor tex_description;
            tex_description.mWidth = static_cast<uint32_t>(size.x);
            tex_description.mHeight = static_cast<uint32_t>(size.y);
            tex_description.mColorSpace = EColorSpace::Linear;
            tex_description.mDataType = ESurfaceDataType::USHORT;
            tex_description.mChannels = ESurfaceChannels::R;

            // Create Y Texture
            mYTexture = std::make_unique<Texture2D>(mService.getCore());
            mYTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mYTexture->init(tex_description, false, 0, errorState))
                return false;

            // Update dimensions for U and V texture
            float uv_x = size.x * 0.5f;
            float uv_y = size.y * 0.5f;
            tex_description.mWidth  = static_cast<uint32_t>(uv_x);
            tex_description.mHeight = static_cast<uint32_t>(uv_y);

            // Create U
            mUTexture = std::make_unique<Texture2D>(mService.getCore());
            mUTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mUTexture->init(tex_description, false, 0, errorState))
                return false;

            // Create V Texture
            mVTexture = std::make_unique<Texture2D>(mService.getCore());
            mVTexture->mUsage = Texture::EUsage::DynamicWrite;
            if (!mVTexture->init(tex_description, false, 0, errorState))
                return false;
        }

        if(mYSampler!= nullptr)
            mYSampler->setTexture(*mYTexture);

        if(mUSampler!= nullptr)
            mUSampler->setTexture(*mUTexture);

        if(mVSampler!= nullptr)
            mVSampler->setTexture(*mVTexture);

        return true;
    }


    void VideoPixelFormatYUV420P16Handler::clearTextures()
    {
        if(!mYTexture)
            return;

        auto vid_x  = static_cast<float>(mYTexture->getWidth());
        auto vid_y  = static_cast<float>(mYTexture->getHeight());
        float uv_x = vid_x * 0.5f;
        float uv_y = vid_y * 0.5f;

        // YUV420p to RGB conversion uses an 'offset' value of (-0.0625, -0.5, -0.5) in the shader.
        // This means that initializing the YUV planes to zero does not actually result in black output.
        // To fix this, we initialize the YUV planes to the negative of the offset
        std::vector<uint8_t> y_default_data(static_cast<size_t>(vid_x * vid_y * 2), 0);

        // Initialize UV planes
        std::vector<uint8_t> uv_default_data(static_cast<size_t>(uv_x * uv_y * 2), 0);

        mYTexture->update(y_default_data.data(), mYTexture->getWidth(), mYTexture->getHeight(), mYTexture->getWidth() * 2, ESurfaceChannels::R);
        mUTexture->update(uv_default_data.data(), mUTexture->getWidth(), mUTexture->getHeight(), mUTexture->getWidth() * 2, ESurfaceChannels::R);
        mVTexture->update(uv_default_data.data(), mVTexture->getWidth(), mVTexture->getHeight(), mVTexture->getWidth() * 2, ESurfaceChannels::R);
    }


    void VideoPixelFormatYUV420P16Handler::update(Frame& frame)
    {
        // Copy data into texture
        assert(mYTexture != nullptr);
        mYTexture->update(frame.mFrame->data[0], mYTexture->getWidth(), mYTexture->getHeight(), frame.mFrame->linesize[0], ESurfaceChannels::R);
        mUTexture->update(frame.mFrame->data[1], mUTexture->getWidth(), mUTexture->getHeight(), frame.mFrame->linesize[1], ESurfaceChannels::R);
        mVTexture->update(frame.mFrame->data[2], mVTexture->getWidth(), mVTexture->getHeight(), frame.mFrame->linesize[2], ESurfaceChannels::R);
    }


    Material* VideoPixelFormatYUV420P16Handler::getOrCreateMaterial(utility::ErrorState& errorState)
    {
        return mService.getCore().getService<RenderService>()->getOrCreateMaterial<VideoShader>(errorState);
    }

    //////////////////////////////////////////////////////////////////////////
    //// Utility
    //////////////////////////////////////////////////////////////////////////

    namespace utility
    {
        std::unique_ptr<VideoPixelFormatHandlerBase> createVideoPixelFormatHandler(int pixelFormat, VideoAdvancedService& service, utility::ErrorState& error)
        {
            switch (pixelFormat)
            {
                case AV_PIX_FMT_YUV420P:
                case AV_PIX_FMT_YUVJ420P:
                    return std::make_unique<VideoPixelFormatYUV420P8Handler>(service, pixelFormat);
                case AV_PIX_FMT_YUV444P16BE:
                case AV_PIX_FMT_YUV444P16LE:
                    return std::make_unique<VideoPixelFormatYUV444P16Handler>(service, pixelFormat);
                case AV_PIX_FMT_YUV420P16LE:
                case AV_PIX_FMT_YUV420P16BE:
                    return std::make_unique<VideoPixelFormatYUV420P16Handler>(service, pixelFormat);
                case AV_PIX_FMT_RGBA:
                case AV_PIX_FMT_RGB0:
                    return std::make_unique<VideoPixelFormatRGBAP8Handler>(service, pixelFormat);
                default:
                    error.fail("Unsupported pixel format: %d", pixelFormat);
                    return nullptr;
            }
        }


        bool getVideoPixelFormatHandlerType(int pixelFormat, rtti::TypeInfo& typeInfo, utility::ErrorState& errorState)
        {
            switch (pixelFormat)
            {
                case AV_PIX_FMT_YUV420P:
                case AV_PIX_FMT_YUVJ420P:
                    typeInfo = rtti::TypeInfo::get<VideoPixelFormatYUV420P8Handler>();
                    return true;
                case AV_PIX_FMT_YUV444P16BE:
                case AV_PIX_FMT_YUV444P16LE:
                    typeInfo = rtti::TypeInfo::get<VideoPixelFormatYUV444P16Handler>();
                    return true;
                case AV_PIX_FMT_RGBA:
                case AV_PIX_FMT_RGB0:
                    typeInfo = rtti::TypeInfo::get<VideoPixelFormatRGBAP8Handler>();
                    return true;
                case AV_PIX_FMT_YUV420P16LE:
                case AV_PIX_FMT_YUV420P16BE:
                    typeInfo = rtti::TypeInfo::get<VideoPixelFormatYUV420P16Handler>();
                    return true;
                default:
                    errorState.fail("Unsupported pixel format: %d", pixelFormat);
                    return false;
            }
        }

    }
}