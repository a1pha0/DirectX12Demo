#pragma once
#include "Application.h"
#include <array>
#include "AssetManager.h"
#include "FrameResource.h"
#include "Camera.h"
#include "BlurFilter.h"
#include "CubeRenderTarget.h"
#include "ShadowMap.h"
#include "Ssao.h"

namespace DX12Lib
{
	class Game : public Application
	{
	public:
		Game(HINSTANCE hInstance);
		~Game();
		bool Init() override;

	private:
		virtual void Update(const Timer& timer) override;
		virtual void Render(const Timer& timer) override;
		virtual void OnResize() override;

		virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
		virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
		virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

		inline D3D12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index) const;
		inline D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index) const;
		inline D3D12_CPU_DESCRIPTOR_HANDLE GetDsv(int index) const;
		inline D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(int index) const;

		void InitTextures();
		void InitDescriptorHeaps();
		void InitMaterials();
		void InitMeshes();
		void InitActors();
		void InitRootSignature();
		void InitPSOs();
		void InitFrameResources();

	private:
		void InitSkullMesh();
		void InitCarMesh();
		void InitSkinnedMesh();
		void InitPostProcessRootSignature();
		void InitSsaoRootSignature();

		void OnInput(const Timer& timer);
		void Tick(const Timer& timer);

		void UpdateInstanceBuffer(const Timer& timer);
		void UpdateMaterialBuffer(const Timer& timer);
		void UpdateShadowTransform(const Timer& timer);
		void UpdateMainPassCB(const Timer& timer);
		void UpdateShadowPassCB(const Timer& timer);
		void UpdateCubeMapFacePassCBs(const Timer& timer);
		void UpdateReflectedPassCB(const Timer& timer);
		void UpdateSsaoPassCB(const Timer& timer);
		void UpdateSkinnedCBs(const Timer& timer);

		void RenderActors(ID3D12GraphicsCommandList* cmdList, const FrameResource* frameResource, const std::vector<Actor*>& actors);
		void RenderSceneToCubeMap();
		void RenderSceneToShadowMap();
		void RenderSceneToBackbuffer();
		void RenderNormalsAndDepth();

		std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
		void Pick(int screenX, int screenY);

	private:
		AssetManager mAssetManager;
		Actor* mPickedActor = nullptr;

		std::wstring mSkinnedModelFilename = L"assets/models/soldier.m3d";
		std::unique_ptr<SkinnedMesh> mSkinnedModelInst;
		SkinnedData mSkinnedInfo;
		std::vector<M3DLoader::Subset> mSkinnedSubsets;
		std::vector<M3DLoader::M3dMaterial> mSkinnedMats;
		std::vector<std::wstring> mSkinnedTextureNames;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;

		enum RootSignatureParams : UINT
		{
			RSP_InstanceBuffer = 0,
			RSP_MaterialBuffer = 1,
			RSP_PassCB = 2,
			RSP_SkinnedCB = 3,
			RSP_CubeMap = 4,
			RSP_ShadowMap = 5,
			RSP_SsaoMap = 6,
			RSP_AllTextures = 7,
		};

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mPostProcessRootSignature;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSignature;

		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		const float mClearColor[4] = { 0.7f, 0.7f, 0.7f, 1.0f };

		std::vector<std::unique_ptr<FrameResource>> mFrameResources;
		int mCurrFrameResourceIndex = 0;
		PassConstant mMainPassCB;
		PassConstant mShadowPassCB;
		PassConstant mReflectedPassCB;

		Camera mCamera;
		DirectX::BoundingFrustum mCameraFrustum;

		//std::unique_ptr<BlurFilter> mBlurFilter;
		std::unique_ptr<CubeRenderTarget> mDynamicCubeMap = nullptr;

		DirectX::BoundingSphere mSceneBound;
		std::unique_ptr<ShadowMap> mShadowMap = nullptr;
		DirectX::XMFLOAT4X4 mShadowTransform;

		std::unique_ptr<Ssao> mSsao;

		POINT mMousePos = { 0, 0 };

		Light mLights[3];
		DirectX::XMFLOAT3 mLightPosW;
		float mLightNearZ, mLightFarZ;
		DirectX::XMFLOAT4X4 mLightView, mLightProj;
	};
}
 