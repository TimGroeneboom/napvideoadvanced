#pragma once

#include <nap/resource.h>
#include <video.h>
#include <texture.h>
#include <materialinstance.h>

namespace nap
{
    class VideoAdvancedService;

    class NAPAPI VideoPixelFormatHandlerBase : public Resource
    {
        friend class RenderVideoAdvancedComponentInstance;

        RTTI_ENABLE(Resource)
    public:
        VideoPixelFormatHandlerBase(VideoAdvancedService& service);

        virtual bool initMaterial(utility::ErrorState& errorState) = 0;

        virtual bool initTextures(const glm::vec2& size, utility::ErrorState& errorState) = 0;

        virtual void clearTextures() = 0;

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


    class NAPAPI VideoPixelFormatRGBAHandler : public VideoPixelFormatHandlerBase
    {
        RTTI_ENABLE(VideoPixelFormatHandlerBase)
    public:
        VideoPixelFormatRGBAHandler(VideoAdvancedService& service);

        bool init(utility::ErrorState& errorState) override;

        virtual bool initTextures(const glm::vec2& size, utility::ErrorState& errorState) override;

        virtual void clearTextures() override;

        virtual void update(Frame& frame) override;

        virtual bool initMaterial(utility::ErrorState& errorState) override;
    private:
        std::unique_ptr<Texture2D> mTexture;
        Sampler2DInstance* mSampler = nullptr;
    };

    using VideoPixelFormatRGBAHandlerObjectCreator = rtti::ObjectCreator<VideoPixelFormatRGBAHandler, VideoAdvancedService>;
}