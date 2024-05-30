#pragma once
#include <memory>
#include "UploadBuffer.h"
#include "Util.h"

namespace DX12Lib
{
	struct MaterialData
	{
		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
		float Roughness = 0.25f;
		DirectX::XMFLOAT4X4 MatTransform = Identity4x4();
		UINT DiffuseMapIndex = -1;
		UINT NormalMapIndex = -1;
		UINT Pad0 = 0;
		UINT Pad1 = 0;
	};

	struct InstanceData
	{
		DirectX::XMFLOAT4X4 World = Identity4x4();
		DirectX::XMFLOAT4X4 TexTransform = Identity4x4();
		UINT MaterialCBIndex = -1;
		UINT Pad0 = 0;
		UINT Pad1 = 0;
		UINT Pad2 = 0;
	};

	struct PassConstant
	{
		DirectX::XMFLOAT4X4 View = Identity4x4();
		DirectX::XMFLOAT4X4 InvView = Identity4x4();
		DirectX::XMFLOAT4X4 Proj = Identity4x4();
		DirectX::XMFLOAT4X4 InvProj = Identity4x4();
		DirectX::XMFLOAT4X4 ViewProj = Identity4x4();
		DirectX::XMFLOAT4X4 InvViewProj = Identity4x4();
		DirectX::XMFLOAT4X4 ViewProjTex = Identity4x4();
		DirectX::XMFLOAT4X4 ShadowTransform = Identity4x4();
		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		float cbPerObjectPad1 = 0.0f;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		float TotalTime = 0.0f;
		float DeltaTime = 0.0f;

		DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

		DirectX::XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
		float gFogStart = 50.0f;
		float gFogRange = 500.0f;
		DirectX::XMFLOAT2 cbPerObjectPad2;

		// Indices [0, NUM_DIR_LIGHTS) are directional lights;
		// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
		// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
		// are spot lights for a maximum of MaxLights per object.
		Light Lights[MaxLights];
	};

	struct SsaoConstant
	{
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ProjTex;
		DirectX::XMFLOAT4   OffsetVectors[14];

		// For SsaoBlur.hlsl
		DirectX::XMFLOAT4 BlurWeights[3];

		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

		// Coordinates given in view space.
		float OcclusionRadius = 0.5f;
		float OcclusionFadeStart = 0.2f;
		float OcclusionFadeEnd = 2.0f;
		float SurfaceEpsilon = 0.05f;
	};

	struct SkinnedConstant
	{
		DirectX::XMFLOAT4X4 BoneTransforms[96];
	};

	class FrameResource
	{
	public:
		FrameResource(ID3D12Device* device, UINT passCount, UINT maxInstanceCount, UINT skinnedObjectCount, UINT materialCount)
		{
			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdListAlloc)));

			PassCB = std::make_unique<UploadBuffer<PassConstant>>(device, passCount, true);
			SsaoCB = std::make_unique<UploadBuffer<SsaoConstant>>(device, 1, true);
			SkinnedCB = std::make_unique<UploadBuffer<SkinnedConstant>>(device, skinnedObjectCount, true);

			InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, maxInstanceCount, false);
			MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
		}

		FrameResource(const FrameResource& rhs) = delete;
		FrameResource& operator=(const FrameResource& rhs) = delete;
		~FrameResource() = default;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

		std::unique_ptr<UploadBuffer<PassConstant>> PassCB = nullptr;
		std::unique_ptr<UploadBuffer<SsaoConstant>> SsaoCB = nullptr;
		std::unique_ptr<UploadBuffer<SkinnedConstant>> SkinnedCB = nullptr;

		std::unique_ptr<UploadBuffer<InstanceData>> InstanceBuffer = nullptr;
		std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

		UINT64 Fence = 0;
	};
}
