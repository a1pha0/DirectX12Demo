#pragma once
#include "d3dx12.h"
#include <wrl.h>

namespace DX12Lib
{
	class ShadowMap
	{
	public:
		ShadowMap(ID3D12Device* device, UINT width, UINT height);
		ShadowMap(const ShadowMap&) = delete;
		ShadowMap& operator=(const ShadowMap&) = delete;
		~ShadowMap() = default;

		void OnResize(UINT width, UINT height);

		inline UINT GetWidth() const { return mWidth; }
		inline UINT GetHeight() const { return mHeight; }
		inline ID3D12Resource* GetResource() const { return mShadowMap.Get(); }
		inline D3D12_VIEWPORT GetViewport() const { return mViewport; }
		inline D3D12_RECT GetScissorRect() const { return mScissorRect; }

		ID3D12DescriptorHeap* GetSRVHeap() const { return mSrvHeap.Get(); }
		//ID3D12DescriptorHeap* GetRTVHeap() const { return mRtvHeap.Get(); }
		ID3D12DescriptorHeap* GetDSVHeap() const { return mDsvHeap.Get(); }

		inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() { return mSrvHeap->GetGPUDescriptorHandleForHeapStart(); }
		//inline D3D12_GPU_DESCRIPTOR_HANDLE GetRTV() { return mRtvHeap->GetGPUDescriptorHandleForHeapStart(); }
		inline D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() { return mDsvHeap->GetCPUDescriptorHandleForHeapStart(); }

	private:
		void InitResource();
		void InitDescriptorHeap();
		void InitDescriptors();

	private:
		ID3D12Device* mDevice;
		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		UINT mWidth, mHeight;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
		//Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

		Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;
	};
}
