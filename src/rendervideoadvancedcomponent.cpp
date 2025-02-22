/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Local Includes
#include "rendervideoadvancedcomponent.h"
#include "videorgbashader.h"

// External Includes
#include <entity.h>
#include <orthocameracomponent.h>
#include <nap/core.h>
#include <renderservice.h>
#include <renderglobals.h>
#include <glm/gtc/matrix_transform.hpp>

RTTI_BEGIN_CLASS(nap::RenderVideoAdvancedComponent)
        RTTI_PROPERTY("OutputTexture",	&nap::RenderVideoAdvancedComponent::mOutputTexture,			nap::rtti::EPropertyMetaData::Required,	"The texture to render output to")
        RTTI_PROPERTY("VideoPlayer",	&nap::RenderVideoAdvancedComponent::mVideoPlayer,			nap::rtti::EPropertyMetaData::Required, "The video player to render to texture")
        RTTI_PROPERTY("Samples",		&nap::RenderVideoAdvancedComponent::mRequestedSamples,		nap::rtti::EPropertyMetaData::Default,	"The number of rasterization samples")
        RTTI_PROPERTY("ClearColor",		&nap::RenderVideoAdvancedComponent::mClearColor,			nap::rtti::EPropertyMetaData::Default,	"Initial target clear color")
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::RenderVideoAdvancedComponentInstance)
        RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

//////////////////////////////////////////////////////////////////////////


namespace nap
{
    /**
     * Creates a model matrix based on the dimensions of the given target.
     */
    static void computeModelMatrix(const nap::IRenderTarget& target, glm::mat4& outMatrix)
    {
        // Transform to middle of target
        glm::ivec2 tex_size = target.getBufferSize();
        outMatrix = glm::translate(glm::mat4(), glm::vec3(
                tex_size.x / 2.0f,
                tex_size.y / 2.0f,
                0.0f));

        // Scale to fit target
        outMatrix = glm::scale(outMatrix, glm::vec3(tex_size.x, tex_size.y, 1.0f));
    }


    RenderVideoAdvancedComponentInstance::RenderVideoAdvancedComponentInstance(EntityInstance& entity, Component& resource) :
            RenderableComponentInstance(entity, resource),
            mTarget(*entity.getCore()),
            mPlane(*entity.getCore()),
            mRenderService(entity.getCore()->getService<RenderService>()){ }


    bool RenderVideoAdvancedComponentInstance::init(utility::ErrorState& errorState)
    {
        if (!RenderableComponentInstance::init(errorState))
            return false;

        // Get resource
        auto* resource = getComponent<RenderVideoAdvancedComponent>();

        // Extract player
        mPlayer = resource->mVideoPlayer.get();
        if (!errorState.check(mPlayer != nullptr, "%s: no video player", resource->mID.c_str()))
            return false;

        // Extract output texture to render to and make sure format is correct
        mOutputTexture = resource->mOutputTexture.get();
        if (!errorState.check(mOutputTexture != nullptr, "%s: no output texture", resource->mID.c_str()))
            return false;

        if (!errorState.check(mOutputTexture->mColorFormat == RenderTexture2D::EFormat::RGBA8, "%s: output texture color format is not RGBA8", resource->mID.c_str()))
            return false;

        // Setup render target and initialize
        mTarget.mClearColor = resource->mClearColor.convert<RGBAColorFloat>();
        mTarget.mColorTexture  = resource->mOutputTexture;
        mTarget.mSampleShading = true;
        mTarget.mRequestedSamples = resource->mRequestedSamples;
        if (!mTarget.init(errorState))
            return false;

        // Now create a plane and initialize it
        // The plane is positioned on update based on current texture output size
        mPlane.mSize = glm::vec2(1.0f, 1.0f);
        mPlane.mPosition = glm::vec3(0.0f, 0.0f, 0.0f);
        mPlane.mCullMode = ECullMode::Back;
        mPlane.mUsage = EMemoryUsage::Static;
        mPlane.mColumns = 1;
        mPlane.mRows = 1;

        if (!mPlane.init(errorState))
            return false;

        // Get pixel format handler
        auto& pixel_format_handler = mPlayer->getPixelFormatHandler();

        // Create the renderable mesh, which represents a valid mesh / material combination
        mRenderableMesh = mRenderService->createRenderableMesh(mPlane, pixel_format_handler.mMaterialInstance, errorState);
        if (!mRenderableMesh.isValid())
            return false;

        return true;
    }


    bool RenderVideoAdvancedComponentInstance::isSupported(nap::CameraComponentInstance& camera) const
    {
        return camera.get_type().is_derived_from(RTTI_OF(OrthoCameraComponentInstance));
    }


    nap::Texture2D& RenderVideoAdvancedComponentInstance::getOutputTexture()
    {
        return mTarget.getColorTexture();
    }


    void RenderVideoAdvancedComponentInstance::draw()
    {
        // Get current command buffer, should be headless.
        VkCommandBuffer command_buffer = mRenderService->getCurrentCommandBuffer();

        // Create orthographic projection matrix
        glm::ivec2 size = mTarget.getBufferSize();

        // Create projection matrix
        glm::mat4 proj_matrix = OrthoCameraComponentInstance::createRenderProjectionMatrix(0.0f, (float)size.x, 0.0f, (float)size.y);

        // Call on draw
        mTarget.beginRendering();
        onDraw(mTarget, command_buffer, glm::mat4(), proj_matrix);
        mTarget.endRendering();
    }


    void RenderVideoAdvancedComponentInstance::onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
    {
        //
        auto& pixel_format_handler = mPlayer->getPixelFormatHandler();

        // Update the model matrix so that the plane mesh is of the same size as the render target
        computeModelMatrix(renderTarget, pixel_format_handler.mModelMatrix);
        pixel_format_handler.mModelMatrixUniform->setValue(pixel_format_handler.mModelMatrix);

        // Update matrices, projection and model are required
        pixel_format_handler.mProjectMatrixUniform->setValue(projectionMatrix);
        pixel_format_handler.mViewMatrixUniform->setValue(viewMatrix);

        // Get valid descriptor set
        const DescriptorSet& descriptor_set = pixel_format_handler.mMaterialInstance.update();

        // Gather draw info
        MeshInstance& mesh_instance = mRenderableMesh.getMesh().getMeshInstance();
        GPUMesh& mesh = mesh_instance.getGPUMesh();

        // Get pipeline to to render with
        utility::ErrorState error_state;
        RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(renderTarget, mRenderableMesh.getMesh(), pixel_format_handler.mMaterialInstance, error_state);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mLayout, 0, 1, &descriptor_set.mSet, 0, nullptr);

        // Bind buffers and draw
        const std::vector<VkBuffer>& vertexBuffers = mRenderableMesh.getVertexBuffers();
        const std::vector<VkDeviceSize>& vertexBufferOffsets = mRenderableMesh.getVertexBufferOffsets();

        vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexBufferOffsets.data());
        for (int index = 0; index < mesh_instance.getNumShapes(); ++index)
        {
            const IndexBuffer& index_buffer = mesh.getIndexBuffer(index);
            vkCmdBindIndexBuffer(commandBuffer, index_buffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, index_buffer.getCount(), 1, 0, 0, 0);
        }
    }
}
