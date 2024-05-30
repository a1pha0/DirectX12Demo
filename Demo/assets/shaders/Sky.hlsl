#include "Common.hlsl"

struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexCoord : TEXCOORD;
	float3 TangentU : TANGENT;
};

struct VertexOut
{
	float4 PositionH : SV_Position;
	float3 PositionL : POSITIONT;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	InstanceData instanceData = gInstances[instanceID];

	VertexOut vout;
	vout.PositionL = vin.PositionL;
	
	float4 positonW = mul(float4(vin.PositionL, 1.0f), instanceData.World);
	positonW.xyz += gEyePosW;
	
	vout.PositionH = mul(positonW, gViewProj).xyww;
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	return gCubeMap.Sample(gsamLinearWrap, pin.PositionL);
}
