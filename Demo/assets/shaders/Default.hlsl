#include "Common.hlsl"

struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC : TEXCOORD;
    float3 TangentL : TANGENT;
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices  : BONEINDICES;
#endif
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
    float4 ShadowPosH : POSITION0;
    float4 SsaoPosH : POSITION1;
	float3 PosW : POSITION2;
	float3 NormalW : NORMAL;
	float3 TangentW : TANGENT;
	float2 TexC : TEXCOORD;
	nointerpolation uint MatIndex : MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	InstanceData instanceData = gInstances[instanceID];
    VertexOut vout;
	
#ifdef SKINNED
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

    float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);
    for(int i = 0; i < 4; ++i)
    {
        // Assume no nonuniform scaling when transforming normals, so 
        // that we do not have to use the inverse-transpose.

        posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
        normalL += weights[i] * mul(vin.NormalL, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
        tangentL += weights[i] * mul(vin.TangentL.xyz, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
    }

    vin.PosL = posL;
    vin.NormalL = normalL;
    vin.TangentL.xyz = tangentL;
#endif
	
	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), instanceData.World);
	vout.PosW = posW.xyz;

	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	vout.NormalW = mul(vin.NormalL, (float3x3) instanceData.World);
    vout.TangentW = mul(vin.TangentL, (float3x3) instanceData.World);
	
	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);
	
    vout.SsaoPosH = mul(posW, gViewProjTex);
	
	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), instanceData.TexTransform);
	vout.MatIndex = instanceData.MaterialIndex;
	vout.TexC = mul(texC, gMaterials[vout.MatIndex].MatTransform).xy;

	// Generate projective tex-coords to project shadow map onto scene.
	vout.ShadowPosH = mul(posW, gShadowTransform);
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	MaterialData material = gMaterials[pin.MatIndex];
	float4 diffuseAlbedo = gTextureMaps[material.DiffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC) * material.DiffuseAlbedo;
	
#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(diffuseAlbedo.a - 0.1f);
#endif

	// Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);
	
	float4 normalMapSample = gTextureMaps[material.NormalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);

	//bumpedNormalW = pin.NormalW;
	
	// Vector from point being lit to eye.
	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW = normalize(toEyeW);

	// Finish texture projection and sample SSAO map.
    pin.SsaoPosH /= pin.SsaoPosH.w;
    float ambientAccess = gSsaoMap.Sample(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;
	
    ambientAccess = 1.0f;
	
	// Light terms.
	float4 ambient = ambientAccess * gAmbientLight * diffuseAlbedo;

	// Only the first light casts a shadow.
	float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
	shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);
	
	// Alpha channel stores shininess at per-pixel level.
	const float shininess = (1.0f - material.Roughness) * normalMapSample.a;;
	Material mat = { diffuseAlbedo, material.FresnelR0, shininess };
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW, bumpedNormalW, toEyeW, shadowFactor);

	float4 litColor = ambient + directLight;

	// Add in specular reflections.
	float3 r = reflect(-toEyeW, bumpedNormalW);
	float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
	float3 fresnelFactor = SchlickFresnel(material.FresnelR0, bumpedNormalW, r);
	litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
#ifdef FOG
	float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);
#endif

	// Common convention to take alpha from diffuse albedo.
	litColor.a = diffuseAlbedo.a;
	
	return litColor;
}
