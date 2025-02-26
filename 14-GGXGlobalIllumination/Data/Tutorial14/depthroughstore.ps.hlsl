#include "HostDeviceSharedMacros.h"

import ShaderCommon;

Texture2D<float4>   worldpos;
Texture2D<float4>   normals;
Texture2D<float4>   roughness;
Texture2D<float4>   firstHitWo;
Texture2D<float4>   direct;

RWTexture2D<float4> depthrough;
RWTexture2D<float4> normwo;
RWTexture2D<float4> directOut;

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	uint2 pixelPos = (uint2)pos.xy;

    float3 normal = normals[pixelPos].xyz;
    float3 wo = FirstHitWo[pixelPos].xyz;
    float depth = length(worldpos[pixelPos].xyz - gCamera.posW);

    float2 normalSpherical = float2(acos(normal.z), atan2(normal.y, normal.x));
    float2 woSpherical = float2(acos(wo.z), atan2(wo.y, wo.x));;

    normwo[pixelPos] = float4(normalSpherical.x, normalSpherical.y, woSpherical.x, woSpherical.y);
	float alpha = roughness[pixelPos].a;
    depthrough[pixelPos] = float4(depth, alpha, 0, 0);
    directOut[pixelPos] = direct[pixelPos];

	return 0;
}
