#include "DX12Lib/Ssao.h"
#include "DX12Lib/Util.h"
#include <DirectXPackedVector.h>

namespace DX12Lib
{
	Ssao::Ssao(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
	{
		mDevice = device;
		InitDescriptorHeap();
		InitOffsetVectors();
		InitRandomVectorTexture(cmdList);
	}

	std::vector<float> Ssao::CalcGaussWeights(float sigma)
	{
		float twoSigma2 = 2.0f * sigma * sigma;

		// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
		// For example, for sigma = 3, the width of the bell curve is 
		int blurRadius = (int)ceil(2.0f * sigma);

		assert(blurRadius <= MaxBlurRadius);

		std::vector<float> weights;
		weights.resize(2 * blurRadius + 1);

		float weightSum = 0.0f;

		for (int i = -blurRadius; i <= blurRadius; ++i)
		{
			float x = (float)i;

			weights[i + blurRadius] = expf(-x * x / twoSigma2);

			weightSum += weights[i + blurRadius];
		}

		// Divide by the sum so all the weights add up to 1.0.
		for (int i = 0; i < weights.size(); ++i)
		{
			weights[i] /= weightSum;
		}

		return weights;
	}

	void Ssao::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)
	{
		mSsaoPso = ssaoPso;
		mBlurPso = ssaoBlurPso;
	}

	void Ssao::Resize(UINT width, UINT height)
	{
		mRenderTargetWidth = width;
		mRenderTargetHeight = height;

		// We render to ambient map at half the resolution.
		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = mRenderTargetWidth / 2.0f;
		mViewport.Height = mRenderTargetHeight / 2.0f;
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		mScissorRect = { 0, 0, (int)mRenderTargetWidth / 2, (int)mRenderTargetHeight / 2 };

		RebuildResources();
	}

	void Ssao::RebuildDescriptors(ID3D12Resource* depthStencilBuffer)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvHeap->GetGPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
		UINT cbvSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		UINT rtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		RebuildDescriptors(depthStencilBuffer, hCpuSrv, hGpuSrv, hCpuRtv, cbvSrvUavDescriptorSize, rtvDescriptorSize);
	}

	void Ssao::ComputeSsao(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount)
	{
		cmdList->RSSetViewports(1, &mViewport);
		cmdList->RSSetScissorRects(1, &mScissorRect);

		// We compute the initial SSAO to AmbientMap0.

		// Change to RENDER_TARGET.
		CD3DX12_RESOURCE_BARRIER barrier0 = CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmdList->ResourceBarrier(1, &barrier0);

		float clearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		cmdList->ClearRenderTargetView(mhAmbientMap0CpuRtv, clearValue, 0, nullptr);

		// Specify the buffers we are going to render to.
		cmdList->OMSetRenderTargets(1, &mhAmbientMap0CpuRtv, true, nullptr);

		// Bind the constant buffer for this pass.
		auto ssaoCBAddress = currFrame->SsaoCB->Resource()->GetGPUVirtualAddress();
		cmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);
		cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);

		// Bind the normal and depth maps.
		cmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);

		// Bind the random vector map.
		cmdList->SetGraphicsRootDescriptorTable(3, mhRandomVectorMapGpuSrv);

		cmdList->SetPipelineState(mSsaoPso);

		// Draw fullscreen quad.
		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(6, 1, 0, 0);

		// Change back to GENERIC_READ so we can read the texture in a shader.
		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList->ResourceBarrier(1, &barrier1);

		BlurAmbientMap(cmdList, currFrame, blurCount);
	}

	void Ssao::InitDescriptorHeap()
	{
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
		srvHeapDesc.NumDescriptors = 5;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srvHeapDesc.NodeMask = 0;
		ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.NumDescriptors = 3;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;
		ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));
	}

	void Ssao::InitOffsetVectors()
	{
		// Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
		// and the 6 center points along each cube face.  We always alternate the points on 
		// opposites sides of the cubes.  This way we still get the vectors spread out even
		// if we choose to use less than 14 samples.

		// 8 cube corners
		mOffsets[0] = DirectX::XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
		mOffsets[1] = DirectX::XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

		mOffsets[2] = DirectX::XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
		mOffsets[3] = DirectX::XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

		mOffsets[4] = DirectX::XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
		mOffsets[5] = DirectX::XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

		mOffsets[6] = DirectX::XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
		mOffsets[7] = DirectX::XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

		// 6 centers of cube faces
		mOffsets[8] = DirectX::XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
		mOffsets[9] = DirectX::XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

		mOffsets[10] = DirectX::XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
		mOffsets[11] = DirectX::XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

		mOffsets[12] = DirectX::XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
		mOffsets[13] = DirectX::XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

		for (int i = 0; i < 14; ++i)
		{
			// Create random lengths in [0.25, 1.0].
			float s = FRand(0.25f, 1.0f);

			DirectX::XMVECTOR v = DirectX::XMVectorScale(DirectX::XMVector4Normalize(DirectX::XMLoadFloat4(&mOffsets[i])), s);

			DirectX::XMStoreFloat4(&mOffsets[i], v);
		}
	}

	void Ssao::InitRandomVectorTexture(ID3D12GraphicsCommandList* cmdList)
	{
		D3D12_RESOURCE_DESC texDesc0;
		ZeroMemory(&texDesc0, sizeof(D3D12_RESOURCE_DESC));
		texDesc0.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc0.Alignment = 0;
		texDesc0.Width = 256;
		texDesc0.Height = 256;
		texDesc0.DepthOrArraySize = 1;
		texDesc0.MipLevels = 1;
		texDesc0.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc0.SampleDesc.Count = 1;
		texDesc0.SampleDesc.Quality = 0;
		texDesc0.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc0.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES properties0(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties0,
			D3D12_HEAP_FLAG_NONE,
			&texDesc0,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mRandomVectorMap)));

		//
		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 
		//

		const UINT num2DSubresources = texDesc0.DepthOrArraySize * texDesc0.MipLevels;
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap.Get(), 0, num2DSubresources);

		CD3DX12_HEAP_PROPERTIES properties1(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC texDesc1 = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties1,
			D3D12_HEAP_FLAG_NONE,
			&texDesc1,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mRandomVectorMapUploadBuffer.GetAddressOf())));

		DirectX::PackedVector::XMCOLOR initData[256 * 256];
		for (int i = 0; i < 256; ++i)
		{
			for (int j = 0; j < 256; ++j)
			{
				// Random vector in [0,1].  We will decompress in shader to [-1,1].
				DirectX::XMFLOAT3 v(FRand(), FRand(), FRand());

				initData[i * 256 + j] = DirectX::PackedVector::XMCOLOR(v.x, v.y, v.z, 0.0f);
			}
		}

		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = initData;
		subResourceData.RowPitch = 256 * sizeof(DirectX::PackedVector::XMCOLOR);
		subResourceData.SlicePitch = subResourceData.RowPitch * 256;

		//
		// Schedule to copy the data to the default resource, and change states.
		// Note that mCurrSol is put in the GENERIC_READ state so it can be 
		// read by a shader.
		//

		CD3DX12_RESOURCE_BARRIER barrier0 = CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(1, &barrier0);

		UpdateSubresources(cmdList, mRandomVectorMap.Get(), mRandomVectorMapUploadBuffer.Get(),
			0, 0, num2DSubresources, &subResourceData);

		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList->ResourceBarrier(1, &barrier1);
	}

	void Ssao::RebuildResources()
	{
		// Free the old resources if they exist.
		mNormalMap = nullptr;
		mAmbientMap0 = nullptr;
		mAmbientMap1 = nullptr;

		D3D12_RESOURCE_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = mRenderTargetWidth;
		texDesc.Height = mRenderTargetHeight;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = Ssao::NormalMapFormat;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


		float normalClearColor[] = { 0.0f, 0.0f, 1.0f, 0.0f };
		CD3DX12_CLEAR_VALUE optClear(NormalMapFormat, normalClearColor);
		CD3DX12_HEAP_PROPERTIES properties0(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties0,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			&optClear,
			IID_PPV_ARGS(&mNormalMap)));

		// Ambient occlusion maps are at half resolution.
		texDesc.Width = mRenderTargetWidth / 2;
		texDesc.Height = mRenderTargetHeight / 2;
		texDesc.Format = Ssao::AmbientMapFormat;

		float ambientClearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		optClear = CD3DX12_CLEAR_VALUE(AmbientMapFormat, ambientClearColor);

		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties0,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			&optClear,
			IID_PPV_ARGS(&mAmbientMap0)));

		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties0,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			&optClear,
			IID_PPV_ARGS(&mAmbientMap1)));
	}

	void Ssao::RebuildDescriptors(ID3D12Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, UINT cbvSrvUavDescriptorSize, UINT rtvDescriptorSize)
	{
		// Save references to the descriptors.  The Ssao reserves heap space
		// for 5 contiguous Srvs.
		mhAmbientMap0CpuSrv = hCpuSrv;
		mhAmbientMap1CpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
		mhNormalMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
		mhDepthMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
		mhRandomVectorMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);

		mhAmbientMap0GpuSrv = hGpuSrv;
		mhAmbientMap1GpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
		mhNormalMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
		mhDepthMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
		mhRandomVectorMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);

		mhNormalMapCpuRtv = hCpuRtv;
		mhAmbientMap0CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);
		mhAmbientMap1CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = NormalMapFormat;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		mDevice->CreateShaderResourceView(mNormalMap.Get(), &srvDesc, mhNormalMapCpuSrv);

		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		mDevice->CreateShaderResourceView(depthStencilBuffer, &srvDesc, mhDepthMapCpuSrv);

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		mDevice->CreateShaderResourceView(mRandomVectorMap.Get(), &srvDesc, mhRandomVectorMapCpuSrv);

		srvDesc.Format = AmbientMapFormat;
		mDevice->CreateShaderResourceView(mAmbientMap0.Get(), &srvDesc, mhAmbientMap0CpuSrv);
		mDevice->CreateShaderResourceView(mAmbientMap1.Get(), &srvDesc, mhAmbientMap1CpuSrv);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = NormalMapFormat;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;
		mDevice->CreateRenderTargetView(mNormalMap.Get(), &rtvDesc, mhNormalMapCpuRtv);

		rtvDesc.Format = AmbientMapFormat;
		mDevice->CreateRenderTargetView(mAmbientMap0.Get(), &rtvDesc, mhAmbientMap0CpuRtv);
		mDevice->CreateRenderTargetView(mAmbientMap1.Get(), &rtvDesc, mhAmbientMap1CpuRtv);
	}

	void Ssao::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount)
	{
		cmdList->SetPipelineState(mBlurPso);

		auto ssaoCBAddress = currFrame->SsaoCB->Resource()->GetGPUVirtualAddress();
		cmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);

		for (int i = 0; i < blurCount; ++i)
		{
			BlurAmbientMap(cmdList, true);
			BlurAmbientMap(cmdList, false);
		}
	}

	void Ssao::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur)
	{
		ID3D12Resource* output = nullptr;
		CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

		// Ping-pong the two ambient map textures as we apply
		// horizontal and vertical blur passes.
		if (horzBlur == true)
		{
			output = mAmbientMap1.Get();
			inputSrv = mhAmbientMap0GpuSrv;
			outputRtv = mhAmbientMap1CpuRtv;
			cmdList->SetGraphicsRoot32BitConstant(1, 1, 0);
		}
		else
		{
			output = mAmbientMap0.Get();
			inputSrv = mhAmbientMap1GpuSrv;
			outputRtv = mhAmbientMap0CpuRtv;
			cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);
		}

		CD3DX12_RESOURCE_BARRIER barrier0 = CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmdList->ResourceBarrier(1, &barrier0);

		float clearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		cmdList->ClearRenderTargetView(outputRtv, clearValue, 0, nullptr);

		cmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

		// Normal/depth map still bound.


		// Bind the normal and depth maps.
		cmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);

		// Bind the input ambient map to second texture table.
		cmdList->SetGraphicsRootDescriptorTable(3, inputSrv);

		// Draw fullscreen quad.
		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(6, 1, 0, 0);

		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList->ResourceBarrier(1, &barrier1);
	}
}
