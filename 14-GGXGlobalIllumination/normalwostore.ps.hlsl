Texture2D<float4>   worldpos;
Texture2D<float4>   normals;

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	uint2 pixelPos = (uint2)pos.xy;
	float3 normal = normals[pixelPos].xyz;
	float3 wo = worldpos[pixelPos].xyz - gCamera.posW.xyz;

	float woPdf;
	//find wopdf here with models

	return float4(normal, woPdf);
}
