/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Local includes
#include "videorgbashader.h"
#include "renderservice.h"
#include "videoadvancedservice.h"

// External includes
#include <nap/core.h>

// nap::VideoShader run time class definition 
RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoRGBAShader)
	RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS


//////////////////////////////////////////////////////////////////////////
// VideoShader
//////////////////////////////////////////////////////////////////////////

namespace nap
{
	namespace shader
	{
		inline constexpr const char* videorgba = "videorgba";
	}


	VideoRGBAShader::VideoRGBAShader(Core& core) : Shader(core),
		mRenderService(core.getService<RenderService>())
	{ }


	bool VideoRGBAShader::init(utility::ErrorState& errorState)
	{
		if (!Shader::init(errorState))
			return false;

        auto* videoadvanced_service = mRenderService->getCore().getService<VideoAdvancedService>();

        std::string relative_path = utility::joinPath({ "shaders", utility::appendFileExtension(shader::videorgba, "vert") });
        const std::string vertex_shader_path = videoadvanced_service->getModule().findAsset(relative_path);
        if (!errorState.check(!vertex_shader_path.empty(), "%s: Unable to find %s vertex shader %s", mRenderService->getModule().getName().c_str(), shader::videorgba, vertex_shader_path.c_str()))
            return false;

        relative_path = utility::joinPath({ "shaders", utility::appendFileExtension(shader::videorgba, "frag") });
        const std::string fragment_shader_path = videoadvanced_service->getModule().findAsset(relative_path);
        if (!errorState.check(!vertex_shader_path.empty(), "%s: Unable to find %s fragment shader %s", mRenderService->getModule().getName().c_str(), shader::videorgba, fragment_shader_path.c_str()))
            return false;

        // Read vert shader file
        std::string vert_source;
        if (!errorState.check(utility::readFileToString(vertex_shader_path, vert_source, errorState), "Unable to read %s vertex shader file", shader::videorgba))
            return false;

        // Read frag shader file
        std::string frag_source;
        if (!errorState.check(utility::readFileToString(fragment_shader_path, frag_source, errorState), "Unable to read %s fragment shader file", shader::videorgba))
            return false;

        // Copy data search paths
        const auto search_paths = videoadvanced_service->getModule().getInformation().mDataSearchPaths;

        // Compile shader
        return this->load(shader::videorgba, search_paths, vert_source.data(), vert_source.size(), frag_source.data(), frag_source.size(), errorState);
	}
}
