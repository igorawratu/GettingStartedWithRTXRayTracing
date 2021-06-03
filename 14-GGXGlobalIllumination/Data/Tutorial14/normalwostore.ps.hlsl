#include "HostDeviceSharedMacros.h"

import ShaderCommon;

Texture2D<float4>   worldpos;
Texture2D<float4>   normals;

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	uint2 pixelPos = (uint2)pos.xy;
	float3 normal = normals[pixelPos].xyz;
	float3 wo = worldpos[pixelPos].xyz - gCamera.posW.xyz;

	float2 normalSpherical = float2(acos(normal.z), atan2(normal.y, normal.x));
    float2 woSpherical = float2(acos(wo.z), atan2(wo.y, wo.x));;

	return float4(normalSpherical.x, normalSpherical.y, woSpherical.x, woSpherical.y);
}
