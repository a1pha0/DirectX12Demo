#pragma once
#include "d3dx12.h"
#include <wrl.h>
#include <DirectXMath.h>
#include "FrameResource.h"

namespace DX12Lib
{
	class Ssao
	{
	public:
		Ssao(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
		Ssao(const Ssao&) = delete;
		Ssao& operator=(const Ssao&) = delete;
		~Ssao() = default;

		static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;
		static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

		static const int MaxBlurRadius = 5;

		inline UINT GetSsaoMapWidth() const { return mRenderTargetWidth / 2; }
		inline UINT GetSsaoMapHeight() const { return mRenderTargetHeight / 2; }

		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]) { std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]); }
		std::vector<float> CalcGaussWeights(float sigma);

		inline ID3D12Resource* GetNormalMap() const { return mNormalMap.Get(); }
		inline ID3D12Resource* GetAmbientMap() const { return mAmbientMap0.Get(); }

		inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetNormalMapRtv() const { return mhNormalMapCpuRtv; }
		inline CD3DX12_GPU_DESCRIPTOR_HANDLE GetNormalMapSrv() const { return mhNormalMapGpuSrv; }
		inline CD3DX12_GPU_DESCRIPTOR_HANDLE GetAmbientMapSrv() const { return mhAmbientMap0GpuSrv; }

		void SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso);

		void Resize(UINT width, UINT height);
		void RebuildDescriptors(ID3D12Resource* depthStencilBuffer);

		///<summary>
		/// Changes the render target to the Ambient render target and draws a fullscreen
		/// quad to kick off the pixel shader to compute the AmbientMap.  We still keep the
		/// main depth buffer binded to the pipeline, but depth buffer read/writes
		/// are disabled, as we do not need the depth buffer computing the Ambient map.
		///</summary>
		void ComputeSsao(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount);

	private:
		void InitDescriptorHeap();
		void InitOffsetVectors();
		void InitRandomVectorTexture(ID3D12GraphicsCommandList* cmdList);

		void RebuildResources();
		void RebuildDescriptors(
			ID3D12Resource* depthStencilBuffer,
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
			UINT cbvSrvUavDescriptorSize,
			UINT rtvDescriptorSize);

		///<summary>
		/// Blurs the ambient map to smooth out the noise caused by only taking a
		/// few random samples per pixel.  We use an edge preserving blur so that 
		/// we do not blur across discontinuities--we want edges to remain edges.
		///</summary>
		void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount);
		void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur);

	private:
		ID3D12Device* mDevice;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSig;

		ID3D12PipelineState* mSsaoPso = nullptr;
		ID3D12PipelineState* mBlurPso = nullptr;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;

		Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap0;
		Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap1;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;

		// Need two for ping-ponging during blur.
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;

		UINT mRenderTargetWidth;
		UINT mRenderTargetHeight;

		DirectX::XMFLOAT4 mOffsets[14];

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;
	};
}
