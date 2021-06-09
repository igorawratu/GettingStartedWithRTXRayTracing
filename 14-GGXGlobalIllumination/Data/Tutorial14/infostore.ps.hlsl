#include "HostDeviceSharedMacros.h"

import ShaderCommon;

Texture2D<float4>   worldpos;
Texture2D<float4>   normals;
Texture2D<float4>   roughness;
Texture2D<float4>   firstHitWo;
Texture2D<float4>   direct;

RWTexture2D<float4> depthrough;
RWTexture2D<float4> norm;
RWTexture2D<float4> wo;
RWTexture2D<float4> directOut;

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
    uint2 pixelPos = (uint2)pos.xy;
    //float depth = length(worldpos[pixelPos].xyz - gCamera.posW);

    norm[pixelPos] = normals[pixelPos];
    wo[pixelPos] = firstHitWo[pixelPos];
    float alpha = roughness[pixelPos].a;
    depthrough[pixelPos] = float4(worldpos[pixelPos].xyz, alpha);
    directOut[pixelPos] = direct[pixelPos];

    return 0;
}
