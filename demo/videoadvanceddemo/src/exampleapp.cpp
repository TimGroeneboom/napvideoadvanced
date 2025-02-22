// Local Includes
#include "exampleapp.h"
#include "rendervideoadvancedcomponent.h"

// External Includes
#include <utility/fileutils.h>
#include <nap/logger.h>
#include <inputrouter.h>
#include <rendergnomoncomponent.h>
#include <perspcameracomponent.h>
#include <videoplayer.h>
#include <videoservice.h>
#include <videofile.h>
#include <imguiutils.h>

namespace nap 
{    
    bool CoreApp::init(utility::ErrorState& error)
    {
		// Retrieve services
		mRenderService	= getCore().getService<nap::RenderService>();
		mSceneService	= getCore().getService<nap::SceneService>();
		mInputService	= getCore().getService<nap::InputService>();
		mGuiService		= getCore().getService<nap::IMGuiService>();

		// Fetch the resource manager
        mResourceManager = getCore().getResourceManager();

		// Get the render window
		mRenderWindow = mResourceManager->findObject<nap::RenderWindow>("Window");
		if (!error.check(mRenderWindow != nullptr, "unable to find render window with name: %s", "Window"))
			return false;

		// Get the scene that contains our entities and components
		mScene = mResourceManager->findObject<Scene>("Scene");
		if (!error.check(mScene != nullptr, "unable to find scene with name: %s", "Scene"))
			return false;

		// Get the camera and origin Gnomon entity
		mCameraEntity = mScene->findEntity("CameraEntity");
		mGnomonEntity = mScene->findEntity("GnomonEntity");
        mRenderVideoEntity = mScene->findEntity("RenderVideoEntity");

        mHapTexture1 = mResourceManager->findObject<RenderTexture2D>("HapTexture1");
        if (!error.check(mHapTexture1 != nullptr, "unable to find render texture with name: %s", "HapTexture1"))
            return false;

        mHapTexture2 = mResourceManager->findObject<RenderTexture2D>("HapTexture2");
        if (!error.check(mHapTexture2 != nullptr, "unable to find render texture with name: %s", "HapTexture2"))
            return false;

        mHapTexture3 = mResourceManager->findObject<RenderTexture2D>("HapTexture3");
        if (!error.check(mHapTexture3 != nullptr, "unable to find render texture with name: %s", "HapTexture3"))
            return false;

        mHapTexture4 = mResourceManager->findObject<RenderTexture2D>("HapTexture4");
        if (!error.check(mHapTexture4 != nullptr, "unable to find render texture with name: %s", "HapTexture4"))
            return false;

        mHapTexture5 = mResourceManager->findObject<RenderTexture2D>("HapTexture5");
        if (!error.check(mHapTexture5 != nullptr, "unable to find render texture with name: %s", "HapTexture5"))
            return false;

        mVideoPlayer1 = mResourceManager->findObject<ThreadedVideoPlayer>("HapPlayer1");
        if (!error.check(mVideoPlayer1 != nullptr, "unable to find video player with name: %s", "HapPlayer1"))
            return false;
        mVideoPlayer1->play();

        mVideoPlayer2 = mResourceManager->findObject<ThreadedVideoPlayer>("HapPlayer2");
        if (!error.check(mVideoPlayer2 != nullptr, "unable to find video player with name: %s", "HapPlayer2"))
            return false;
        mVideoPlayer2->play();

        mVideoPlayer3 = mResourceManager->findObject<ThreadedVideoPlayer>("HapPlayer3");
        if (!error.check(mVideoPlayer3 != nullptr, "unable to find video player with name: %s", "HapPlayer3"))
            return false;
        mVideoPlayer3->play();

        mVideoPlayer4 = mResourceManager->findObject<ThreadedVideoPlayer>("HapPlayer4");
        if (!error.check(mVideoPlayer4 != nullptr, "unable to find video player with name: %s", "HapPlayer4"))
            return false;
        mVideoPlayer4->play();

        mVideoPlayer5 = mResourceManager->findObject<ThreadedVideoPlayer>("HapPlayer5");
        if (!error.check(mVideoPlayer5 != nullptr, "unable to find video player with name: %s", "HapPlayer5"))
            return false;
        mVideoPlayer5->play();

		// All done!
        return true;
    }


    // Render app
    void CoreApp::render()
    {
		// Signal the beginning of a new frame, allowing it to be recorded.
		// The system might wait until all commands that were previously associated with the new frame have been processed on the GPU.
		// Multiple frames are in flight at the same time, but if the graphics load is heavy the system might wait here to ensure resources are available.
		mRenderService->beginFrame();

        if(mRenderService->beginHeadlessRecording())
        {
            std::vector<RenderVideoAdvancedComponentInstance*> components_to_render;
            mRenderVideoEntity->getComponentsOfType(components_to_render);
            for(auto& component : components_to_render)
            {
                component->draw();
            }
            mRenderService->endHeadlessRecording();
        }

		// Begin recording the render commands for the main render window
		if (mRenderService->beginRecording(*mRenderWindow))
		{
			// Begin render pass
			mRenderWindow->beginRendering();

			// Get Perspective camera to render with
			auto& perp_cam = mCameraEntity->getComponent<PerspCameraComponentInstance>();

			// Add Gnomon
			std::vector<nap::RenderableComponentInstance*> components_to_render
			{
				&mGnomonEntity->getComponent<RenderGnomonComponentInstance>()
			};

			// Render Gnomon
			mRenderService->renderObjects(*mRenderWindow, perp_cam, components_to_render);

			// Draw GUI elements
			mGuiService->draw();

			// Stop render pass
			mRenderWindow->endRendering();

			// End recording
			mRenderService->endRecording();
		}

		// Proceed to next frame
		mRenderService->endFrame();
    }


    void CoreApp::windowMessageReceived(WindowEventPtr windowEvent)
    {
		mRenderService->addEvent(std::move(windowEvent));
    }


    void CoreApp::inputMessageReceived(InputEventPtr inputEvent)
    {
		// If we pressed escape, quit the loop
		if (inputEvent->get_type().is_derived_from(RTTI_OF(nap::KeyPressEvent)))
		{
			nap::KeyPressEvent* press_event = static_cast<nap::KeyPressEvent*>(inputEvent.get());
			if (press_event->mKey == nap::EKeyCode::KEY_ESCAPE)
				quit();

			if (press_event->mKey == nap::EKeyCode::KEY_f)
				mRenderWindow->toggleFullscreen();
		}
		mInputService->addEvent(std::move(inputEvent));
    }


    int CoreApp::shutdown()
    {
		return 0;
    }


	// Update app
    void CoreApp::update(double deltaTime)
    {
		// Use a default input router to forward input events (recursively) to all input components in the scene
		// This is explicit because we don't know what entity should handle the events from a specific window.
		nap::DefaultInputRouter input_router(true);
		mInputService->processWindowEvents(*mRenderWindow, input_router, { &mScene->getRootEntity() });

        if(ImGui::Begin("Video Players"))
        {
            ImGui::PushID(1);
            float ratio = mVideoPlayer1->getWidth() / (float)mVideoPlayer1->getHeight();
            float width = ImGui::GetContentRegionAvailWidth();
            ImGui::Image(*mHapTexture1, ImVec2(width, width / ratio));

            if(ImGui::SliderFloat("Seek", &mSeek1, 0.0f, 1.0f))
            {
                mVideoPlayer1->seek(mSeek1 * mVideoPlayer1->getDuration());
            }

            if(ImGui::Button("Load Again"))
            {
                mVideoPlayer1->loadVideo(mVideoPlayer1->mFilePath);
            }
            ImGui::PopID();

            ImGui::PushID(2);
            ratio = mVideoPlayer2->getWidth() / (float)mVideoPlayer2->getHeight();
            width = ImGui::GetContentRegionAvailWidth();
            ImGui::Image(*mHapTexture2, ImVec2(width, width / ratio));

            if(ImGui::SliderFloat("Seek", &mSeek2, 0.0f, 1.0f))
            {
                mVideoPlayer2->seek(mSeek2 * mVideoPlayer2->getDuration());
            }

            if(ImGui::Button("Load Again"))
            {
                mVideoPlayer2->loadVideo(mVideoPlayer2->mFilePath);
            }

            ImGui::PopID();

            ImGui::PushID(3);
            ratio = mVideoPlayer3->getWidth() / (float)mVideoPlayer3->getHeight();
            width = ImGui::GetContentRegionAvailWidth();
            ImGui::Image(*mHapTexture3, ImVec2(width, width / ratio));

            if(ImGui::SliderFloat("Seek", &mSeek3, 0.0f, 1.0f))
            {
                mVideoPlayer3->seek(mSeek3 * mVideoPlayer3->getDuration());
            }

            if(ImGui::Button("Load Again"))
            {
                mVideoPlayer3->loadVideo(mVideoPlayer3->mFilePath);
            }

            ImGui::PopID();

            ImGui::PushID(4);
            ratio = mVideoPlayer4->getWidth() / (float)mVideoPlayer4->getHeight();
            width = ImGui::GetContentRegionAvailWidth();

            ImGui::Image(*mHapTexture4, ImVec2(width, width / ratio));

            if(ImGui::SliderFloat("Seek", &mSeek4, 0.0f, 1.0f))
            {
                mVideoPlayer4->seek(mSeek4 * mVideoPlayer4->getDuration());
            }

            if(ImGui::Button("Load Again"))
            {
                mVideoPlayer4->loadVideo(mVideoPlayer4->mFilePath);
            }

            ImGui::PopID();

            ImGui::PushID(5);
            ratio = mVideoPlayer5->getWidth() / (float)mVideoPlayer5->getHeight();
            width = ImGui::GetContentRegionAvailWidth();
            ImGui::Image(*mHapTexture5, ImVec2(width, width / ratio));

            if(ImGui::SliderFloat("Seek", &mSeek5, 0.0f, 1.0f))
            {
                mVideoPlayer5->seek(mSeek5 * mVideoPlayer5->getDuration());
            }

            if(ImGui::Button("Load Again"))
            {
                mVideoPlayer5->loadVideo(mVideoPlayer5->mFilePath);
            }

            ImGui::PopID();
        }
        ImGui::End();
    }
}
