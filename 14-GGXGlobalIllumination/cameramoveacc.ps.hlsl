cbuffer PerFrameCB
{
	uint gAccumCount;
}

Texture2D<float4>   gLastFrame;
Texture2D<float4>   gCurFrame;

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	uint2 pixelPos = (uint2)pos.xy;
	float4 curColor = gCurFrame[pixelPos];
	float4 prevColor = float4(gLastFrame[pixelPos].rgb, 1);
	uint corrected_accum_count = gAccumCount + gLastFrame[pixelPos].a;

	float4 final = (corrected_accum_count * prevColor + curColor) / (corrected_accum_count + 1);
	final.a = gLastFrame[pixelPos].a;

	return final;
}
