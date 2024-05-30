#pragma once
#include "d3dx12.h"
#include <DirectXMath.h>
#include <wrl.h>
#include <comdef.h>
#include <string>
#include "DDSTextureLoader12.h"

#ifdef max
#undef max
#endif // max

#ifdef min
#undef min
#endif // min

#ifndef MaxLights
#define MaxLights 16
#endif

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                                       \
{                                                                              \
	HRESULT hr__ = (x);                                                        \
	std::wstring wfn = DX12Lib::AnsiToWString(__FILE__);                        \
	if (FAILED(hr__)) {  __debugbreak(); throw DX12Lib::DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#ifndef TLOG
#define TLOG(x)             OutputDebugString(x);
//#define TLOG(...)           OutputDebugString(__VA_ARGS__);
#endif

namespace DX12Lib
{
	static const int gNumFrameResources = 3;
	static const UINT CubeMapSize = 512;

	void OutputXMVECTOR(const DirectX::XMVECTOR& vector);
	void OutputXMFLOAT3(const DirectX::XMFLOAT3& floats);

	inline bool IsKeyDown(int vKey) { return GetAsyncKeyState(vKey) & 0x8000; }

	Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(LPCWSTR pFileName, const D3D_SHADER_MACRO* defines, LPCSTR pEntrypoint, LPCSTR pTarget);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	inline DirectX::XMFLOAT4X4 Identity4x4()
	{
		static DirectX::XMFLOAT4X4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);

		return I;
	}

	inline UINT CalcConstantBufferByteSize(UINT byteSize) { return (byteSize + 255) & ~255; }

	HRESULT CreateDDSTextureFromFile(_In_ ID3D12Device* device,
		_In_ ID3D12GraphicsCommandList* cmdList,
		_In_z_ const wchar_t* szFileName,
		_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
		_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap,
		_In_ size_t maxsize = 0,
		_Out_opt_ DirectX::DDS_ALPHA_MODE* alphaMode = nullptr);

	DirectX::XMVECTOR GetRightFromRotationVector(const DirectX::XMVECTOR& rotation);
	DirectX::XMVECTOR GetUpFromRotationVector(const DirectX::XMVECTOR& rotation);
	DirectX::XMVECTOR GetForwardFromRotationVector(const DirectX::XMVECTOR& rotation);

	DirectX::XMVECTOR GetRightFromQuaternion(const DirectX::XMVECTOR& quaternion);
	DirectX::XMVECTOR GetUpFromQuaternion(const DirectX::XMVECTOR& quaternion);
	DirectX::XMVECTOR GetForwardFromQuaternion(const DirectX::XMVECTOR& quaternion);

	inline DirectX::XMVECTOR GetWorldRightVector()   { return DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f); }
	inline DirectX::XMVECTOR GetWorldUpVector()      { return DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); }
	inline DirectX::XMVECTOR GetWorldForwardVector() { return DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); }

	DirectX::XMVECTOR RotationVectorToQuaternion(const DirectX::XMVECTOR& rotation);
	DirectX::XMVECTOR QuaternionToRotationVector(const DirectX::XMVECTOR& quaternion);

	inline std::wstring AnsiToWString(const std::string& str)
	{
		WCHAR buffer[512];
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
		return std::wstring(buffer);
	}

	inline float FRand(float min = 0.0f, float max = 1.0f)
	{
		return min + (float)rand() / (float)RAND_MAX * (max - min);
	}

	inline void ResourceBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(pResource, stateBefore, stateAfter);
		cmdList->ResourceBarrier(1, &barrier);
	}

	// ----------------------------------------------------------------------------------
	// 
	// ----------------------------------------------------------------------------------

	class DxException
	{
	public:
		DxException() = default;
		DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
			ErrorCode(hr),
			FunctionName(functionName),
			Filename(filename),
			LineNumber(lineNumber)
		{
		}

		std::wstring ToString() const
		{
			// Get the string description of the error code.
			_com_error err(ErrorCode);
			std::wstring msg = err.ErrorMessage();

			return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
		}

		HRESULT ErrorCode = S_OK;
		std::wstring FunctionName;
		std::wstring Filename;
		int LineNumber = -1;
	};

	class Texture
	{
	public:
		Texture(const std::wstring& name, const std::wstring& filename);

		inline const std::wstring& GetName() const { return mName; }

		inline Microsoft::WRL::ComPtr<ID3D12Resource>& GetResource() { return mResource; }
		inline Microsoft::WRL::ComPtr<ID3D12Resource>& GetUploadHeap() { return mUploadHeap; }

		UINT SrvHeapIndex = -1;
	private:
		// Unique material name for lookup.
		std::wstring mName;
		std::wstring mFilename;

		Microsoft::WRL::ComPtr<ID3D12Resource> mResource = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mUploadHeap = nullptr;
	};

	struct Material
	{
	public:
		Material(const std::wstring& name) : mName(name) {}

		std::wstring mName;

		int MatCBIndex = -1;
		int DiffuseSrvHeapIndex = -1;
		int NormalSrvHeapIndex = -1;

		int NumFramesDirty = gNumFrameResources;

		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
		float Roughness = 0.25f;
		DirectX::XMFLOAT4X4 MatTransform = Identity4x4();
	};

	struct Light
	{
	public:
		DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
		float FalloffStart = 1.0f;                          // point/spot light only
		DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
		float FalloffEnd = 10.0f;                           // point/spot light only
		DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
		float SpotPower = 64.0f;                            // spot light only
	};

	enum RenderLayer : unsigned int
	{
		Render_Layer_None = 0x0,
		Render_Layer_Opaque = 0x1,
		Render_Layer_Mirror = 0x2,
		Render_Layer_Reflected = 0x4,
		Render_Layer_Transparent = 0x8,
		Render_Layer_Shadow = 0x10,
		Render_Layer_AlphaTested = 0x20,
		Render_Layer_AlphaTestedBillboard = 0x40,
		Render_Layer_OpaqueTessellation = 0x80,
		Render_Layer_SKinnedOpaque = 0x100,
		Render_Layer_Highlight = 0x200,
		Render_Layer_OpaqueDynamicReflectors = 0x400,
		Render_Layer_Sky = 0x800,
		Render_Layer_Debug = 0x1000,

		Render_Layer_All = Render_Layer_None
		| Render_Layer_Opaque
		| Render_Layer_Mirror
		| Render_Layer_Reflected
		| Render_Layer_Transparent
		| Render_Layer_Shadow
		| Render_Layer_AlphaTested
		| Render_Layer_AlphaTestedBillboard
		| Render_Layer_OpaqueTessellation
		| Render_Layer_SKinnedOpaque
		| Render_Layer_Highlight
		| Render_Layer_OpaqueDynamicReflectors
		| Render_Layer_Sky
		| Render_Layer_Debug
	};
}
