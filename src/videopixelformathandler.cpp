#include "videopixelformathandler.h"
#include "videoadvancedservice.h"
#include "videorgbashader.h"
#include "renderglobals.h"

#include <video.h>
#include <nap/core.h>
#include <renderservice.h>

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

namespace nap
{
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


    VideoPixelFormatRGBAHandler::VideoPixelFormatRGBAHandler(VideoAdvancedService& service) :
            VideoPixelFormatHandlerBase(service)
    { }


    bool VideoPixelFormatRGBAHandler::init(utility::ErrorState& errorState)
    {
        SurfaceDescriptor tex_description;
        tex_description.mWidth = 1;
        tex_description.mHeight = 1;
        tex_description.mColorSpace = EColorSpace::Linear;
        tex_description.mDataType = ESurfaceDataType::BYTE;
        tex_description.mChannels = ESurfaceChannels::RGBA;

        // Create Y Texture
        mTexture = std::make_unique<Texture2D>(mService.getCore());
        mTexture->mUsage = Texture::EUsage::DynamicWrite;

        if (!mTexture->init(tex_description, false, 0, errorState))
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


    bool VideoPixelFormatRGBAHandler::initMaterial(utility::ErrorState &errorState)
    {
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
}