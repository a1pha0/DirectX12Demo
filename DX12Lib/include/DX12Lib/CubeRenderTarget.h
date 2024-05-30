#pragma once
#include "d3dx12.h"
#include <wrl.h>
#include "Camera.h"

namespace DX12Lib
{
	enum class CubeMapFace : int
	{
		PositiveX = 0,
		NegativeX = 1,
		PositiveY = 2,
		NegativeY = 3,
		PositiveZ = 4,
		NegativeZ = 5,
	};

	class CubeRenderTarget
	{
	public:
		CubeRenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat);
		CubeRenderTarget(const CubeRenderTarget&) = delete;
		CubeRenderTarget& operator=(const CubeRenderTarget&) = delete;
		~CubeRenderTarget() = default;

		void InitCubeFaceCamera(float x, float y, float z);
		void OnResize(UINT width, UINT height);
		
		inline ID3D12Resource* GetResource() const { return mCubeMap.Get(); }
		inline D3D12_VIEWPORT GetViewport() const { return mViewport; }
		inline D3D12_RECT GetScissorRect() const { return mScissorRect; }

		ID3D12DescriptorHeap* GetSRVHeap() const { return mSrvHeap.Get(); }
		ID3D12DescriptorHeap* GetRTVHeap() const { return mRtvHeap.Get(); }
		ID3D12DescriptorHeap* GetDSVHeap() const { return mDsvHeap.Get(); }

		inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() { return mSrvHeap->GetGPUDescriptorHandleForHeapStart(); }
		inline D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(int index)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE handle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
			handle.ptr += index * mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			return handle;
		}
		inline D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() { return mDsvHeap->GetCPUDescriptorHandleForHeapStart(); }

		inline Camera* GetCamera(int index) const { return mCubeMapCamera[index].get(); }

	private:
		void InitResources();
		void InitDescriptorHeap();
		void InitDescriptors();

	private:
		std::unique_ptr<Camera> mCubeMapCamera[6];

		ID3D12Device* mDevice;
		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		UINT mWidth, mHeight;

		DXGI_FORMAT mRTFormat;
		DXGI_FORMAT mDSFormat;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

		Microsoft::WRL::ComPtr<ID3D12Resource> mCubeMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;
	};
}
