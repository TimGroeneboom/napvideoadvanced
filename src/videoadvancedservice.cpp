// Local Includes
#include "videoadvancedservice.h"
#include "videoservice.h"
#include "videoplayeradvanced.h"
#include "videopixelformathandler.h"
#include "threadedvideoplayer.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <iostream>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoAdvancedService)
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	bool VideoAdvancedService::init(nap::utility::ErrorState& errorState)
	{
		//Logger::info("Initializing happlayerService");
		return true;
	}


	void VideoAdvancedService::update(double deltaTime)
	{
        for(auto player : mPlayers)
        {
            player->update(deltaTime);
        }
	}
	

	void VideoAdvancedService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
	}
	

	void VideoAdvancedService::shutdown()
	{
	}


    void VideoAdvancedService::registerPlayer(nap::VideoPlayerAdvancedBase &player)
    {
        mPlayers.emplace_back(&player);
    }


    void VideoAdvancedService::removePlayer(nap::VideoPlayerAdvancedBase &player)
    {
        auto found_it = std::find_if(mPlayers.begin(), mPlayers.end(), [&](const auto& it)
        {
            return it == &player;
        });
        assert(found_it != mPlayers.end());
        mPlayers.erase(found_it);
    }


    void VideoAdvancedService::registerObjectCreators(rtti::Factory &factory)
    {
        factory.addObjectCreator(std::make_unique<VideoPlayerAdvancedObjectCreator>(*this));
        factory.addObjectCreator(std::make_unique<VideoPixelFormatRGBAHandlerObjectCreator>(*this));
        factory.addObjectCreator(std::make_unique<ThreadedVideoPlayerObjectCreator>(*this));
    }
}
