#pragma once
#include <wrl.h>
#include <exception>
#include "Util.h"

namespace DX12Lib
{
	template<typename T>
	class UploadBuffer
	{
	public:
		UploadBuffer(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t elementCount, bool isConstantBuffer)
			: mElementCount(elementCount)
			, mIsConstantBuffer(isConstantBuffer)
		{
			mElementSizeInBytes = sizeof(T);

			if (isConstantBuffer)
				mElementSizeInBytes = CalcConstantBufferByteSize(sizeof(T));

			CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(mElementSizeInBytes * mElementCount);

			ThrowIfFailed(device->CreateCommittedResource(
				&properties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&mUploadBuffer)));

			ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mData)));
		}

		UploadBuffer(const UploadBuffer& rhs) = delete;
		UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

		~UploadBuffer()
		{
			if (mUploadBuffer)
				mUploadBuffer->Unmap(0, nullptr);

			mData = nullptr;
		}

		Microsoft::WRL::ComPtr<ID3D12Resource> Resource() const
		{
			return mUploadBuffer;
		}

		inline uint32_t GetElementSizeInBytes() const { return mElementSizeInBytes; }

		void UploadData(uint32_t offset, const T* data)
		{
			memcpy(&mData[offset * mElementSizeInBytes], data, sizeof(T));
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
		BYTE* mData = nullptr;

		uint32_t mElementCount;
		uint32_t mElementSizeInBytes;
		bool mIsConstantBuffer;
	};
}
