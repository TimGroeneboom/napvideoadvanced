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

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPixelFormatRGBAHandler)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService&)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPixelFormatYUV8Handler)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService&)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPixelFormatYUV16Handler)
        RTTI_CONSTRUCTOR(nap::VideoAdvancedService&)
RTTI_END_CLASS

namespace nap
{
    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatHandlerBase
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatHandlerBase::VideoPixelFormatHandlerBase(VideoAdvancedService& service) :
            mService(service)
    { }


    nap::UniformMat4Instance* VideoPixelFormatHandlerBase::ensureUniform(const std::string& uniformName, utility::ErrorState& error)
    {
        assert(mMVPStruct != nullptr);
        UniformMat4Instance* found_uniform = mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniformName);
        if (!error.check(found_uniform != nullptr,
                         "%s: unable to find uniform: %s in material: %s", this->mID.c_str(), uniformName.c_str(),
                         mMaterialInstance.getMaterial().mID.c_str()))
            return nullptr;
        return found_uniform;
    }


    nap::Sampler2DInstance* VideoPixelFormatHandlerBase::ensureSampler(const std::string& samplerName, utility::ErrorState& error)
    {
        Sampler2DInstance* found_sampler = mMaterialInstance.getOrCreateSampler<Sampler2DInstance>(samplerName);
        if (!error.check(found_sampler != nullptr,
                         "%s: unable to find sampler: %s in material: %s", this->mID.c_str(), samplerName.c_str(),
                         mMaterialInstance.getMaterial().mID.c_str()))
            return nullptr;
        return found_sampler;
    }

    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatRGBAHandler
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatRGBAHandler::VideoPixelFormatRGBAHandler(VideoAdvancedService& service) :
            VideoPixelFormatHandlerBase(service)
    { }


    bool VideoPixelFormatRGBAHandler::init(utility::ErrorState& errorState)
    {
        // Initialize texture with dummy data
        if (!initTextures({1, 1}, errorState))
            return false;

        // Extract render service
        auto* render_service = mService.getCore().getService<RenderService>();
        assert(render_service != nullptr);

        // Get video material
        Material* video_material = render_service->getOrCreateMaterial<VideoRGBAShader>(errorState);
        if (!errorState.check(video_material != nullptr, "%s: unable to get or create video material", mID.c_str()))
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
        if (!errorState.check(mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
                              this->mID.c_str(), uniform::mvpStruct, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        // Get all matrices
        mModelMatrixUniform = ensureUniform(uniform::modelMatrix, errorState);
        mProjectMatrixUniform = ensureUniform(uniform::projectionMatrix, errorState);
        mViewMatrixUniform = ensureUniform(uniform::viewMatrix, errorState);

        if (mModelMatrixUniform == nullptr || mProjectMatrixUniform == nullptr || mViewMatrixUniform == nullptr)
            return false;

        // Get sampler inputs to update from video material
        mSampler = ensureSampler(uniform::videorgba::sampler::RGBASampler, errorState);
        mSampler->setTexture(*mTexture);

        if (!errorState.check(mSampler != nullptr, "%s: Unable to find sampler: %s in material: %s",
                              this->mID.c_str(), uniform::videorgba::sampler::RGBASampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        return true;
    }


    bool VideoPixelFormatRGBAHandler::initTextures(const glm::vec2& size, utility::ErrorState& errorState)
    {
        if(mTexture == nullptr || mTexture->getWidth() != size.x || mTexture->getHeight() != size.y)
        {
            // Create texture description
            SurfaceDescriptor tex_description;
            tex_description.mWidth = size.x;
            tex_description.mHeight = size.y;
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


    void VideoPixelFormatRGBAHandler::clearTextures()
    {
        if(!mTexture)
            return;

        std::vector<uint8_t> y_default_data(mTexture->getHeight() * mTexture->getWidth() * 4, 0);
        mTexture->update(y_default_data.data(), mTexture->getWidth(), mTexture->getHeight(), mTexture->getWidth() * 4, ESurfaceChannels::RGBA);
    }


    void VideoPixelFormatRGBAHandler::update(Frame& frame)
    {
        // Copy data into texture
        assert(mTexture != nullptr);
        mTexture->update(frame.mFrame->data[0], mTexture->getWidth(), mTexture->getHeight(), frame.mFrame->linesize[0], ESurfaceChannels::RGBA);
    }

    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatYUV8Handler
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatYUV8Handler::VideoPixelFormatYUV8Handler(VideoAdvancedService& service) :
            VideoPixelFormatHandlerBase(service)
    { }


    bool VideoPixelFormatYUV8Handler::init(utility::ErrorState& errorState)
    {
        // Initialize texture with dummy data
        if (!initTextures({2, 2}, errorState))
            return false;

        // Extract render service
        auto* render_service = mService.getCore().getService<RenderService>();
        assert(render_service != nullptr);

        // Get video material
        Material* video_material = render_service->getOrCreateMaterial<VideoShader>(errorState);
        if (!errorState.check(video_material != nullptr, "%s: unable to get or create video material", mID.c_str()))
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
        if (!errorState.check(mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
                              this->mID.c_str(), uniform::mvpStruct, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        // Get all matrices
        mModelMatrixUniform = ensureUniform(uniform::modelMatrix, errorState);
        mProjectMatrixUniform = ensureUniform(uniform::projectionMatrix, errorState);
        mViewMatrixUniform = ensureUniform(uniform::viewMatrix, errorState);

        if (mModelMatrixUniform == nullptr || mProjectMatrixUniform == nullptr || mViewMatrixUniform == nullptr)
            return false;

        // Get sampler inputs to update from video material
        mYSampler = ensureSampler(uniform::video::sampler::YSampler, errorState);
        mUSampler = ensureSampler(uniform::video::sampler::USampler, errorState);
        mVSampler = ensureSampler(uniform::video::sampler::VSampler, errorState);

        if (!errorState.check(mYSampler != nullptr, "%s: Unable to find sampler: %s in material: %s",
                              this->mID.c_str(), uniform::video::sampler::YSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mUSampler != nullptr, "%s: Unable to find sampler: %s in material: %s",
                              this->mID.c_str(), uniform::video::sampler::USampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mVSampler != nullptr, "%s: Unable to find sampler: %s in material: %s",
                              this->mID.c_str(), uniform::video::sampler::VSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        mYSampler->setTexture(*mYTexture);
        mUSampler->setTexture(*mUTexture);
        mVSampler->setTexture(*mVTexture);

        return true;
    }


    bool VideoPixelFormatYUV8Handler::initTextures(const glm::vec2& size, utility::ErrorState& errorState)
    {
        if(mYTexture == nullptr || mYTexture->getWidth() != size.x || mYTexture->getHeight() != size.y)
        {
            // Create texture description
            SurfaceDescriptor tex_description;
            tex_description.mWidth = size.x;
            tex_description.mHeight = size.y;
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
            tex_description.mWidth  = uv_x;
            tex_description.mHeight = uv_y;

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


    void VideoPixelFormatYUV8Handler::clearTextures()
    {
        if(!mYTexture)
            return;

        float vid_x  = mYTexture->getWidth();
        float vid_y  = mYTexture->getHeight();
        float uv_x = vid_x * 0.5f;
        float uv_y = vid_y * 0.5f;

        // YUV420p to RGB conversion uses an 'offset' value of (-0.0625, -0.5, -0.5) in the shader.
        // This means that initializing the YUV planes to zero does not actually result in black output.
        // To fix this, we initialize the YUV planes to the negative of the offset
        std::vector<uint8_t> y_default_data(vid_x * vid_y, 16);

        // Initialize UV planes
        std::vector<uint8_t> uv_default_data(uv_x * uv_y, 127);

        mYTexture->update(y_default_data.data(), mYTexture->getWidth(), mYTexture->getHeight(), mYTexture->getWidth(), ESurfaceChannels::R);
        mUTexture->update(uv_default_data.data(), mUTexture->getWidth(), mUTexture->getHeight(), mUTexture->getWidth(), ESurfaceChannels::R);
        mVTexture->update(uv_default_data.data(), mVTexture->getWidth(), mVTexture->getHeight(), mVTexture->getWidth(), ESurfaceChannels::R);
    }


    void VideoPixelFormatYUV8Handler::update(Frame& frame)
    {
        // Copy data into texture
        // Copy data into texture
        assert(mYTexture != nullptr);
        mYTexture->update(frame.mFrame->data[0], mYTexture->getWidth(), mYTexture->getHeight(), frame.mFrame->linesize[0], ESurfaceChannels::R);
        mUTexture->update(frame.mFrame->data[1], mUTexture->getWidth(), mUTexture->getHeight(), frame.mFrame->linesize[1], ESurfaceChannels::R);
        mVTexture->update(frame.mFrame->data[2], mVTexture->getWidth(), mVTexture->getHeight(), frame.mFrame->linesize[2], ESurfaceChannels::R);
    }

    //////////////////////////////////////////////////////////////////////////
    //// VideoPixelFormatYUV16Handler
    //////////////////////////////////////////////////////////////////////////

    VideoPixelFormatYUV16Handler::VideoPixelFormatYUV16Handler(VideoAdvancedService& service) :
            VideoPixelFormatHandlerBase(service)
    { }


    bool VideoPixelFormatYUV16Handler::init(utility::ErrorState& errorState)
    {
        // Initialize texture with dummy data
        if (!initTextures({2, 2}, errorState))
            return false;

        // Extract render service
        auto* render_service = mService.getCore().getService<RenderService>();
        assert(render_service != nullptr);

        // Get video material
        Material* video_material = render_service->getOrCreateMaterial<VideoShader>(errorState);
        if (!errorState.check(video_material != nullptr, "%s: unable to get or create video material", mID.c_str()))
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
        if (!errorState.check(mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
                              this->mID.c_str(), uniform::mvpStruct, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        // Get all matrices
        mModelMatrixUniform = ensureUniform(uniform::modelMatrix, errorState);
        mProjectMatrixUniform = ensureUniform(uniform::projectionMatrix, errorState);
        mViewMatrixUniform = ensureUniform(uniform::viewMatrix, errorState);

        if (mModelMatrixUniform == nullptr || mProjectMatrixUniform == nullptr || mViewMatrixUniform == nullptr)
            return false;

        // Get sampler inputs to update from video material
        mYSampler = ensureSampler(uniform::video::sampler::YSampler, errorState);
        mUSampler = ensureSampler(uniform::video::sampler::USampler, errorState);
        mVSampler = ensureSampler(uniform::video::sampler::VSampler, errorState);

        if (!errorState.check(mYSampler != nullptr, "%s: Unable to find sampler: %s in material: %s",
                              this->mID.c_str(), uniform::video::sampler::YSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mUSampler != nullptr, "%s: Unable to find sampler: %s in material: %s",
                              this->mID.c_str(), uniform::video::sampler::USampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        if (!errorState.check(mVSampler != nullptr, "%s: Unable to find sampler: %s in material: %s",
                              this->mID.c_str(), uniform::video::sampler::VSampler, mMaterialInstance.getMaterial().mID.c_str()))
            return false;

        mYSampler->setTexture(*mYTexture);
        mUSampler->setTexture(*mUTexture);
        mVSampler->setTexture(*mVTexture);

        return true;
    }


    bool VideoPixelFormatYUV16Handler::initTextures(const glm::vec2& size, utility::ErrorState& errorState)
    {
        if(mYTexture == nullptr || mYTexture->getWidth() != size.x || mYTexture->getHeight() != size.y)
        {
            // Create texture description
            SurfaceDescriptor tex_description;
            tex_description.mWidth = size.x;
            tex_description.mHeight = size.y;
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
            tex_description.mWidth  = uv_x;
            tex_description.mHeight = uv_y;

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


    void VideoPixelFormatYUV16Handler::clearTextures()
    {
        if(!mYTexture)
            return;

        float vid_x  = mYTexture->getWidth();
        float vid_y  = mYTexture->getHeight();
        float uv_x = vid_x;
        float uv_y = vid_y;

        // YUV420p to RGB conversion uses an 'offset' value of (-0.0625, -0.5, -0.5) in the shader.
        // This means that initializing the YUV planes to zero does not actually result in black output.
        // To fix this, we initialize the YUV planes to the negative of the offset
        std::vector<uint8_t> y_default_data(vid_x * vid_y * 2, 4095);

        // Initialize UV planes
        std::vector<uint8_t> uv_default_data(uv_x * uv_y * 2, 32768);

        mYTexture->update(y_default_data.data(), mYTexture->getWidth(), mYTexture->getHeight(), mYTexture->getWidth() * 2, ESurfaceChannels::R);
        mUTexture->update(uv_default_data.data(), mUTexture->getWidth(), mUTexture->getHeight(), mUTexture->getWidth() * 2, ESurfaceChannels::R);
        mVTexture->update(uv_default_data.data(), mVTexture->getWidth(), mVTexture->getHeight(), mVTexture->getWidth() * 2, ESurfaceChannels::R);
    }


    void VideoPixelFormatYUV16Handler::update(Frame& frame)
    {
        // Copy data into texture
        // Copy data into texture
        assert(mYTexture != nullptr);
        mYTexture->update(frame.mFrame->data[0], mYTexture->getWidth(), mYTexture->getHeight(), frame.mFrame->linesize[0], ESurfaceChannels::R);
        mUTexture->update(frame.mFrame->data[1], mUTexture->getWidth(), mUTexture->getHeight(), frame.mFrame->linesize[1], ESurfaceChannels::R);
        mVTexture->update(frame.mFrame->data[2], mVTexture->getWidth(), mVTexture->getHeight(), frame.mFrame->linesize[2], ESurfaceChannels::R);
    }
}