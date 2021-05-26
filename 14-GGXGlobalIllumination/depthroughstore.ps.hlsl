Texture2D<float4>   worldpos;
Texture2D<float4>   roughness;

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	uint2 pixelPos = (uint2)pos.xy;
	float depth = len(worldpos[pixelPos] - gCamera.posW);
	float alpha = roughness[pixelPos].a;

	return float4(depth, alpha, 0, 0);
}
