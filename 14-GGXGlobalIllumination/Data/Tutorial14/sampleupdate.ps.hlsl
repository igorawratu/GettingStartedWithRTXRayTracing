#include "HostDeviceSharedMacros.h"

import ShaderCommon;

cbuffer SUpdateCB
{
	int gCurrentBufferSize;
	CameraData gSampleCamera;
	bool gUsePrevBuffer;
    int gImageWidth;
    int gImageHeight;
    float gDThresh;
}

Texture2D<float4> gPos;
Texture2D<float4> gPrevRender;
Texture2D<float4> gCurrRender;
Texture2D<float4> gPrevDirect;
Texture2D<float4> gNormal;
Texture2D<float4> gWo;
Texture2D<float4> gPrevAccum;
Texture2D<float4> gDepthRough;
Texture2D<float4> gMatDif;
Texture2D<float4> gMatSpec;

#define PI 3.141592653

float ggxNormalDistribution(float NdotH, float roughness)
{
    float a2 = roughness * roughness;
    float d = ((NdotH * a2 - NdotH) * NdotH + 1);
    return a2 / max(0.001f, (d * d * M_PI));
}

float probabilityToSampleDiffuse(float3 difColor, float3 specColor)
{
    float lumDiffuse = max(0.01f, luminance(difColor.rgb));
    float lumSpecular = max(0.01f, luminance(specColor.rgb));
    return lumDiffuse / (lumDiffuse + lumSpecular);
}

float2 UVFromRGB(float3 rgbcol)
{
    rgbcol *= 255;

    float2 uvcol;
    uvcol.x = -0.148 * rgbcol.r - 0.291 * rgbcol.g + 0.439 * rgbcol.b + 128;
    uvcol.y =  0.439 * rgbcol.r - 0.368 * rgbcol.g - 0.071 * rgbcol.b + 128;

    return uvcol / 255;
}

float colDistanceChrom(float3 c1, float3 c2){
    float2 c1uv = UVFromRGB(c1);
    float2 c2uv = UVFromRGB(c2);

    return length(c1uv - c2uv);
}

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	//get current world position and compute hypothetical ray to it from previous camera
    uint2 pixelPos = (uint2)pos.xy;
    float4 prevAccum = gUsePrevBuffer ? gPrevAccum[pixelPos] : float4(0, 0, 0, 0);
    
    float4 position = gPos[pixelPos];
    float4 specCol = gMatSpec[pixelPos];
    float4 difCol = gMatDif[pixelPos];
    float3 currcol = gCurrRender[pixelPos].xyz;

    //find pixel pos of current position to previous render's camera
    float4 local_pos = mul(position, gSampleCamera.viewMat);

    //behind camera or out of frustum
    if(-local_pos.z < gSampleCamera.nearZ || -local_pos.z > gSampleCamera.farZ){
        return prevAccum;
    }

    float wLength = length(gSampleCamera.cameraW);
    float2 ss_pos = float2(wLength * local_pos.x / -local_pos.z, wLength * local_pos.y / -local_pos.z);
    ss_pos /= float2(length(gSampleCamera.cameraU), -length(gSampleCamera.cameraV));

    if(ss_pos.x < -1 || ss_pos.x > 1 || ss_pos.y < -1 || ss_pos.y > 1){
        return prevAccum;
    }

    float2 prevPixelPos = float2((ss_pos.x + 1) / 2 * gImageWidth, (ss_pos.y + 1) / 2 * gImageHeight);
    float3 norm = gNormal[prevPixelPos].xyz;
    float3 wo = gWo[prevPixelPos].xyz;
    float4 posrough = gDepthRough[prevPixelPos];
    float3 prevcol = gPrevRender[prevPixelPos].rgb;

    //check for discontinuities in depth to decide if discard is needed
    float pointDist = length(posrough.xyz - position.xyz);

    bool failedDistThresh = pointDist > gDThresh;

    if(failedDistThresh){
        return prevAccum;
    }

    //compute roughness, wo, and normal of prev, and wi of current and prev
    float rough = posrough.w;
    float3 prevWi = normalize(position.xyz - gSampleCamera.posW);
    float3 wi = normalize(position.xyz - gCamera.posW);

    float3 oldh = normalize(wo + prevWi);
    float3 newh = normalize(wo + wi);
    float NoH = saturate(dot(norm, newh));
    float prevNoH = saturate(dot(norm, oldh));

    float prevD = ggxNormalDistribution(prevNoH, rough);
    float D = ggxNormalDistribution(prevNoH, rough);
    float prevWOoH = saturate(dot(wo, oldh));
    float newWOoH = saturate(dot(wo, newh));

    float pdfSpecPrev = prevD * prevNoH / (4 * prevWOoH);
    float pdfSpec = D * NoH / (4 * newWOoH);

    float WOoN = saturate(dot(wo, norm));
    float pdfDif = WOoN / PI;

    float difMIS = probabilityToSampleDiffuse(difCol.rgb, specCol.rgb);

    float prevPdf = difMIS * pdfDif + (1 - difMIS) * pdfSpecPrev;
    float curPdf = difMIS * pdfDif + (1 - difMIS) * pdfSpec;

    float reweight = prevPdf / curPdf;

    //or some epsilon..
    if(reweight > 0){
        float3 prevFHDirect = gPrevDirect[prevPixelPos].rgb;
        float3 col = prevcol;
        float total_samples = prevAccum.a + 1;

        col -= prevFHDirect;
        col *= reweight;
        col += prevFHDirect;
        
        prevAccum = float4((prevAccum.a * prevAccum.rgb + col) / total_samples, total_samples);
    }

	return prevAccum;
}
