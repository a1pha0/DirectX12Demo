#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <string>
#include "Timer.h"

namespace DX12Lib
{
	static const int gSwapChainBufferCount = 3;

	class Application
	{
	public:
		Application(HINSTANCE hInstance);
		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;
		~Application();

		static Application* Get();
		inline HWND GetHWnd() const { return mhWnd; }
		inline Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return mDevice; }
		inline Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetDirectCommandList() const { return mCommandList; }

		int Run();
		
		virtual bool Init();
		virtual LRESULT MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		inline float GetAspectRatio() const { return static_cast<float>(mClientWidth) / static_cast<float>(mClientHeight); }

		inline bool Get4xMsaaState() const { return m4xMsaaState; }
		void Set4xMsaaState(bool value);

	protected:
		virtual void InitRtvAndDsvDescriptorHeaps();
		virtual void Update(const Timer& timer) = 0;
		virtual void Render(const Timer& timer) = 0;
		virtual void OnResize();

		virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
		virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
		virtual void OnMouseMove(WPARAM btnState, int x, int y) {}

	protected:
		void ShowCaptionBar();

		bool InitWindow();
		bool InitDirect3D();
		void InitCommandObjects();
		void InitSwapChain();

		void FlushCommandQueue();

		ID3D12Resource* GetCurrentBackBuffer() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

		Microsoft::WRL::ComPtr<IDXGIAdapter> GetAdapter();
		bool IsAllowTearing();

	protected:
		static Application* mApp;
		HINSTANCE mhInstance = nullptr;
		HWND mhWnd = nullptr;

		// Set true to use 4X MSAA (?.1.8).  The default is false.
		bool m4xMsaaState = false;    // 4X MSAA enabled
		UINT m4xMsaaQuality = 0;      // quality level of 4X MSAA

		Microsoft::WRL::ComPtr<IDXGIFactory5> mdxgiFactory;
		Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
		Microsoft::WRL::ComPtr<IDXGISwapChain1> mSwapChain;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCommandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		UINT64 mFenceValue = 0;

		int mCurrentBackBufferIndex = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[gSwapChainBufferCount];
		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap, mDsvHeap;

		UINT mRtvDescriptorSize, mDsvDescriptorSize, mCbvSrvUavDescriptorSize, mSamplerDescriptorSize;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::wstring mMainWndCaption = L"Application";
		DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		const int mClientWidth = 1280;
		const int mClientHeight = 720;

		Timer mTimer;
	};
}
