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

#include "CameraMoveAcc.h"

namespace {
	const char *kAccumShader = "Tutorial14\\cameramoveacc.ps.hlsl";
	const char *kSUpdateShader = "Tutorial14\\sampleupdate.ps.hlsl";
	const char *kNormalWoStore = "Tutorial14\\normalwostore.ps.hlsl";
	const char *kDepthRoughStore = "Tutorial14\\depthroughstore.ps.hlsl";
	const std::uint8_t MAX_BUFFER_SIZE = 10;
};

CameraMoveAccPass::CameraMoveAccPass(const std::string &bufferToAccumulate)
	: ::RenderPass("Accumulation Pass", "Accumulation Options")
{
	mAccumChannel = bufferToAccumulate;
}

bool CameraMoveAccPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	if (!pResManager) return false;

	// Stash our resource manager; ask for the texture the developer asked us to accumulate
	mpResManager = pResManager;
	mpResManager->requestTextureResources({ "WorldNormal", "FirstHitWo", "MaterialSpecRough", "WorldPosition" });
	mpResManager->requestTextureResource(mAccumChannel);

	// Create our graphics state and accumulation shader
	mpGfxState = GraphicsState::create();
	mpAccumShader = FullscreenLaunch::create(kAccumShader);
	mpSampleUpdateShader = FullscreenLaunch::create(kSUpdateShader);
	mpNormalWoStoreShader = FullscreenLaunch::create(kNormalWoStore);
	mpDepthRoughnessStoreShader = FullscreenLaunch::create(kDepthRoughStore);

	// Our GUI needs less space than other passes, so shrink the GUI window.
	setGuiSize(ivec2(250, 135));

	return true;
}

void CameraMoveAccPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Reset accumulation.
	mAccumCount = 0;

	// When our renderer moves around, we want to reset accumulation
	mpScene = pScene;

	// Grab a copy of the current scene's camera matrix (if it exists)
	if (mpScene && mpScene->getActiveCamera())
		mpLastCameraMatrix = mpScene->getActiveCamera()->getViewMatrix();
}

void CameraMoveAccPass::resize(uint32_t width, uint32_t height)
{
	mpLastFrames.clear();
	mpLastFramesMat1.clear();
	mpLastFramesMat2.clear();
	mpLastCameras.clear();

	for (std::uint8_t i = 0; i < MAX_BUFFER_SIZE; ++i) {
		mpLastFrames.push_back(Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr,
			Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget));

		//roughness and depth
		mpLastFramesMat1.push_back(Texture::create2D(width, height, ResourceFormat::RG32Float, 1, 1, nullptr,
			Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget));

		//normal and wo
		mpLastFramesMat2.push_back(Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr,
			Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget));

		mpLastCameras.push_back(Camera::create());
	}

	for (std::uint8_t i = 0; i < 2; ++i) {
		mpSUpdatePingPong[i] = Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr,
			Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget);
	}
	
	// Resize internal resources
	mpAccumFrame = Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget);

	// We need a framebuffer to attach to our graphics pipe state (when running our full-screen pass)
	mpInternalFbo = ResourceManager::createFbo(width, height, ResourceFormat::RGBA32Float);
	mpGfxState->setFbo(mpInternalFbo);

	mpDepthRoughnessFbo = ResourceManager::createFbo(width, height, ResourceFormat::RG16Float);

	// Whenever we resize, we'd better force accumulation to restart
	mAccumCount = 0;

	mbCurrentBufferPos = 0;
	mbCurrentBufferSize = 0;
}

void CameraMoveAccPass::renderGui(Gui* pGui)
{
	// Print the name of the buffer we're accumulating from and into.  Add a blank line below that for clarity
	pGui->addText((std::string("Accumulating buffer:   ") + mAccumChannel).c_str());
	pGui->addText("");

	// Add a toggle to enable/disable temporal accumulation.  Whenever this toggles, reset the
	//     frame count and tell the pipeline we're part of that our rendering options have changed.
	if (pGui->addCheckBox(mDoAccumulation ? "Accumulating samples temporally" : "No temporal accumulation", mDoAccumulation))
	{
		mAccumCount = 0;
		setRefreshFlag();
	}

	// Display a count of accumulated frames
	pGui->addText("");
	pGui->addText((std::string("Frames accumulated: ") + std::to_string(mAccumCount)).c_str());
}

bool CameraMoveAccPass::hasCameraMoved()
{
	// Has our camera moved?
	return mpScene &&                      // No scene?  Then the answer is no
		mpScene->getActiveCamera() &&   // No camera in our scene?  Then the answer is no
		(mpLastCameraMatrix != mpScene->getActiveCamera()->getViewMatrix());   // Compare the current matrix with the last one
}

void CameraMoveAccPass::execute(RenderContext* pRenderContext)
{
	// Grab the texture to accumulate
	Texture::SharedPtr inputTexture = mpResManager->getTexture(mAccumChannel);

	// If our input texture is invalid, or we've been asked to skip accumulation, do nothing.
	if (!inputTexture || !mDoAccumulation) return;

	// If the camera in our current scene has moved, we want to reset accumulation
	if (hasCameraMoved())
	{
		mAccumCount = 0;
		mpLastCameraMatrix = mpScene->getActiveCamera()->getViewMatrix();

		//backproject last frame positions and update accum texture here with reweight etc.
		auto supdateVars = mpSampleUpdateShader->getVars();

		supdateVars["SUpdateCB"]["gCurrentBufferSize"] = mbCurrentBufferSize;
		supdateVars["gPos"] = mpResManager->getTexture("WorldPosition");

		for (std::uint8_t i = 0; i < mbCurrentBufferSize; ++i) {
			supdateVars["SUpdateCB"]["gUsePrevBuffer"] = i > 0;

			int idx = (mbCurrentBufferPos + i) % mbCurrentBufferSize;
			supdateVars["gPrevRender"] = mpLastFrames[idx];
			supdateVars["gNormalWo"] = mpLastFramesMat1[idx]; 
			supdateVars["gDepthRoughness"] = mpLastFramesMat2[idx];
			mpSampleUpdateShader->setCamera(mpLastCameras[idx]);

			//may have to ping pong this
			supdateVars["prevAccum"] = mpSUpdatePingPong[i % 2];
			mpSampleUpdateShader->execute(pRenderContext, mpGfxState);
			pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), mpSUpdatePingPong[(i + 1) % 2]->getRTV());
		}

		pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), mpAccumFrame->getRTV());
	}

	//save last n frames to account for moving
	pRenderContext->blit(inputTexture->getSRV(), mpLastFrames[mbCurrentBufferPos]->getRTV());

	auto nwoVars = mpNormalWoStoreShader->getVars();
	nwoVars["normals"] = mpResManager->getTexture("WorldNormal");
	nwoVars["wo"] = mpResManager->getTexture("FirstHitWo");
	mpNormalWoStoreShader->execute(pRenderContext, mpGfxState);
	pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), mpLastFramesMat1[mbCurrentBufferPos]->getRTV());

	mpGfxState->setFbo(mpDepthRoughnessFbo);
	auto drVars = mpDepthRoughnessStoreShader->getVars();
	nwoVars["worldpos"] = mpResManager->getTexture("WorldPosition");
	nwoVars["roughness"] = mpResManager->getTexture("MaterialSpecRough");
	mpDepthRoughnessStoreShader->execute(pRenderContext, mpGfxState);
	pRenderContext->blit(mpDepthRoughnessFbo->getColorTexture(0)->getSRV(), mpLastFramesMat2[mbCurrentBufferPos]->getRTV());

	mpGfxState->setFbo(mpInternalFbo);

	*mpLastCameras[mbCurrentBufferPos] = *mpScene->getActiveCamera();

	mbCurrentBufferSize = std::min(MAX_BUFFER_SIZE, std::uint8_t(mbCurrentBufferSize + 1));
	mbCurrentBufferPos = (mbCurrentBufferPos + 1) % MAX_BUFFER_SIZE;

	// Set shader parameters for our accumulation
	auto accumVars = mpAccumShader->getVars();
	accumVars["PerFrameCB"]["gAccumCount"] = mAccumCount++;
	accumVars["gLastFrame"] = mpAccumFrame;
	accumVars["gCurFrame"] = inputTexture;

	// Do the accumulation
	mpAccumShader->execute(pRenderContext, mpGfxState);
	// We've accumulated our result.  Copy that back to the input/output buffer
	pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), inputTexture->getRTV());
	// Keep a copy for next frame (we need this to avoid reading & writing to the same resource)
	pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), mpAccumFrame->getRTV());
}

void CameraMoveAccPass::stateRefreshed()
{
	// This gets called because another pass else in the pipeline changed state.  Restart accumulation
	mAccumCount = 0;
	mbCurrentBufferPos = 0;
	mbCurrentBufferSize = 0;
}
