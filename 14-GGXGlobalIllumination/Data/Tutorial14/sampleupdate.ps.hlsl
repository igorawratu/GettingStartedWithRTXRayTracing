#include "HostDeviceSharedMacros.h"

import ShaderCommon;

cbuffer SUpdateCB
{
	int gCurrentBufferSize;
	CameraData gSampleCamera;
	bool gUsePrevBuffer;
}

Texture2D<float4> gPos;
Texture2D<float4> gPrevRender;
Texture2D<float4> gNormalWo;
Texture2D<float4> gDepthRoughness;
Texture2D<float4> gPrevAccum;

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	//get current world position and compute hypothetical ray to it from previous camera
	
	//see which pixel this would have corresponded to in the previous camera render
	
	//update the sample at that pixel with the various sample update strategies and blend that with the prevaccum if gUsePrevBuffer is set and return the output

	return float4(0, 0, 0, 0);
}
