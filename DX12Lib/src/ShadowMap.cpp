#include "DX12Lib/ShadowMap.h"
#include "DX12Lib/Util.h"

namespace DX12Lib
{
	ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height)
	{
		mDevice = device;

		InitDescriptorHeap();
		OnResize(width, height);
	}

	void ShadowMap::OnResize(UINT width, UINT height)
	{
		mWidth = width;
		mHeight = height;

		mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
		mScissorRect = { 0, 0, (int)width, (int)height };

		InitResource();

		// New resource, so we need new descriptors to that resource.
		InitDescriptors();
	}

	void ShadowMap::InitResource()
	{
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = mWidth;
		texDesc.Height = mHeight;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);

		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			&optClear,
			IID_PPV_ARGS(&mShadowMap)))
	}

	void ShadowMap::InitDescriptorHeap()
	{
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;
		ThrowIfFailed(mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
	}

	void ShadowMap::InitDescriptors()
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS ;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		mDevice->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, mSrvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		mDevice->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	}
}
