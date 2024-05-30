#include "Lighting.hlsl"

Texture2DArray gDiffuseMaps : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
	float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPerObjectPad2;

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float gRoughness;
	float4x4 gMatTransform;
};

struct VertexIn
{
	float3 PosL : POSITION;
	float2 Size : SIZE;
};

struct VertexOut
{
	float3 PosL : POSITION;
	float2 Size : SIZE;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosL = vin.PosL;
	vout.Size = vin.Size;
	return vout;
}

struct GeoOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC : TEXCOORD;
	uint PrimID : SV_PrimitiveID;
};

[maxvertexcount(4)]
void GS(point VertexOut gin[1], uint primID : SV_PrimitiveID, inout TriangleStream<GeoOut> triStream)
{
	float3 toEye = gEyePosW - gin[0].PosL;
	toEye.y = 0.0f;
	toEye = normalize(toEye);
	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 right = cross(up, toEye);
	
	float halfWidth = gin[0].Size.x * 0.5f;
	float halfHeight = gin[0].Size.y * 0.5f;
	
	float4 v[4];
	v[0] = float4(gin[0].PosL + right * halfWidth - up * halfHeight, 1.0f);
	v[1] = float4(gin[0].PosL + right * halfWidth + up * halfHeight, 1.0f);
	v[2] = float4(gin[0].PosL - right * halfWidth - up * halfHeight, 1.0f);
	v[3] = float4(gin[0].PosL - right * halfWidth + up * halfHeight, 1.0f);
	
	float2 uv[4];
	uv[0] = float2(0.0f, 1.0f);
	uv[1] = float2(0.0f, 0.0f);
	uv[2] = float2(1.0f, 1.0f);
	uv[3] = float2(1.0f, 0.0f);
	
	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		GeoOut gout;
		gout.PosW = v[i].xyz;
		gout.PosH = mul(v[i], gViewProj);
		gout.TexC = uv[i];
		gout.NormalW = toEye;
		gout.PrimID = primID;
		triStream.Append(gout);
	}
}


float4 PS(GeoOut pin) : SV_Target
{
	float3 texCoord = float3(pin.TexC, pin.PrimID % 4);
	float4 diffuseAlbedo = gDiffuseMaps.Sample(gsamAnisotropicWrap, texCoord) * gDiffuseAlbedo;
	
#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(diffuseAlbedo.a - 0.1f);
#endif

	// Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);

	// Vector from point being lit to eye. 
	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye; // normalize

	// Light terms.
	float4 ambient = gAmbientLight * diffuseAlbedo;

	const float shininess = 1.0f - gRoughness;
	Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
		pin.NormalW, toEyeW, shadowFactor);

	float4 litColor = ambient + directLight;

#ifdef FOG
	float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);
#endif

	// Common convention to take alpha from diffuse albedo.
	litColor.a = diffuseAlbedo.a;
	
	return litColor;
}
