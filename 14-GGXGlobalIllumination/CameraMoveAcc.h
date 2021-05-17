/**********************************************************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#  * Redistributions of code must retain the copyright notice, this list of conditions and the following disclaimer.
#  * Neither the name of NVIDIA CORPORATION nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT
# SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************************/

#pragma once
#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/SimpleVars.h"
#include "../SharedUtils/FullscreenLaunch.h"

#include <vector>

class CameraMoveAccPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, CameraMoveAccPass>
{
public:
	using SharedPtr = std::shared_ptr<CameraMoveAccPass>;
	using SharedConstPtr = std::shared_ptr<const CameraMoveAccPass>;

	static SharedPtr create(const std::string &bufferToAccumulate) { return SharedPtr(new CameraMoveAccPass(bufferToAccumulate)); }
	virtual ~CameraMoveAccPass() = default;

protected:
	CameraMoveAccPass(const std::string &bufferToAccumulate);

	// Implementation of SimpleRenderPass interface
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
	void execute(RenderContext* pRenderContext) override;
	void renderGui(Gui* pGui) override;
	void resize(uint32_t width, uint32_t height) override;
	void stateRefreshed() override;

	// Override some functions that provide information to the RenderPipeline class
	bool appliesPostprocess() override { return true; }
	bool hasAnimation() override { return false; }

	// A helper utility to determine if the current scene (if any) has had any camera motion
	bool hasCameraMoved();

	// Information about the rendering texture we're accumulating into
	std::string                   mAccumChannel;

	// State for our accumulation shader
	FullscreenLaunch::SharedPtr   mpAccumShader;
	FullscreenLaunch::SharedPtr   mpSampleUpdateShader;
	FullscreenLaunch::SharedPtr   mpNormalWoStoreShader;
	FullscreenLaunch::SharedPtr   mpDepthRoughnessStoreShader;

	GraphicsState::SharedPtr      mpGfxState;
	Texture::SharedPtr mpAccumFrame;
	std::array<Texture::SharedPtr, 2> mpSUpdatePingPong;
	std::vector<Texture::SharedPtr> mpLastFrames;
	std::vector<Texture::SharedPtr> mpLastFramesMat1;
	std::vector<Texture::SharedPtr> mpLastFramesMat2;
	std::vector<Camera::SharedPtr> mpLastCameras;
	std::uint8_t mbCurrentBufferPos;
	std::uint8_t mbCurrentBufferSize;
	Fbo::SharedPtr                mpInternalFbo;
	Fbo::SharedPtr                mpDepthRoughnessFbo;

	// We stash a copy of our current scene.  Why?  To detect if changes have occurred.
	Scene::SharedPtr              mpScene;
	mat4                          mpLastCameraMatrix;

	// Is our accumulation enabled?
	bool                          mDoAccumulation = true;

	// How many frames have we accumulated so far?
	std::uint8_t                  mAccumCount = 0;
	std::uint8_t				  mFirstFramePosition = 0;
	std::uint8_t				  mTotalAccFrames;
};
