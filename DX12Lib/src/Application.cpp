#include "DX12Lib/Application.h"
#include <assert.h>
#include <windowsx.h>
#include "DX12Lib/Util.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DX12Lib::Application::Get()->MsgProc(hWnd, msg, wParam, lParam);
}

namespace DX12Lib
{
	Application* Application::mApp = nullptr;

	Application::Application(HINSTANCE hInstance)
		: mhInstance(hInstance)
	{
		assert(mApp == nullptr);
		mApp = this;
	}

	Application::~Application()
	{
		if (mDevice)
			FlushCommandQueue();
	}

	Application* Application::Get()
	{
		return mApp;
	}

	int Application::Run()
	{
		mTimer.Reset();

		// Message loop
		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				mTimer.Tick();
				ShowCaptionBar();
				Update(mTimer);
				Render(mTimer);
			}
		}

		return (int)msg.wParam;
	}

	bool Application::Init()
	{
		if (!InitWindow())
			return false;

		if (!InitDirect3D())
			return false;

		OnResize();

		return true;
	}

	LRESULT Application::MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_KEYUP:
			if (wParam == VK_ESCAPE)
				PostQuitMessage(0);
			else if ((int)wParam == VK_F2)
				Set4xMsaaState(!m4xMsaaState);

			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_MOUSEMOVE:
			OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
		return 0;
	}

	void Application::Set4xMsaaState(bool value)
	{
		if (m4xMsaaState != value)
		{
			m4xMsaaState = value;

			// Recreate the swapchain and buffers with new multisample settings.
			InitSwapChain();
			OnResize();
		}
	}

	void Application::ShowCaptionBar()
	{
		static int frameCnt = 0;
		static float timeElapsed = 0.0f;

		frameCnt++;

		// Compute averages over one second period.
		if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
		{
			float fps = (float)frameCnt; // fps = frameCnt / 1
			float mspf = 1000.0f / fps;

			std::wstring fpsStr = std::to_wstring(fps);
			std::wstring mspfStr = std::to_wstring(mspf);

			std::wstring windowText = mMainWndCaption +
				L"    fps: " + fpsStr +
				L"   mspf: " + mspfStr;

			SetWindowText(mhWnd, windowText.c_str());

			// Reset for next average.
			frameCnt = 0;
			timeElapsed += 1.0f;
		}
	}

	void Application::OnResize()
	{
		assert(mDevice);
		assert(mSwapChain);
		assert(mCommandList);
		assert(mDirectCommandAllocator);

		// Flush before changing any resources.
		FlushCommandQueue();

		ThrowIfFailed(mCommandList->Reset(mDirectCommandAllocator.Get(), nullptr));

		// Release the previous resources we will be recreating.
		for (UINT i = 0; i < gSwapChainBufferCount; ++i)
			mSwapChainBuffer[i].Reset();
		mDepthStencilBuffer.Reset();

		// Resize the swap chain.
		ThrowIfFailed(mSwapChain->ResizeBuffers(gSwapChainBufferCount, mClientWidth, mClientHeight, mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

		mCurrentBackBufferIndex = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < gSwapChainBufferCount; i++)
		{
			ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
			mDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
			rtvHeapHandle.Offset(1, mRtvDescriptorSize);
		}

		// Create the depth/stencil buffer and view.
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = mClientWidth;
		depthStencilDesc.Height = mClientHeight;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;

		// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
		// the depth buffer.  Therefore, because we need to create two views to the same resource:
		//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
		//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
		// we need to create the depth buffer resource with a typeless format.  
		depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

		depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = mDepthStencilFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;
		
		CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(mDevice->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(&mDepthStencilBuffer)));

		// Create descriptor to mip level 0 of entire resource using the format of the resource.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = mDepthStencilFormat;
		dsvDesc.Texture2D.MipSlice = 0;
		mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, GetDepthStencilView());

		// Transition the resource from its initial state to be used as a depth buffer.
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mCommandList->ResourceBarrier(1, &barrier);

		// Execute the resize commands.
		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		// Wait until resize is complete.
		FlushCommandQueue();

		// Update the viewport transform to cover the client area.
		mViewport = { 0.0f, 0.0f, static_cast<float>(mClientWidth), static_cast<float>(mClientHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, mClientWidth, mClientHeight };
	}

	bool Application::InitWindow()
	{
		// register window class
		{
			WNDCLASSEX wndClass = {};
			wndClass.cbSize = sizeof(WNDCLASSEX);
			wndClass.hInstance = mhInstance;
			wndClass.style = CS_HREDRAW | CS_VREDRAW;
			wndClass.lpszClassName = mMainWndCaption.c_str();
			wndClass.lpfnWndProc = WndProc;

			ATOM atom = RegisterClassEx(&wndClass);
			assert(atom > 0 && "Failed to register class");
		}
		
		// create a window
		{
			RECT windowRect = { 0, 0, static_cast<LONG>(mClientWidth), static_cast<LONG>(mClientHeight) };
			AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
			int windowWidth = windowRect.right - windowRect.left;
			int windowHeight = windowRect.bottom - windowRect.top;

			int x = std::max<int>(0, (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2);
			int y = std::max<int>(0, (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2);
			
			mhWnd = CreateWindow(mMainWndCaption.c_str(), mMainWndCaption.c_str(), WS_OVERLAPPEDWINDOW, x, y, windowWidth, windowHeight, nullptr, nullptr, mhInstance, nullptr);
			assert(mhWnd && "Failed to create window");
		}

		ShowWindow(mhWnd, SW_SHOW);

		UpdateWindow(mhWnd);
		return true;
	}

	bool Application::InitDirect3D()
	{
		UINT flag = 0;
#ifdef _DEBUG
		Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
		debugInterface->EnableDebugLayer();

		flag |= DXGI_CREATE_FACTORY_DEBUG;
#endif
		ThrowIfFailed(CreateDXGIFactory2(flag, IID_PPV_ARGS(&mdxgiFactory)));

		Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter = GetAdapter();
		ThrowIfFailed(D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice)));

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
		msQualityLevels.Format = mBackBufferFormat;
		msQualityLevels.SampleCount = 4;
		msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		msQualityLevels.NumQualityLevels = 0;
		ThrowIfFailed(mDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));

		m4xMsaaQuality = msQualityLevels.NumQualityLevels;
		assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

		InitCommandObjects();

		ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		
		InitSwapChain();

		mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		mDsvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		mCbvSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		mSamplerDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		InitRtvAndDsvDescriptorHeaps();

		return true;
	}

	void Application::InitCommandObjects()
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.NodeMask = 0;
		ThrowIfFailed(mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mCommandQueue)));
		ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectCommandAllocator)));
		ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&mCommandList)));
		ThrowIfFailed(mCommandList->Close());
	}

	void Application::InitSwapChain()
	{
		mSwapChain.Reset();

		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.Width = mClientWidth;
		desc.Height = mClientHeight;
		desc.Format = mBackBufferFormat;
		desc.Stereo = FALSE;
		desc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		desc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = gSwapChainBufferCount;
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ThrowIfFailed(mdxgiFactory->CreateSwapChainForHwnd(mCommandQueue.Get(), mhWnd, &desc, nullptr, nullptr, &mSwapChain));
	}

	void Application::InitRtvAndDsvDescriptorHeaps()
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.NumDescriptors = gSwapChainBufferCount;
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

	void Application::FlushCommandQueue()
	{
		mFenceValue++;
		ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mFenceValue));
		if (mFence->GetCompletedValue() < mFenceValue)
		{
			HANDLE eventHandle = CreateEvent(nullptr, false, false, nullptr);
			ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValue, eventHandle));
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
	}

	ID3D12Resource* Application::GetCurrentBackBuffer() const
	{
		return mSwapChainBuffer[mCurrentBackBufferIndex].Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Application::GetCurrentBackBufferView() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += mCurrentBackBufferIndex * mRtvDescriptorSize;
		return handle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Application::GetDepthStencilView() const
	{
		return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter> Application::GetAdapter()
	{
		DXGI_ADAPTER_DESC1 desc = {};
		Microsoft::WRL::ComPtr<IDXGIAdapter> tempAdapter, dxgiAdapter;
		Microsoft::WRL::ComPtr<IDXGIAdapter1> tempAdapter1;
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; mdxgiFactory->EnumAdapters(i, &tempAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			ThrowIfFailed(tempAdapter.As(&tempAdapter1));
			tempAdapter1->GetDesc1(&desc);
			if (((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) &&
				SUCCEEDED(D3D12CreateDevice(tempAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))
				&& desc.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
				dxgiAdapter = tempAdapter;
			}
		}
		return dxgiAdapter;
	}

	bool Application::IsAllowTearing()
	{
		UINT allowTearing = 0;
		ThrowIfFailed(mdxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)));
		return (allowTearing == 1);
	}
}
