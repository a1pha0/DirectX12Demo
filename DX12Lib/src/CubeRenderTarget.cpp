#include "DX12Lib/CubeRenderTarget.h"
#include "DX12Lib/Util.h"

namespace DX12Lib
{
	CubeRenderTarget::CubeRenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat)
	{
		mDevice = device;
		mRTFormat = rtFormat;
		mDSFormat = dsFormat;
		
		InitDescriptorHeap();
		OnResize(width, height);
	}

	void CubeRenderTarget::InitCubeFaceCamera(float x, float y, float z)
	{
		// Generate the cube map about the given position.
		DirectX::XMFLOAT3 center(x, y, z);
		DirectX::XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);

		// Look along each coordinate axis.
		DirectX::XMFLOAT3 directions[6] =
		{
			DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),  // +X
			DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), // -X
			DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),  // +Y
			DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), // -Y
			DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f),  // +Z
			DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), // -Z
		};

		// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
		// are looking down +Y or -Y, so we need a different "up" vector.
		DirectX::XMFLOAT3 ups[6] =
		{
			DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),  // +X
			DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),  // -X
			DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
			DirectX::XMFLOAT3(0.0f, 0.0f, +1.0f), // -Y
			DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),  // +Z
			DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)   // -Z
		};

		for (int i = 0; i < 6; ++i)
		{
			mCubeMapCamera[i] = std::make_unique<Camera>();
			mCubeMapCamera[i]->LookTo(center, directions[i], ups[i]);
			mCubeMapCamera[i]->SetPerspective(DirectX::XM_PIDIV2, 1.0f, 1.0f, 1000.0f);
			mCubeMapCamera[i]->UpdateViewMatrix();
		}
	}

	void CubeRenderTarget::OnResize(UINT width, UINT height)
	{
		mWidth = width;
		mHeight = height;

		mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
		mScissorRect = { 0, 0, (int)width, (int)height };

		InitResources();

		// New resource, so we need new descriptors to that resource.
		InitDescriptors();
	}

	void CubeRenderTarget::InitResources()
	{
		D3D12_RESOURCE_DESC renderTargetDesc = {};
		renderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		renderTargetDesc.Alignment = 0;
		renderTargetDesc.Width = mWidth;
		renderTargetDesc.Height = mHeight;
		renderTargetDesc.DepthOrArraySize = 6;
		renderTargetDesc.MipLevels = 1;
		renderTargetDesc.Format = mRTFormat;
		renderTargetDesc.SampleDesc.Count = 1;
		renderTargetDesc.SampleDesc.Quality = 0;
		renderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		renderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE optRtClear;
		optRtClear.Format = mRTFormat;
		optRtClear.DepthStencil.Depth = 1.0f;
		optRtClear.DepthStencil.Stencil = 0;
		optRtClear.Color[0] = 0.7f;
		optRtClear.Color[1] = 0.7f;
		optRtClear.Color[2] = 0.7f;
		optRtClear.Color[3] = 1.0f;

		CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);

		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&renderTargetDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			&optRtClear,
			IID_PPV_ARGS(&mCubeMap)))

		// Create the depth/stencil buffer and view.
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = mWidth;
		depthStencilDesc.Height = mHeight;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = mDSFormat;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optDsClear;
		optDsClear.Format = mDSFormat;
		optDsClear.DepthStencil.Depth = 1.0f;
		optDsClear.DepthStencil.Stencil = 0;

		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optDsClear,
			IID_PPV_ARGS(&mDepthStencilBuffer)));
	}

	void CubeRenderTarget::InitDescriptorHeap()
	{
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.NumDescriptors = 6;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;
		ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;
		ThrowIfFailed(mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
	}

	void CubeRenderTarget::InitDescriptors()
	{
		// Create SRV to the entire cubemap resource.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = mRTFormat;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		mDevice->CreateShaderResourceView(mCubeMap.Get(), &srvDesc, mSrvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create RTV to each cube face.
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = mRTFormat;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;
		rtvDesc.Texture2DArray.ArraySize = 1; // Only view one element of the array.

		D3D12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (int i = 0; i < 6; ++i)
		{
			rtvDesc.Texture2DArray.FirstArraySlice = i; // Render target to ith element.

			// Create RTV to ith cubemap face.
			mDevice->CreateRenderTargetView(mCubeMap.Get(), &rtvDesc, hCpuDescriptor);
			hCpuDescriptor.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// Create descriptor to mip level 0 of entire resource using the format of the resource.
		mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	}
}
