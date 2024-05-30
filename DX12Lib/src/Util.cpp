#include "DX12Lib/Util.h"
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include "DX12Lib/Application.h"

namespace DX12Lib
{
	void OutputXMVECTOR(const DirectX::XMVECTOR& vector)
	{
		std::string str = std::to_string(DirectX::XMVectorGetX(vector)) + ", ";
		str += std::to_string(DirectX::XMVectorGetY(vector)) + ", ";
		str += std::to_string(DirectX::XMVectorGetZ(vector)) + ", ";
		str += std::to_string(DirectX::XMVectorGetW(vector)) + "\n";

		OutputDebugStringA(str.c_str());
	}

	void OutputXMFLOAT3(const DirectX::XMFLOAT3& floats)
	{
		std::string str = std::to_string(floats.x) + ", ";
		str += std::to_string(floats.y) + ", ";
		str += std::to_string(floats.z) + "\n";

		OutputDebugStringA(str.c_str());
	}

	Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(LPCWSTR pFileName, const D3D_SHADER_MACRO* defines, LPCSTR pEntrypoint, LPCSTR pTarget)
	{
		UINT flags = 0;
#ifdef _DEBUG
		flags |= (D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION);
#endif
		Microsoft::WRL::ComPtr<ID3DBlob> codeBlob, errorBlob;
		HRESULT hr = D3DCompileFromFile(pFileName, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, pEntrypoint, pTarget, flags, 0, &codeBlob, &errorBlob);
		
		if (errorBlob)
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		
		ThrowIfFailed(hr);
		return codeBlob;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

		CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

		// Create the actual default buffer resource.
		ThrowIfFailed(device->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&defaultBuffer)));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 
		properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		ThrowIfFailed(device->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)));


		// Describe the data we want to copy into the default buffer.
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = initData;
		subResourceData.RowPitch = byteSize;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
		// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
		// the intermediate upload heap data will be copied to mBuffer.
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(1, &barrier);

		UpdateSubresources(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
		cmdList->ResourceBarrier(1, &barrier);

		// Note: uploadBuffer has to be kept alive after the above function calls because
		// the command list has not been executed yet that performs the actual copy.
		// The caller can Release the uploadBuffer after it knows the copy has been executed.

		return defaultBuffer;
	}


	HRESULT CreateDDSTextureFromFile(_In_ ID3D12Device* device, 
		_In_ ID3D12GraphicsCommandList* cmdList, 
		_In_z_ const wchar_t* szFileName, 
		_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& texture, 
		_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap, 
		_In_ size_t maxsize, 
		_Out_opt_ DirectX::DDS_ALPHA_MODE* alphaMode)
	{
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;

		HRESULT hr = LoadDDSTextureFromFile(device, szFileName, &texture, ddsData, subresources, maxsize, alphaMode);
		if (FAILED(hr))
			return hr;

		D3D12_RESOURCE_DESC texDesc = texture->GetDesc();

		const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, num2DSubresources);

		CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

		hr = device->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadHeap));
		if (FAILED(hr))
		{
			texture = nullptr;
			return hr;
		}
		else
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			cmdList->ResourceBarrier(1, &barrier);

			// Use Heap-allocating UpdateSubresources implementation for variable number of subresources (which is the case for textures).
			UpdateSubresources(cmdList, texture.Get(), textureUploadHeap.Get(), 0, 0, num2DSubresources, subresources.data());

			barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			cmdList->ResourceBarrier(1, &barrier);

			return S_OK;
		}
	}

	DirectX::XMVECTOR GetRightFromRotationVector(const DirectX::XMVECTOR& rotation)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYawFromVector(rotation);
		return DirectX::XMVector3Transform(GetWorldRightVector(), R);
	}

	DirectX::XMVECTOR GetUpFromRotationVector(const DirectX::XMVECTOR& rotation)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYawFromVector(rotation);
		return DirectX::XMVector3Transform(GetWorldUpVector(), R);
	}

	DirectX::XMVECTOR GetForwardFromRotationVector(const DirectX::XMVECTOR& rotation)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYawFromVector(rotation);
		return DirectX::XMVector3Transform(GetWorldForwardVector(), R);
	}

	DirectX::XMVECTOR GetRightFromQuaternion(const DirectX::XMVECTOR& quaternion)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(quaternion);
		return DirectX::XMVector3Transform(GetWorldRightVector(), R);
	}

	DirectX::XMVECTOR GetUpFromQuaternion(const DirectX::XMVECTOR& quaternion)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(quaternion);
		return DirectX::XMVector3Transform(GetWorldUpVector(), R);
	}

	DirectX::XMVECTOR GetForwardFromQuaternion(const DirectX::XMVECTOR& quaternion)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(quaternion);
		return DirectX::XMVector3Transform(GetWorldForwardVector(), R);
	}

	DirectX::XMVECTOR RotationVectorToQuaternion(const DirectX::XMVECTOR& rotation)
	{
		return DirectX::XMQuaternionRotationRollPitchYawFromVector(rotation);
	}

	DirectX::XMVECTOR QuaternionToRotationVector(const DirectX::XMVECTOR& quaternion)
	{
		DirectX::XMVECTOR axis;
		float angle;
		DirectX::XMQuaternionToAxisAngle(&axis, &angle, quaternion);

		float x = DirectX::XMVectorGetX(axis);
		float y = DirectX::XMVectorGetY(axis);
		float z = DirectX::XMVectorGetZ(axis);
		float w = angle;

		float roll = atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y));
		float pitch = asin(2 * (w * y - x * z));
		float yaw = atan2(2 * (w * z + x * y), 1 - 2 * (z * z + y * y));

		return DirectX::XMVector3Normalize(DirectX::XMVectorSet(roll, pitch, yaw, 0.0f));
	}


	Texture::Texture(const std::wstring& name, const std::wstring& filename)
		: mName(name)
		, mFilename(filename)
	{
		Microsoft::WRL::ComPtr<ID3D12Device> device = Application::Get()->GetDevice();
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList = Application::Get()->GetDirectCommandList();

		ThrowIfFailed(CreateDDSTextureFromFile(device.Get(), cmdList.Get(), mFilename.c_str(), mResource, mUploadHeap));
	}
}
