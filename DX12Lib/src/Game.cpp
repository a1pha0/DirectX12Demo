#include "DX12Lib/Game.h"
#include <chrono>
#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <fstream>
#include <sstream>

namespace DX12Lib
{
	Game::Game(HINSTANCE hInstance)
		: Application(hInstance)
	{
		// Estimate the scene bounding sphere manually since we know how the scene was constructed.
		// The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
		// the world space origin.  In general, you need to loop over every world space vertex
		// position and compute the bounding sphere.
		mSceneBound.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		mSceneBound.Radius = sqrtf(10.0f * 10.0f + 10.0f * 10.0f);

		mLights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
		mLights[0].Strength = { 0.6f, 0.6f, 0.6f };
		mLights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
		mLights[1].Strength = { 0.3f, 0.3f, 0.3f };
		mLights[2].Direction = { 0.0f, -0.707f, -0.707f };
		mLights[2].Strength = { 0.15f, 0.15f, 0.15f };

		mCamera.LookAt(DirectX::XMVectorSet(0.0f, 5.0f, -10.f, 0.0f), DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	}

	Game::~Game()
	{
		if (mDevice)
			FlushCommandQueue();
	}

	bool Game::Init()
	{
		if (!Application::Init())
			return false;

		ThrowIfFailed(mCommandList->Reset(mDirectCommandAllocator.Get(), nullptr));

		//mBlurFilter = std::make_unique<BlurFilter>(mDevice.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
		
		mDynamicCubeMap = std::make_unique<CubeRenderTarget>(mDevice.Get(), CubeMapSize, CubeMapSize, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
		mDynamicCubeMap->InitCubeFaceCamera(0.0f, 2.0f, 0.0f);

		mShadowMap = std::make_unique<ShadowMap>(mDevice.Get(), 2048, 2048);

		InitMeshes();
		InitTextures();
		InitDescriptorHeaps();
		InitMaterials();
		InitActors();

		InitRootSignature();
		InitSsaoRootSignature();
		InitPostProcessRootSignature();
		InitPSOs();

		mSsao = std::make_unique<Ssao>(mDevice.Get(), mCommandList.Get());
		mSsao->SetPSOs(mPSOs[L"ssao"].Get(), mPSOs[L"ssaoBlur"].Get());

		InitFrameResources();

		// Execute the initialization commands.
		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		// Wait until initialization is complete.
		FlushCommandQueue();

		return true;
	}

	void Game::Update(const Timer& timer)
	{
		mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gSwapChainBufferCount;
		if (mFence->GetCompletedValue() < mFrameResources[mCurrFrameResourceIndex]->Fence)
		{
			HANDLE eventHandle = CreateEvent(nullptr, false, false, nullptr);
			ThrowIfFailed(mFence->SetEventOnCompletion(mFrameResources[mCurrFrameResourceIndex]->Fence, eventHandle));
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}

		OnInput(timer);
		Tick(timer);

		UpdateInstanceBuffer(timer);
		UpdateMaterialBuffer(timer);
		UpdateShadowTransform(timer);
		UpdateMainPassCB(timer);
		UpdateShadowPassCB(timer);
		UpdateCubeMapFacePassCBs(timer);
		UpdateSsaoPassCB(timer);
		UpdateSkinnedCBs(timer);
		//UpdateReflectedPassCB(timer);
	}

	void Game::Render(const Timer& timer)
	{
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc = mFrameResources[mCurrFrameResourceIndex]->CmdListAlloc;

		ThrowIfFailed(cmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs[L"opaque"].Get()));

		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		ID3D12DescriptorHeap* descriptorHeaps0[] = { mSrvHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps0), descriptorHeaps0);

		auto matBuffer = mFrameResources[mCurrFrameResourceIndex]->MaterialBuffer->Resource();
		CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvHeap->GetGPUDescriptorHandleForHeapStart(), mAssetManager.FindTextureIndex(L"skyCubeMap"), mCbvSrvUavDescriptorSize);

		mCommandList->SetGraphicsRootShaderResourceView(RSP_MaterialBuffer, matBuffer->GetGPUVirtualAddress());
		mCommandList->SetGraphicsRootDescriptorTable(RSP_CubeMap, skyTexDescriptor);
		mCommandList->SetGraphicsRootDescriptorTable(RSP_AllTextures, mSrvHeap->GetGPUDescriptorHandleForHeapStart());

		RenderSceneToCubeMap();
		RenderSceneToShadowMap();

		//RenderNormalsAndDepth();
		//mCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());
		//mSsao->ComputeSsao(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), 3);
		//mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		RenderSceneToBackbuffer();

		ThrowIfFailed(mCommandList->Close());

		ID3D12CommandList* const commandList = mCommandList.Get();
		mCommandQueue->ExecuteCommandLists(1, &commandList);

		ThrowIfFailed(mSwapChain->Present(0, 0));
		mCurrentBackBufferIndex = (mCurrentBackBufferIndex + 1) % gSwapChainBufferCount;

		//FlushCommandQueue();
		mFrameResources[mCurrFrameResourceIndex]->Fence = ++mFenceValue;
		ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mFenceValue));
	}

	void Game::RenderSceneToBackbuffer()
	{
		ID3D12Resource* backBuffer = GetCurrentBackBuffer();
		D3D12_CPU_DESCRIPTOR_HANDLE bbv = GetCurrentBackBufferView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();

		ID3D12DescriptorHeap* descriptorHeaps0[] = { mSrvHeap.Get() };
		ID3D12DescriptorHeap* descriptorHeaps1[] = { mDynamicCubeMap->GetSRVHeap() };
		ID3D12DescriptorHeap* descriptorHeaps2[] = { mShadowMap->GetSRVHeap() };
		CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvHeap->GetGPUDescriptorHandleForHeapStart(), mAssetManager.FindTextureIndex(L"skyCubeMap"), mCbvSrvUavDescriptorSize);

		mCommandList->RSSetViewports(1, &mViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &barrier1);

		mCommandList->ClearRenderTargetView(bbv, mClearColor, 0, nullptr);
		mCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &bbv, true, &dsv);

		auto passCB = mFrameResources[mCurrFrameResourceIndex]->PassCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(RSP_PassCB, passCB->GetGPUVirtualAddress());

		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps1), descriptorHeaps1);
		mCommandList->SetGraphicsRootDescriptorTable(RSP_CubeMap, mDynamicCubeMap->GetSRV());
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps2), descriptorHeaps2);
		mCommandList->SetGraphicsRootDescriptorTable(RSP_ShadowMap, mShadowMap->GetSRV());
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps0), descriptorHeaps0);

		mCommandList->SetPipelineState(mPSOs[L"opaque"].Get());
		RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_OpaqueDynamicReflectors));
		mCommandList->SetGraphicsRootDescriptorTable(RSP_CubeMap, skyTexDescriptor);

		RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_Opaque));

		mCommandList->SetPipelineState(mPSOs[L"skinnedOpaque"].Get());
		RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_SKinnedOpaque));

		mCommandList->SetPipelineState(mPSOs[L"debug"].Get());
		RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_Debug));

		mCommandList->SetPipelineState(mPSOs[L"sky"].Get());
		RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_Sky));

		CD3DX12_RESOURCE_BARRIER barrier3 = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCommandList->ResourceBarrier(1, &barrier3);
	}

	void Game::RenderNormalsAndDepth()
	{
		mCommandList->RSSetViewports(1, &mViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		auto normalMap = mSsao->GetNormalMap();
		auto normalMapRtv = mSsao->GetNormalMapRtv();

		// Change to RENDER_TARGET.
		CD3DX12_RESOURCE_BARRIER barrier0 = CD3DX12_RESOURCE_BARRIER::Transition(normalMap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &barrier0);

		// Clear the screen normal map and depth buffer.
		float clearValue[] = { 0.0f, 0.0f, 1.0f, 0.0f };
		auto dsv = GetDepthStencilView();
		mCommandList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);
		mCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		// Specify the buffers we are going to render to.
		mCommandList->OMSetRenderTargets(1, &normalMapRtv, true, &dsv);

		// Bind the constant buffer for this pass.
		auto passCB = mFrameResources[mCurrFrameResourceIndex]->PassCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(RSP_PassCB, passCB->GetGPUVirtualAddress());

		mCommandList->SetPipelineState(mPSOs[L"drawNormals"].Get());

		RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_Opaque));

		// Change back to GENERIC_READ so we can read the texture in a shader.
		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(normalMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		mCommandList->ResourceBarrier(1, &barrier1);
	}

	void Game::OnResize()
	{
		Application::OnResize();

		mCamera.SetPerspective(45.0f, GetAspectRatio(), 1.0f, 1000.0f);
		DirectX::BoundingFrustum::CreateFromMatrix(mCameraFrustum, mCamera.GetProjMatrix());

		if (mSsao != nullptr)
		{
			mSsao->Resize(mClientWidth, mClientHeight);
			mSsao->RebuildDescriptors(mDepthStencilBuffer.Get());
		}
	}

	void Game::OnMouseDown(WPARAM btnState, int x, int y)
	{
		if (btnState & MK_LBUTTON)
		{
			if (mPickedActor)
			{
				OutputDebugString(L"yes");
				Pick(x, y);
			}
		}
		else if (btnState & MK_RBUTTON)
		{
			SetCapture(mhWnd);
		}
	}

	void Game::OnMouseUp(WPARAM btnState, int x, int y)
	{
		ReleaseCapture();
	}

	void Game::OnMouseMove(WPARAM btnState, int x, int y)
	{

	}

	D3D12_CPU_DESCRIPTOR_HANDLE Game::GetCpuSrv(int index) const
	{
		auto srv = mSrvHeap->GetCPUDescriptorHandleForHeapStart();
		srv.ptr += index * mCbvSrvUavDescriptorSize;
		return srv;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Game::GetGpuSrv(int index) const
	{
		auto srv = mSrvHeap->GetGPUDescriptorHandleForHeapStart();
		srv.ptr += index * mCbvSrvUavDescriptorSize;
		return srv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Game::GetDsv(int index) const
	{
		auto dsv = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
		dsv.ptr += index * mDsvDescriptorSize;
		return dsv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Game::GetRtv(int index) const
	{
		auto rtv = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtv.ptr += index * mRtvDescriptorSize;
		return rtv;
	}

	void Game::InitTextures()
	{
		std::vector<std::pair<std::wstring, std::wstring>> textures =
		{
			std::make_pair(L"bricksDiffuseMap", L"assets/textures/bricks2.dds"),
			std::make_pair(L"bricksNormalMap", L"assets/textures/bricks2_nmap.dds"),
			std::make_pair(L"tileDiffuseMap", L"assets/textures/tile.dds"),
			std::make_pair(L"tileNormalMap", L"assets/textures/tile_nmap.dds"),
			std::make_pair(L"defaultDiffuseMap", L"assets/textures/white1x1.dds"),
			std::make_pair(L"defaultNormalMap", L"assets/textures/default_nmap.dds"),
			std::make_pair(L"skyCubeMap", L"assets/textures/grasscube1024.dds"),
			//std::make_pair(L"skyCubeMap", L"assets/textures/snowcube1024.dds"),
			//std::make_pair(L"skyCubeMap", L"assets/textures/desertcube1024.dds"),
		};

		// Add skinned model textures to list so we can reference by name later.
		for (UINT i = 0; i < mSkinnedMats.size(); ++i)
		{
			std::wstring diffuseName = mSkinnedMats[i].DiffuseMapName;
			std::wstring normalName = mSkinnedMats[i].NormalMapName;

			std::wstring diffuseFilename = L"assets/textures/" + diffuseName;
			std::wstring normalFilename = L"assets/textures/" + normalName;

			// strip off extension
			diffuseName = diffuseName.substr(0, diffuseName.find_last_of(L"."));
			normalName = normalName.substr(0, normalName.find_last_of(L"."));

			mSkinnedTextureNames.emplace_back(diffuseName);
			textures.emplace_back(std::make_pair(diffuseName, diffuseFilename));

			mSkinnedTextureNames.emplace_back(normalName);
			textures.emplace_back(std::make_pair(normalName, normalFilename));
		}

		for (int i = 0; i < (int)textures.size(); ++i)
		{
			mAssetManager.CreateTexture(textures[i].first, textures[i].second);
		}
	}

	void Game::InitDescriptorHeaps()
	{
		//
		// Create the SRV heap.
		//
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = mAssetManager.GetTexturesCount();
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

		//
		// Fill out the heap with actual descriptors.
		//
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor(mSrvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		auto textures = mAssetManager.GetAllTextures();
		UINT srvHeapIndex = 0;
		for (auto tex : textures)
		{
			srvDesc.Format = tex->GetResource()->GetDesc().Format;

			if (tex->GetName() != L"skyCubeMap")
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = tex->GetResource()->GetDesc().MipLevels;
				srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			}
			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MostDetailedMip = 0;
				srvDesc.TextureCube.MipLevels = tex->GetResource()->GetDesc().MipLevels;
				srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			}
			mDevice->CreateShaderResourceView(tex->GetResource().Get(), &srvDesc, hCpuDescriptor);
			hCpuDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

			tex->SrvHeapIndex = srvHeapIndex++;
		}
	}
	
	void Game::InitMaterials()
	{
		auto mat0 = mAssetManager.CreateMaterial(L"bricks0");
		mat0->DiffuseSrvHeapIndex = mAssetManager.GetTexture(L"bricksDiffuseMap")->SrvHeapIndex;
		mat0->NormalSrvHeapIndex = mAssetManager.GetTexture(L"bricksNormalMap")->SrvHeapIndex;
		mat0->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mat0->FresnelR0 = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
		mat0->Roughness = 0.3f;

		auto mat1 = mAssetManager.CreateMaterial(L"tile0");
		mat1->DiffuseSrvHeapIndex = mAssetManager.GetTexture(L"tileDiffuseMap")->SrvHeapIndex;
		mat1->NormalSrvHeapIndex = mAssetManager.GetTexture(L"tileNormalMap")->SrvHeapIndex;
		mat1->DiffuseAlbedo = DirectX::XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
		mat1->FresnelR0 = DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f);
		mat1->Roughness = 0.1f;

		auto mat2 = mAssetManager.CreateMaterial(L"mirror0");
		mat2->DiffuseSrvHeapIndex = mAssetManager.GetTexture(L"defaultDiffuseMap")->SrvHeapIndex;
		mat2->NormalSrvHeapIndex = mAssetManager.GetTexture(L"defaultNormalMap")->SrvHeapIndex;
		mat2->DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f);
		mat2->FresnelR0 = DirectX::XMFLOAT3(0.98f, 0.97f, 0.95f);
		mat2->Roughness = 0.1f;

		auto mat3 = mAssetManager.CreateMaterial(L"gray0");
		mat3->DiffuseSrvHeapIndex = mAssetManager.GetTexture(L"defaultDiffuseMap")->SrvHeapIndex;
		mat3->NormalSrvHeapIndex = mAssetManager.GetTexture(L"defaultNormalMap")->SrvHeapIndex;
		mat3->DiffuseAlbedo = DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		mat3->FresnelR0 = DirectX::XMFLOAT3(0.6f, 0.6f, 0.6f);
		mat3->Roughness = 0.2f;

		auto mat4 = mAssetManager.CreateMaterial(L"sky");
		mat4->DiffuseSrvHeapIndex = mAssetManager.GetTexture(L"skyCubeMap")->SrvHeapIndex;
		mat4->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mat4->FresnelR0 = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
		mat4->Roughness = 1.0f;

		auto mat5 = mAssetManager.CreateMaterial(L"highlight0");
		mat5->DiffuseSrvHeapIndex = mAssetManager.GetTexture(L"defaultDiffuseMap")->SrvHeapIndex;
		mat5->NormalSrvHeapIndex = mAssetManager.GetTexture(L"defaultNormalMap")->SrvHeapIndex;
		mat5->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 0.6f);
		mat5->FresnelR0 = DirectX::XMFLOAT3(0.06f, 0.06f, 0.06f);
		mat5->Roughness = 0.0f;

		for (UINT i = 0; i < mSkinnedMats.size(); ++i)
		{
			std::wstring diffuseName = mSkinnedMats[i].DiffuseMapName;
			std::wstring normalName = mSkinnedMats[i].NormalMapName;
			diffuseName = diffuseName.substr(0, diffuseName.find_last_of(L"."));
			normalName = normalName.substr(0, normalName.find_last_of(L"."));

			auto mat = mAssetManager.CreateMaterial(mSkinnedMats[i].Name);
			mat->DiffuseSrvHeapIndex = mAssetManager.GetTexture(diffuseName)->SrvHeapIndex;
			mat->NormalSrvHeapIndex = mAssetManager.GetTexture(normalName)->SrvHeapIndex;
			mat->DiffuseAlbedo = mSkinnedMats[i].DiffuseAlbedo;
			mat->FresnelR0 = mSkinnedMats[i].FresnelR0;
			mat->Roughness = mSkinnedMats[i].Roughness;
		}

		auto materials = mAssetManager.GetAllMaterials();
		UINT matCBIndex = 0;
		for (auto mat : materials)
		{
			mat->MatCBIndex = matCBIndex++;
		}
	}

	void Game::InitMeshes()
	{
		std::vector<Mesh> meshes;
		meshes.emplace_back(Mesh(L"sphere", MeshGenerator::Sphere(0.5f, 20, 20)));
		meshes.emplace_back(Mesh(L"cylinder", MeshGenerator::Cylinder(0.5f, 0.3f, 3.0f, 20, 20)));
		meshes.emplace_back(Mesh(L"grid", MeshGenerator::Grid(20.0f, 20.0f, 40, 40)));
		meshes.emplace_back(Mesh(L"box", MeshGenerator::Box(1.0f, 1.0f, 1.0f, 0)));
		meshes.emplace_back(Mesh(L"quad", MeshGenerator::Quad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f)));
		mAssetManager.CreateMeshGroup(L"default", meshes);

		InitCarMesh();
		InitSkullMesh();
		InitSkinnedMesh();
	}

	void Game::InitActors()
	{
		auto actor0 = mAssetManager.CreateActor(L"sky");
		actor0->Group = mAssetManager.GetMeshGroup(L"default");
		actor0->DrawArg = L"sphere";
		actor0->RenderLayer = Render_Layer_Sky;
		DirectX::XMStoreFloat4x4(&actor0->Instance.World, DirectX::XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
		actor0->Instance.MaterialCBIndex = mAssetManager.GetMaterial(L"sky")->MatCBIndex;
		//actor0->Hidden = true;

		auto actor2 = mAssetManager.CreateActor(L"grid");
		actor2->Group = mAssetManager.GetMeshGroup(L"default");
		actor2->DrawArg = L"grid";
		actor2->RenderLayer = Render_Layer_Opaque;
		DirectX::XMStoreFloat4x4(&actor2->Instance.TexTransform, DirectX::XMMatrixScaling(8.0f, 8.0f, 1.0f));
		actor2->Instance.MaterialCBIndex = mAssetManager.GetMaterial(L"tile0")->MatCBIndex;
		//actor2->Hidden = true;

		auto actor3 = mAssetManager.CreateActor(L"car");
		actor3->Group = mAssetManager.GetMeshGroup(L"car");
		actor3->DrawArg = L"car";
		actor3->RenderLayer = Render_Layer_Opaque;
		DirectX::XMStoreFloat4x4(&actor3->Instance.World, DirectX::XMMatrixScaling(0.2f, 0.2f, 0.2f) * DirectX::XMMatrixTranslation(3.5f, 0.5f, 0.0f));
		actor3->Instance.MaterialCBIndex = mAssetManager.GetMaterial(L"gray0")->MatCBIndex;
		//actor3->Hidden = true;

		auto actor6 = mAssetManager.CreateActor(L"box");
		actor6->Group = mAssetManager.GetMeshGroup(L"default");
		actor6->DrawArg = L"box";
		actor6->RenderLayer = Render_Layer_Opaque;
		DirectX::XMStoreFloat4x4(&actor6->Instance.World, DirectX::XMMatrixScaling(3.0f, 1.0f, 3.0f) * DirectX::XMMatrixTranslation(0.0f, 0.5f, 0.0f));
		DirectX::XMStoreFloat4x4(&actor6->Instance.TexTransform, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
		actor6->Instance.MaterialCBIndex = mAssetManager.GetMaterial(L"bricks0")->MatCBIndex;
		//actor6->Hidden = true;

		auto actor7 = mAssetManager.CreateActor(L"dynamicSphere");
		actor7->Group = mAssetManager.GetMeshGroup(L"default");
		actor7->DrawArg = L"sphere";
		actor7->RenderLayer = Render_Layer_OpaqueDynamicReflectors;
		DirectX::XMStoreFloat4x4(&actor7->Instance.World, DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f) * DirectX::XMMatrixTranslation(0.0f, 2.0f, 0.0f));
		actor7->Instance.MaterialCBIndex = mAssetManager.GetMaterial(L"mirror0")->MatCBIndex;
		//actor7->Hidden = true;

		auto actor10 = mAssetManager.CreateActor(L"debug");
		actor10->Group = mAssetManager.GetMeshGroup(L"default");
		actor10->DrawArg = L"quad";
		actor10->RenderLayer = Render_Layer_Debug;
		actor10->Instance.MaterialCBIndex = mAssetManager.GetMaterial(L"bricks0")->MatCBIndex;
		actor10->Hidden = true;

		for (UINT i = 0; i < mSkinnedMats.size(); ++i)
		{
			std::wstring submeshName = L"sm_" + std::to_wstring(i);

			auto actor = mAssetManager.CreateActor(submeshName);
			actor->Group = mAssetManager.GetMeshGroup(mSkinnedModelFilename);
			actor->DrawArg = submeshName;
			actor->RenderLayer = Render_Layer_SKinnedOpaque;
			actor->Instance.MaterialCBIndex = mAssetManager.GetMaterial(mSkinnedMats[i].Name)->MatCBIndex;
			// Reflect to change coordinate system from the RHS the data was exported out as.
			DirectX::XMMATRIX modelScale = DirectX::XMMatrixScaling(0.05f, 0.05f, -0.05f);
			DirectX::XMMATRIX modelRot = DirectX::XMMatrixRotationY(DirectX::XM_PI);
			DirectX::XMMATRIX modelOffset = DirectX::XMMatrixTranslation(0.0f, 0.0f, -6.0f);
			DirectX::XMStoreFloat4x4(&actor->Instance.World, modelScale * modelRot * modelOffset);

			// All render items for this solider.m3d instance share
			// the same skinned model instance.
			actor->SkinnedCBIndex = 0;
			actor->mSkinnedMesh = mSkinnedModelInst.get();
		}
	}

	void Game::InitRootSignature()
	{
		//D3D12_FEATURE_DATA_ROOT_SIGNATURE feature = {};
		//feature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		//if (FAILED(mDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature, sizeof(feature))))
		//{
		//	feature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		//}

		CD3DX12_DESCRIPTOR_RANGE texTable0[1];
		texTable0[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1[1];
		texTable1[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		CD3DX12_DESCRIPTOR_RANGE texTable2[1];
		texTable2[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		CD3DX12_DESCRIPTOR_RANGE texTable3[1];
		texTable3[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, mAssetManager.GetTexturesCount(), 3);

		CD3DX12_ROOT_PARAMETER params[8];
		params[0].InitAsShaderResourceView(0, 1); // Instance Buffer
		params[1].InitAsShaderResourceView(1, 1); // Material Buffer
		params[2].InitAsConstantBufferView(0);    // Main Constant Buffer
		params[3].InitAsConstantBufferView(1);    // Skinned Constant Buffer
		params[4].InitAsDescriptorTable(1, texTable0, D3D12_SHADER_VISIBILITY_PIXEL); // Cube Map
		params[5].InitAsDescriptorTable(1, texTable1, D3D12_SHADER_VISIBILITY_PIXEL); // Shadow Map
		params[6].InitAsDescriptorTable(1, texTable2, D3D12_SHADER_VISIBILITY_PIXEL); // Ssao Map
		params[7].InitAsDescriptorTable(1, texTable3, D3D12_SHADER_VISIBILITY_PIXEL); // All Textures

		auto staticSamplers = GetStaticSamplers();

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = staticSamplers.size();
		desc.pStaticSamplers = staticSamplers.data();

		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob, errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob);
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		ThrowIfFailed(hr);
		ThrowIfFailed(mDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}

	void Game::InitPSOs()
	{
		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_INPUT_ELEMENT_DESC skinnedInputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		const D3D_SHADER_MACRO fogDefines[] =
		{
			"FOG", "1",
			NULL, NULL
		};

		const D3D_SHADER_MACRO alphaTestDefines[] =
		{
			"ALPHA_TEST", "1",
			NULL, NULL
		};

		const D3D_SHADER_MACRO skinnedDefines[] =
		{
			"SKINNED", "1",
			NULL, NULL
		};

		Microsoft::WRL::ComPtr<ID3DBlob> standardVS = mAssetManager.CreateShader(L"standardVS", L"assets/shaders/Default.hlsl", nullptr, "VS", "vs_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> skinnedVS = mAssetManager.CreateShader(L"skinnedVS", L"assets/shaders/Default.hlsl", skinnedDefines, "VS", "vs_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> opaquePS = mAssetManager.CreateShader(L"opaquePS", L"assets/shaders/Default.hlsl", nullptr, "PS", "ps_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> alphaTestedPS = mAssetManager.CreateShader(L"alphaTestedPS", L"assets/shaders/Default.hlsl", alphaTestDefines, "PS", "ps_5_1");

		Microsoft::WRL::ComPtr<ID3DBlob> SkyVS = mAssetManager.CreateShader(L"SkyVS", L"assets/shaders/Sky.hlsl", nullptr, "VS", "vs_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> SkyPS = mAssetManager.CreateShader(L"SkyPS", L"assets/shaders/Sky.hlsl", nullptr, "PS", "ps_5_1");

		Microsoft::WRL::ComPtr<ID3DBlob> shadowVS = mAssetManager.CreateShader(L"shadowVS", L"assets/shaders/Shadow.hlsl", nullptr, "VS", "vs_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> shadowPS = mAssetManager.CreateShader(L"shadowPS", L"assets/shaders/Shadow.hlsl", nullptr, "PS", "ps_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> shadowAlphaTestPS = mAssetManager.CreateShader(L"shadowAlphaTestPS", L"assets/shaders/Shadow.hlsl", alphaTestDefines, "PS", "ps_5_1");

		Microsoft::WRL::ComPtr<ID3DBlob> debugVS = mAssetManager.CreateShader(L"debugVS", L"assets/shaders/ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> debugPS = mAssetManager.CreateShader(L"debugPS", L"assets/shaders/ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");

		Microsoft::WRL::ComPtr<ID3DBlob> drawNormalsVS = mAssetManager.CreateShader(L"drawNormalsVS", L"assets/shaders/DrawNormal.hlsl", nullptr, "VS", "vs_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> drawNormalsPS = mAssetManager.CreateShader(L"drawNormalsPS", L"assets/shaders/DrawNormal.hlsl", nullptr, "PS", "ps_5_1");

		Microsoft::WRL::ComPtr<ID3DBlob> ssaoVS = mAssetManager.CreateShader(L"ssaoVS", L"assets/shaders/Ssao.hlsl", nullptr, "VS", "vs_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> ssaoPS = mAssetManager.CreateShader(L"ssaoPS", L"assets/shaders/Ssao.hlsl", nullptr, "PS", "ps_5_1");

		Microsoft::WRL::ComPtr<ID3DBlob> ssaoBlurVS = mAssetManager.CreateShader(L"ssaoBlurVS", L"assets/shaders/SsaoBlur.hlsl", nullptr, "VS", "vs_5_1");
		Microsoft::WRL::ComPtr<ID3DBlob> ssaoBlurPS = mAssetManager.CreateShader(L"ssaoBlurPS", L"assets/shaders/SsaoBlur.hlsl", nullptr, "PS", "ps_5_1");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC basePsoDesc = {};
		basePsoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
		basePsoDesc.pRootSignature = mRootSignature.Get();
		basePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		basePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		basePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		basePsoDesc.SampleMask = UINT_MAX;
		basePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		basePsoDesc.NumRenderTargets = 1;
		basePsoDesc.RTVFormats[0] = mBackBufferFormat;
		basePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		basePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
		basePsoDesc.DSVFormat = mDepthStencilFormat;

		//
		// PSO for opaque objects.
		//
		D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = basePsoDesc;
		opaquePsoDesc.VS = { reinterpret_cast<BYTE*>(standardVS->GetBufferPointer()), standardVS->GetBufferSize() };
		opaquePsoDesc.PS = { reinterpret_cast<BYTE*>(opaquePS->GetBufferPointer()), opaquePS->GetBufferSize() };
		//opaquePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
		//opaquePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs[L"opaque"])));

		//
		// PSO for skinned pass.
		//
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
		skinnedOpaquePsoDesc.InputLayout = { skinnedInputLayout, _countof(skinnedInputLayout) };
		skinnedOpaquePsoDesc.VS = { reinterpret_cast<BYTE*>(skinnedVS->GetBufferPointer()), skinnedVS->GetBufferSize() };
		skinnedOpaquePsoDesc.PS = { reinterpret_cast<BYTE*>(opaquePS->GetBufferPointer()), opaquePS->GetBufferSize() };
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs[L"skinnedOpaque"])));

		//
		// PSO for sky.
		//
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = basePsoDesc;
		// The camera is inside the sky sphere, so just turn off culling.
		skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		// Make sure the depth function is LESS_EQUAL and not just LESS.  
		// Otherwise, the normalized depth values at z = 1 (NDC) will 
		// fail the depth test if the depth buffer was cleared to 1.
		skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		skyPsoDesc.VS = { reinterpret_cast<BYTE*>(SkyVS->GetBufferPointer()), SkyVS->GetBufferSize() };
		skyPsoDesc.PS = { reinterpret_cast<BYTE*>(SkyPS->GetBufferPointer()), SkyPS->GetBufferSize() };
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs[L"sky"])));

		//
		// PSO for shadow map pass.
		//
		D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = basePsoDesc;
		smapPsoDesc.RasterizerState.DepthBias = 100000;
		smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
		smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
		smapPsoDesc.VS = { reinterpret_cast<BYTE*>(shadowVS->GetBufferPointer()), shadowVS->GetBufferSize() };
		smapPsoDesc.PS = { reinterpret_cast<BYTE*>(shadowPS->GetBufferPointer()), shadowPS->GetBufferSize() };
		// Shadow map pass does not have a render target.
		smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		smapPsoDesc.NumRenderTargets = 0;
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs[L"shadow"])));

		//
		// PSO for debug layer.
		//
		D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = basePsoDesc;
		debugPsoDesc.VS = { reinterpret_cast<BYTE*>(debugVS->GetBufferPointer()), debugVS->GetBufferSize() };
		debugPsoDesc.PS = { reinterpret_cast<BYTE*>(debugPS->GetBufferPointer()), debugPS->GetBufferSize() };
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs[L"debug"])));

		//
		// PSO for drawing normals.
		//
		D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = basePsoDesc;
		drawNormalsPsoDesc.VS = { reinterpret_cast<BYTE*>(drawNormalsVS->GetBufferPointer()), drawNormalsVS->GetBufferSize() };
		drawNormalsPsoDesc.PS = { reinterpret_cast<BYTE*>(drawNormalsPS->GetBufferPointer()), drawNormalsPS->GetBufferSize() };
		drawNormalsPsoDesc.RTVFormats[0] = Ssao::NormalMapFormat;
		drawNormalsPsoDesc.SampleDesc.Count = 1;
		drawNormalsPsoDesc.SampleDesc.Quality = 0;
		drawNormalsPsoDesc.DSVFormat = mDepthStencilFormat;
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs[L"drawNormals"])));

		//
		// PSO for SSAO.
		//
		D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = basePsoDesc;
		ssaoPsoDesc.InputLayout = { nullptr, 0 };
		ssaoPsoDesc.pRootSignature = mSsaoRootSignature.Get();
		ssaoPsoDesc.VS = { reinterpret_cast<BYTE*>(ssaoVS->GetBufferPointer()), ssaoVS->GetBufferSize() };
		ssaoPsoDesc.PS = { reinterpret_cast<BYTE*>(ssaoPS->GetBufferPointer()), ssaoPS->GetBufferSize() };
		// SSAO effect does not need the depth buffer.
		ssaoPsoDesc.DepthStencilState.DepthEnable = false;
		ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		ssaoPsoDesc.RTVFormats[0] = Ssao::AmbientMapFormat;
		ssaoPsoDesc.SampleDesc.Count = 1;
		ssaoPsoDesc.SampleDesc.Quality = 0;
		ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs[L"ssao"])));

		//
		// PSO for SSAO blur.
		//
		D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = ssaoPsoDesc;
		ssaoBlurPsoDesc.VS = { reinterpret_cast<BYTE*>(ssaoBlurVS->GetBufferPointer()), ssaoBlurVS->GetBufferSize() };
		ssaoBlurPsoDesc.PS = { reinterpret_cast<BYTE*>(ssaoBlurPS->GetBufferPointer()), ssaoBlurPS->GetBufferSize() };
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs[L"ssaoBlur"])));
	}

	void Game::InitFrameResources()
	{
		for (int i = 0; i < gNumFrameResources; ++i)
		{
			mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(), 2 + 6, mAssetManager.GetActorsCount(), 1, mAssetManager.GetMaterialsCount()));
		}
	}

	void Game::InitSkullMesh()
	{
		std::ifstream fin("assets/models/skull.txt");

		if (!fin)
		{
			MessageBox(0, L"assets/models/skull.txt not found.", 0, 0);
			return;
		}

		UINT vcount = 0;
		UINT tcount = 0;
		std::string ignore;

		fin >> ignore >> vcount;
		fin >> ignore >> tcount;
		fin >> ignore >> ignore >> ignore >> ignore;

		MeshData meshdata;
		meshdata.Vertices.resize(vcount);
		meshdata.Indices32.resize(tcount * 3);

		for (UINT i = 0; i < vcount; ++i)
		{
			fin >> meshdata.Vertices[i].Position.x >> meshdata.Vertices[i].Position.y >> meshdata.Vertices[i].Position.z;
			fin >> meshdata.Vertices[i].Normal.x >> meshdata.Vertices[i].Normal.y >> meshdata.Vertices[i].Normal.z;

			DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&meshdata.Vertices[i].Position);

			// Project point onto unit sphere and generate spherical texture coordinates.
			DirectX::XMFLOAT3 spherePos;
			DirectX::XMStoreFloat3(&spherePos, DirectX::XMVector3Normalize(P));

			float theta = atan2f(spherePos.z, spherePos.x);

			// Put in [0, 2pi].
			if (theta < 0.0f)
				theta += DirectX::XM_2PI;

			float phi = acosf(spherePos.y);

			float u = theta / DirectX::XM_2PI;
			float v = phi / DirectX::XM_PI;

			meshdata.Vertices[i].TexCoord = { u, v };
		}

		fin >> ignore >> ignore >> ignore;

		for (UINT i = 0; i < tcount; ++i)
		{
			fin >> meshdata.Indices32[i * 3 + 0] >> meshdata.Indices32[i * 3 + 1] >> meshdata.Indices32[i * 3 + 2];
		}

		fin.close();

		std::vector<Mesh> meshes;
		meshes.emplace_back(std::move(mAssetManager.CreateMesh(L"skull", meshdata)));
		mAssetManager.CreateMeshGroup(L"skull", meshes);
	}

	void Game::InitCarMesh()
	{
		std::ifstream fin("assets/models/car.txt");

		if (!fin)
		{
			MessageBox(0, L"assets/models/car.txt not found.", 0, 0);
			return;
		}

		UINT vcount = 0;
		UINT tcount = 0;
		std::string ignore;

		fin >> ignore >> vcount;
		fin >> ignore >> tcount;
		fin >> ignore >> ignore >> ignore >> ignore;

		MeshData meshdata;
		meshdata.Vertices.resize(vcount);
		meshdata.Indices32.resize(tcount * 3);

		for (UINT i = 0; i < vcount; ++i)
		{
			fin >> meshdata.Vertices[i].Position.x >> meshdata.Vertices[i].Position.y >> meshdata.Vertices[i].Position.z;
			fin >> meshdata.Vertices[i].Normal.x >> meshdata.Vertices[i].Normal.y >> meshdata.Vertices[i].Normal.z;

			DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&meshdata.Vertices[i].Position);

			// Project point onto unit sphere and generate spherical texture coordinates.
			DirectX::XMFLOAT3 spherePos;
			DirectX::XMStoreFloat3(&spherePos, DirectX::XMVector3Normalize(P));

			float theta = atan2f(spherePos.z, spherePos.x);

			// Put in [0, 2pi].
			if (theta < 0.0f)
				theta += DirectX::XM_2PI;

			float phi = acosf(spherePos.y);

			float u = theta / DirectX::XM_2PI;
			float v = phi / DirectX::XM_PI;

			meshdata.Vertices[i].TexCoord = { u, v };
		}

		fin >> ignore >> ignore >> ignore;

		for (UINT i = 0; i < tcount; ++i)
		{
			fin >> meshdata.Indices32[i * 3 + 0] >> meshdata.Indices32[i * 3 + 1] >> meshdata.Indices32[i * 3 + 2];
		}

		fin.close();

		std::vector<Mesh> meshes;
		meshes.emplace_back(std::move(mAssetManager.CreateMesh(L"car", meshdata)));
		mAssetManager.CreateMeshGroup(L"car", meshes);
	}

	void Game::InitSkinnedMesh()
	{
		std::vector<SkinnedVertex> vertices;
		std::vector<std::uint16_t> indices;
		std::wstring skinnedModelFilename = L"assets/models/soldier.m3d";

		M3DLoader m3dLoader;
		m3dLoader.LoadM3d(skinnedModelFilename, vertices, indices, mSkinnedSubsets, mSkinnedMats, mSkinnedInfo);

		mSkinnedModelInst = std::make_unique<SkinnedMesh>();
		mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
		mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.GetBoneCount());
		mSkinnedModelInst->AnimationClipName = L"Take1";
		mSkinnedModelInst->TimePos = 0.0f;

		const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto geo = std::make_unique<MeshGroup>(skinnedModelFilename);

		ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
		CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

		ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
		CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		geo->VertexBufferGPU = CreateDefaultBuffer(mDevice.Get(),
			mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

		geo->IndexBufferGPU = CreateDefaultBuffer(mDevice.Get(),
			mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

		geo->VertexByteStride = sizeof(SkinnedVertex);
		geo->VertexBufferByteSize = vbByteSize;
		geo->IndexFormat = DXGI_FORMAT_R16_UINT;
		geo->IndexBufferByteSize = ibByteSize;

		for (UINT i = 0; i < (UINT)mSkinnedSubsets.size(); ++i)
		{
			Submesh submesh;
			std::wstring name = L"sm_" + std::to_wstring(i);

			submesh.IndexCount = (UINT)mSkinnedSubsets[i].FaceCount * 3;
			submesh.StartIndexLocation = mSkinnedSubsets[i].FaceStart * 3;
			submesh.BaseVertexLocation = 0;

			geo->DrawArgs[name] = submesh;
		}

		mAssetManager.CreateMeshGroup(geo);
	}

	void Game::InitPostProcessRootSignature()
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable, uavTable;
		srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER params[3];
		params[0].InitAsConstants(12, 0);
		params[1].InitAsDescriptorTable(1, &srvTable);
		params[2].InitAsDescriptorTable(1, &uavTable);

		CD3DX12_ROOT_SIGNATURE_DESC desc(3, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob, errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		ThrowIfFailed(hr);
		ThrowIfFailed(mDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&mPostProcessRootSignature)));
	}

	void Game::InitSsaoRootSignature()
	{
		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[4];

		// Perfomance TIP: Order from most frequent to least frequent.
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstants(1, 1);
		slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

		const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
			0, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
			1, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
			2, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
			0.0f,
			0,
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

		const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
			3, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

		std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers =
		{
			pointClamp, linearClamp, depthMapSam, linearWrap
		};

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob, errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob);
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		ThrowIfFailed(hr);
		ThrowIfFailed(mDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&mSsaoRootSignature)));
	}

	void Game::OnInput(const Timer& timer)
	{
		POINT mousePos;
		GetCursorPos(&mousePos);

		bool RB = IsKeyDown(VK_RBUTTON);
		bool LB = IsKeyDown(VK_LBUTTON);
		bool LShift = IsKeyDown(VK_LSHIFT);
		bool W = IsKeyDown('W');
		bool S = IsKeyDown('S');
		bool A = IsKeyDown('A');
		bool D = IsKeyDown('D');
		bool E = IsKeyDown('E');
		bool Q = IsKeyDown('Q');

		if (RB)
		{
			// @fixme : why moving so fast ?
			float distance = (LShift ? 2.0f : 8.0) * timer.DeltaTime();
			if (W) mCamera.MoveForward(distance);
			if (S) mCamera.MoveForward(-distance);
			if (A) mCamera.MoveRight(-distance);
			if (D) mCamera.MoveRight(distance);
			if (E) mCamera.MoveUp(distance);
			if (Q) mCamera.MoveUp(-distance);

			long dx = mousePos.x - mMousePos.x;
			long dy = mousePos.y - mMousePos.y;
			if (dx) mCamera.RotateY(dx * 0.8 * timer.DeltaTime());
			if (dy) mCamera.RotateX(dy * 0.8 * timer.DeltaTime());
		}

		mMousePos = mousePos;

		mCamera.UpdateViewMatrix();
	}

	void Game::Tick(const Timer& timer)
	{
		DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(0.2f, 0.2f, 0.2f);
		DirectX::XMMATRIX offset = DirectX::XMMatrixTranslation(3.5f, 0.5f, 0.0f);
		DirectX::XMMATRIX localRotate = DirectX::XMMatrixRotationY(2.0f * timer.TotalTime());
		DirectX::XMMATRIX globalRotate = DirectX::XMMatrixRotationY(0.5f * timer.TotalTime());
		//DirectX::XMStoreFloat4x4(&mAssetManager.GetActor(L"car")->Instance.World, scale * localRotate * offset * globalRotate);
		DirectX::XMStoreFloat4x4(&mAssetManager.GetActor(L"car")->Instance.World, scale * offset * globalRotate);

		DirectX::XMMATRIX R = DirectX::XMMatrixRotationY(0.1f * timer.DeltaTime());
		for (int i = 0; i < 3; ++i)
		{
			DirectX::XMVECTOR lightDir = DirectX::XMLoadFloat3(&mLights[i].Direction);
			lightDir = DirectX::XMVector3TransformNormal(lightDir, R);
			DirectX::XMStoreFloat3(&mLights[i].Direction, lightDir);
		}
	}

	void Game::UpdateInstanceBuffer(const Timer& timer)
	{
		auto currInstanceBuffer = mFrameResources[mCurrFrameResourceIndex]->InstanceBuffer.get();
		DirectX::XMMATRIX view = mCamera.GetViewMatrix();
		DirectX::XMVECTOR d = DirectX::XMMatrixDeterminant(view);
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&d, view);

		DirectX::BoundingFrustum worldCamearFrustum;
		mCameraFrustum.Transform(worldCamearFrustum, invView);

		auto actors = mAssetManager.GetActors(Render_Layer_All);
		UINT instanceOffset = 0;
		for (auto a : actors)
		{
			a->Visible = false;

			if (a->Hidden || a->Group == nullptr)
				continue;

			DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&a->Instance.World);

			DirectX::BoundingBox worldBoundingBox;
			a->Group->DrawArgs[a->DrawArg].Bound.Transform(worldBoundingBox, world);
			
			if (worldCamearFrustum.Contains(worldBoundingBox) != DirectX::DISJOINT)
			{
				a->Visible = true;

				InstanceData data;
				DirectX::XMStoreFloat4x4(&data.World, DirectX::XMMatrixTranspose(world));
				DirectX::XMMATRIX texTransform = DirectX::XMLoadFloat4x4(&a->Instance.TexTransform);
				DirectX::XMStoreFloat4x4(&data.TexTransform, DirectX::XMMatrixTranspose(texTransform));
				data.MaterialCBIndex = a->Instance.MaterialCBIndex;

				a->InstanceBufferOffset = instanceOffset++;
				currInstanceBuffer->UploadData(a->InstanceBufferOffset, &data);
			}
			else
			{
				OutputDebugString(L"[");
				OutputDebugString(std::to_wstring(timer.TotalTime()).c_str());
				OutputDebugString(L"]\t");
				OutputDebugString(a->Name.c_str());
				OutputDebugString(L"\n");
			}
		}

		std::wostringstream outs;
		outs.precision(6);
		outs << L"DX12Lib" << L"    " << instanceOffset << L" actors visible out of " << mAssetManager.GetActorsCount();
		mMainWndCaption = outs.str();
	}

	void Game::UpdateMaterialBuffer(const Timer& timer)
	{
		auto currMatBuffer = mFrameResources[mCurrFrameResourceIndex]->MaterialBuffer.get();
		auto materials = mAssetManager.GetAllMaterials();

		for (auto mat : materials)
		{
			// Only update the cbuffer data if the constants have changed.  If the cbuffer
			// data changes, it needs to be updated for each FrameResource.
			if (mat->NumFramesDirty > 0)
			{
				MaterialData matData;
				matData.DiffuseAlbedo = mat->DiffuseAlbedo;
				matData.FresnelR0 = mat->FresnelR0;
				matData.Roughness = mat->Roughness;
				matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
				matData.NormalMapIndex = mat->NormalSrvHeapIndex;
				DirectX::XMMATRIX matTransform = DirectX::XMLoadFloat4x4(&mat->MatTransform);
				DirectX::XMStoreFloat4x4(&matData.MatTransform, DirectX::XMMatrixTranspose(matTransform));

				currMatBuffer->UploadData(mat->MatCBIndex, &matData);

				// Next FrameResource need to be updated too.
				mat->NumFramesDirty--;
			}
		}
	}

	void Game::UpdateShadowTransform(const Timer& timer)
	{
		// Only the first "main" light casts a shadow.
		DirectX::XMVECTOR lightDir = DirectX::XMLoadFloat3(&mLights[0].Direction);
		DirectX::XMVECTOR lightPos = DirectX::XMVectorScale(lightDir, - 2.0f * mSceneBound.Radius);
		DirectX::XMVECTOR targetPos = DirectX::XMLoadFloat3(&mSceneBound.Center);
		DirectX::XMVECTOR lightUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(lightPos, targetPos, lightUp);

		DirectX::XMStoreFloat3(&mLightPosW, lightPos);

		// Transform bounding sphere to light space.
		DirectX::XMFLOAT3 sphereCenterLS;
		DirectX::XMStoreFloat3(&sphereCenterLS, DirectX::XMVector3TransformCoord(targetPos, lightView));

		// Ortho frustum in light space encloses scene.
		float l = sphereCenterLS.x - mSceneBound.Radius;
		float b = sphereCenterLS.y - mSceneBound.Radius;
		float n = sphereCenterLS.z - mSceneBound.Radius;
		float r = sphereCenterLS.x + mSceneBound.Radius;
		float t = sphereCenterLS.y + mSceneBound.Radius;
		float f = sphereCenterLS.z + mSceneBound.Radius;

		mLightNearZ = n;
		mLightFarZ = f;
		DirectX::XMMATRIX lightProj = DirectX::XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		DirectX::XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		DirectX::XMMATRIX S = lightView * lightProj * T;
		DirectX::XMStoreFloat4x4(&mLightView, lightView);
		DirectX::XMStoreFloat4x4(&mLightProj, lightProj);
		DirectX::XMStoreFloat4x4(&mShadowTransform, S);
	}

	void Game::UpdateMainPassCB(const Timer& timer)
	{
		DirectX::XMMATRIX view = mCamera.GetViewMatrix();
		DirectX::XMMATRIX proj = mCamera.GetProjMatrix();
		DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
		
		DirectX::XMVECTOR viewDeterminant = DirectX::XMMatrixDeterminant(view);
		DirectX::XMVECTOR projDeterminant = DirectX::XMMatrixDeterminant(proj);
		DirectX::XMVECTOR viewProjDeterminant = DirectX::XMMatrixDeterminant(viewProj);

		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&viewDeterminant, view);
		DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&projDeterminant, proj);
		DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&viewProjDeterminant, viewProj);

		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		DirectX::XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);
		DirectX::XMMATRIX viewProjTex = DirectX::XMMatrixMultiply(viewProj, T);

		DirectX::XMMATRIX shadowTransform = DirectX::XMLoadFloat4x4(&mShadowTransform);

		DirectX::XMStoreFloat4x4(&mMainPassCB.View, DirectX::XMMatrixTranspose(view));
		DirectX::XMStoreFloat4x4(&mMainPassCB.InvView, DirectX::XMMatrixTranspose(invView));
		DirectX::XMStoreFloat4x4(&mMainPassCB.Proj, DirectX::XMMatrixTranspose(proj));
		DirectX::XMStoreFloat4x4(&mMainPassCB.InvProj, DirectX::XMMatrixTranspose(invProj));
		DirectX::XMStoreFloat4x4(&mMainPassCB.ViewProj, DirectX::XMMatrixTranspose(viewProj));
		DirectX::XMStoreFloat4x4(&mMainPassCB.InvViewProj, DirectX::XMMatrixTranspose(invViewProj));
		DirectX::XMStoreFloat4x4(&mMainPassCB.ViewProjTex, DirectX::XMMatrixTranspose(invViewProj));
		DirectX::XMStoreFloat4x4(&mMainPassCB.ShadowTransform, DirectX::XMMatrixTranspose(shadowTransform));
		
		mMainPassCB.EyePosW = mCamera.GetPosition3f();
		mMainPassCB.RenderTargetSize = DirectX::XMFLOAT2((float)mClientWidth, (float)mClientHeight);
		mMainPassCB.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
		mMainPassCB.NearZ = 1.0f;
		mMainPassCB.FarZ = 1000.0f;
		mMainPassCB.TotalTime = timer.TotalTime();
		mMainPassCB.DeltaTime = timer.DeltaTime();
		mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };
		mMainPassCB.Lights[0] = mLights[0];
		mMainPassCB.Lights[1] = mLights[1];
		mMainPassCB.Lights[2] = mLights[2];

		auto currPassCB = mFrameResources[mCurrFrameResourceIndex]->PassCB.get();
		currPassCB->UploadData(0, &mMainPassCB);
	}

	void Game::UpdateShadowPassCB(const Timer& timer)
	{
		DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&mLightView);
		DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mLightProj);
		DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);

		DirectX::XMVECTOR viewDeterminant = DirectX::XMMatrixDeterminant(view);
		DirectX::XMVECTOR projDeterminant = DirectX::XMMatrixDeterminant(proj);
		DirectX::XMVECTOR viewProjDeterminant = DirectX::XMMatrixDeterminant(viewProj);

		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&viewDeterminant, view);
		DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&projDeterminant, proj);
		DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&viewProjDeterminant, viewProj);

		DirectX::XMStoreFloat4x4(&mShadowPassCB.View, DirectX::XMMatrixTranspose(view));
		DirectX::XMStoreFloat4x4(&mShadowPassCB.InvView, DirectX::XMMatrixTranspose(invView));
		DirectX::XMStoreFloat4x4(&mShadowPassCB.Proj, DirectX::XMMatrixTranspose(proj));
		DirectX::XMStoreFloat4x4(&mShadowPassCB.InvProj, DirectX::XMMatrixTranspose(invProj));
		DirectX::XMStoreFloat4x4(&mShadowPassCB.ViewProj, DirectX::XMMatrixTranspose(viewProj));
		DirectX::XMStoreFloat4x4(&mShadowPassCB.InvViewProj, DirectX::XMMatrixTranspose(invViewProj));
	
		mShadowPassCB.EyePosW = mLightPosW;
		mShadowPassCB.RenderTargetSize = DirectX::XMFLOAT2((float)mShadowMap->GetWidth(), (float)mShadowMap->GetHeight());
		mShadowPassCB.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / (float)mShadowMap->GetWidth(), 1.0f / (float)mShadowMap->GetHeight());
		mShadowPassCB.NearZ = mLightNearZ;
		mShadowPassCB.FarZ = mLightFarZ;

		auto currPassCB = mFrameResources[mCurrFrameResourceIndex]->PassCB.get();
		currPassCB->UploadData(1, &mShadowPassCB);
	}

	void Game::UpdateCubeMapFacePassCBs(const Timer& timer)
	{
		auto currPassCB = mFrameResources[mCurrFrameResourceIndex]->PassCB.get();
		
		for (int i = 0; i < 6; ++i)
		{
			PassConstant cubeFacePassCB = mMainPassCB;
			DirectX::XMMATRIX view = mDynamicCubeMap->GetCamera(i)->GetViewMatrix();
			DirectX::XMMATRIX proj = mDynamicCubeMap->GetCamera(i)->GetProjMatrix();
			DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);

			DirectX::XMVECTOR viewDeterminant = DirectX::XMMatrixDeterminant(view);
			DirectX::XMVECTOR projDeterminant = DirectX::XMMatrixDeterminant(proj);
			DirectX::XMVECTOR viewProjDeterminant = DirectX::XMMatrixDeterminant(viewProj);

			DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&viewDeterminant, view);
			DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&projDeterminant, proj);
			DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&viewProjDeterminant, viewProj);

			DirectX::XMStoreFloat4x4(&cubeFacePassCB.View, DirectX::XMMatrixTranspose(view));
			DirectX::XMStoreFloat4x4(&cubeFacePassCB.InvView, DirectX::XMMatrixTranspose(invView));
			DirectX::XMStoreFloat4x4(&cubeFacePassCB.Proj, DirectX::XMMatrixTranspose(proj));
			DirectX::XMStoreFloat4x4(&cubeFacePassCB.InvProj, DirectX::XMMatrixTranspose(invProj));
			DirectX::XMStoreFloat4x4(&cubeFacePassCB.ViewProj, DirectX::XMMatrixTranspose(viewProj));
			DirectX::XMStoreFloat4x4(&cubeFacePassCB.InvViewProj, DirectX::XMMatrixTranspose(invViewProj));

			cubeFacePassCB.EyePosW = mDynamicCubeMap->GetCamera(i)->GetPosition3f();
			cubeFacePassCB.RenderTargetSize = DirectX::XMFLOAT2((float)CubeMapSize, (float)CubeMapSize);
			cubeFacePassCB.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / CubeMapSize, 1.0f / CubeMapSize);
			cubeFacePassCB.NearZ = 1.0f;
			cubeFacePassCB.FarZ = 1000.0f;
			cubeFacePassCB.TotalTime = timer.TotalTime();
			cubeFacePassCB.DeltaTime = timer.DeltaTime();
			cubeFacePassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
			cubeFacePassCB.Lights[0] = mLights[0];
			cubeFacePassCB.Lights[1] = mLights[1];
			cubeFacePassCB.Lights[2] = mLights[2];

			currPassCB->UploadData(2 + i, &cubeFacePassCB);
		}
	}

	void Game::UpdateReflectedPassCB(const Timer& timer)
	{
		mReflectedPassCB = mMainPassCB;

		DirectX::XMVECTOR mirrorPlane = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
		DirectX::XMMATRIX R = DirectX::XMMatrixReflect(mirrorPlane);

		// Reflect the lighting.
		for (int i = 0; i < 3; ++i)
		{
			DirectX::XMVECTOR lightDir = DirectX::XMLoadFloat3(&mMainPassCB.Lights[i].Direction);
			DirectX::XMVECTOR reflectedLightDir = DirectX::XMVector3TransformNormal(lightDir, R);
			DirectX::XMStoreFloat3(&mReflectedPassCB.Lights[i].Direction, reflectedLightDir);
		}

		// Reflected pass stored in index 1
		auto currPassCB = mFrameResources[mCurrFrameResourceIndex]->PassCB.get();
		currPassCB->UploadData(1, &mReflectedPassCB);
	}

	void Game::UpdateSsaoPassCB(const Timer& timer)
	{
		SsaoConstant ssaoCB;

		DirectX::XMMATRIX P = mCamera.GetProjMatrix();

		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		DirectX::XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		ssaoCB.Proj = mMainPassCB.Proj;
		ssaoCB.InvProj = mMainPassCB.InvProj;
		DirectX::XMStoreFloat4x4(&ssaoCB.ProjTex, DirectX::XMMatrixTranspose(P * T));

		mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

		auto blurWeights = mSsao->CalcGaussWeights(2.5f);
		ssaoCB.BlurWeights[0] = DirectX::XMFLOAT4(&blurWeights[0]);
		ssaoCB.BlurWeights[1] = DirectX::XMFLOAT4(&blurWeights[4]);
		ssaoCB.BlurWeights[2] = DirectX::XMFLOAT4(&blurWeights[8]);

		ssaoCB.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / mSsao->GetSsaoMapWidth(), 1.0f / mSsao->GetSsaoMapHeight());

		// Coordinates given in view space.
		ssaoCB.OcclusionRadius = 0.5f;
		ssaoCB.OcclusionFadeStart = 0.2f;
		ssaoCB.OcclusionFadeEnd = 1.0f;
		ssaoCB.SurfaceEpsilon = 0.05f;

		auto currSsaoCB = mFrameResources[mCurrFrameResourceIndex]->SsaoCB.get();
		currSsaoCB->UploadData(0, &ssaoCB);
	}

	void Game::UpdateSkinnedCBs(const Timer& timer)
	{
		auto currSkinnedCB = mFrameResources[mCurrFrameResourceIndex]->SkinnedCB.get();

		// We only have one skinned model being animated.
		mSkinnedModelInst->UpdateSkinnedAnimation(timer.DeltaTime());

		SkinnedConstant skinnedConstant;
		std::copy(
			std::begin(mSkinnedModelInst->FinalTransforms),
			std::end(mSkinnedModelInst->FinalTransforms),
			&skinnedConstant.BoneTransforms[0]);

		currSkinnedCB->UploadData(0, &skinnedConstant);
	}

	void Game::RenderActors(ID3D12GraphicsCommandList* cmdList, const FrameResource* frameResource, const std::vector<Actor*>& actors)
	{
		auto elementSizeInBytes = frameResource->InstanceBuffer->GetElementSizeInBytes();
		auto instanceBuffer = frameResource->InstanceBuffer->Resource();
		auto skinnedCB = frameResource->SkinnedCB->Resource();
		UINT skinnedCBByteSize = CalcConstantBufferByteSize(sizeof(SkinnedConstant));

		for (auto actor : actors)
		{
			if (actor->Visible == false)
				continue;

			D3D12_VERTEX_BUFFER_VIEW vbv = actor->Group->VertexBufferView();
			D3D12_INDEX_BUFFER_VIEW ibv = actor->Group->IndexBufferView();

			cmdList->IASetVertexBuffers(0, 1, &vbv);
			cmdList->IASetIndexBuffer(&ibv);
			cmdList->IASetPrimitiveTopology(actor->PrimitiveType);

			D3D12_GPU_VIRTUAL_ADDRESS address = instanceBuffer->GetGPUVirtualAddress();
			address += actor->InstanceBufferOffset * elementSizeInBytes;

			cmdList->SetGraphicsRootShaderResourceView(RSP_InstanceBuffer, address);

			if (actor->mSkinnedMesh != nullptr)
			{
				D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + actor->SkinnedCBIndex * skinnedCBByteSize;
				cmdList->SetGraphicsRootConstantBufferView(RSP_SkinnedCB, skinnedCBAddress);
			}
			else
			{
				cmdList->SetGraphicsRootConstantBufferView(RSP_SkinnedCB, 0);
			}

			Submesh submesh = actor->Group->DrawArgs[actor->DrawArg];
			cmdList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation, submesh.BaseVertexLocation, 0);
		}
	}

	void Game::RenderSceneToCubeMap()
	{
		D3D12_VIEWPORT viewport = mDynamicCubeMap->GetViewport();
		D3D12_RECT scissorRect = mDynamicCubeMap->GetScissorRect();

		mCommandList->RSSetViewports(1, &viewport);
		mCommandList->RSSetScissorRects(1, &scissorRect);

		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(mDynamicCubeMap->GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &barrier1);

		auto passCB = mFrameResources[mCurrFrameResourceIndex]->PassCB->Resource();
		UINT passCBByteSizes = CalcConstantBufferByteSize(sizeof(PassConstant));

		for (int i = 0; i < 6; ++i)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cubeRtvHandle = mDynamicCubeMap->GetRTV(i);
			D3D12_CPU_DESCRIPTOR_HANDLE cubeDsvHandle = mDynamicCubeMap->GetDSV();

			mCommandList->ClearRenderTargetView(cubeRtvHandle, mClearColor, 0, nullptr);
			mCommandList->ClearDepthStencilView(cubeDsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
			mCommandList->OMSetRenderTargets(1, &cubeRtvHandle, true, &cubeDsvHandle);

			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + (2 + i) * passCBByteSizes;
			mCommandList->SetGraphicsRootConstantBufferView(RSP_PassCB, passCBAddress);

			RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_Opaque | Render_Layer_SKinnedOpaque));

			mCommandList->SetPipelineState(mPSOs[L"sky"].Get());
			RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_Sky));

			mCommandList->SetPipelineState(mPSOs[L"opaque"].Get());
		}

		CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(mDynamicCubeMap->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		mCommandList->ResourceBarrier(1, &barrier2);
	}

	void Game::RenderSceneToShadowMap()
	{
		D3D12_VIEWPORT viewport = mShadowMap->GetViewport();
		D3D12_RECT scissorRect = mShadowMap->GetScissorRect();
		D3D12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor = mShadowMap->GetDSV();
		//D3D12_CPU_DESCRIPTOR_HANDLE handle = mDynamicCubeMap->GetRTV();

		mCommandList->RSSetViewports(1, &viewport);
		mCommandList->RSSetScissorRects(1, &scissorRect);

		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mCommandList->ResourceBarrier(1, &barrier1);

		//mCommandList->ClearRenderTargetView(handle, mClearColor, 0, nullptr);
		mCommandList->ClearDepthStencilView(hCpuDescriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		mCommandList->OMSetRenderTargets(0, nullptr, true, &hCpuDescriptor);

		auto passCB = mFrameResources[mCurrFrameResourceIndex]->PassCB->Resource();
		UINT passCBByteSizes = CalcConstantBufferByteSize(sizeof(PassConstant));
		D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + passCBByteSizes;
		mCommandList->SetGraphicsRootConstantBufferView(RSP_PassCB, passCBAddress);

		mCommandList->SetPipelineState(mPSOs[L"shadow"].Get());
		RenderActors(mCommandList.Get(), mFrameResources[mCurrFrameResourceIndex].get(), mAssetManager.GetActors(Render_Layer_Opaque | Render_Layer_OpaqueDynamicReflectors));

		CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
		mCommandList->ResourceBarrier(1, &barrier2);
	}

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> Game::GetStaticSamplers()
	{
		const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
			0, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
			1, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
			2, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
			3, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
			4, // shaderRegister
			D3D12_FILTER_ANISOTROPIC, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
			0.0f,                             // mipLODBias
			8);                               // maxAnisotropy

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
			5, // shaderRegister
			D3D12_FILTER_ANISOTROPIC, // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
			0.0f,                              // mipLODBias
			8);                                // maxAnisotropy

		const CD3DX12_STATIC_SAMPLER_DESC shadow(
			6, // shaderRegister
			D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
			0.0f,                               // mipLODBias
			16,                                 // maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

		return {
			pointWrap, pointClamp,
			linearWrap, linearClamp,
			anisotropicWrap, anisotropicClamp,
			shadow
		};
	}

	void Game::Pick(int screenX, int screenY)
	{
		DirectX::XMFLOAT4X4 P = mCamera.GetProjMatrix4x4f();
		DirectX::XMMATRIX V = mCamera.GetViewMatrix();
		auto d = DirectX::XMMatrixDeterminant(V);
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&d, V);

		// Ray definition in view space.
		float vx = (+2.0f * screenX / mClientWidth - 1.0f) / P(0, 0);
		float vy = (-2.0f * screenY / mClientHeight + 1.0f) / P(1, 1);
		DirectX::XMVECTOR rayViewOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		DirectX::XMVECTOR rayViewDir = DirectX::XMVector3Normalize(DirectX::XMVectorSet(vx, vy, 1.0f, 0.0f));

		// Assume nothing is picked to start, so the picked render-item is invisible.
		mPickedActor->Visible = false;

		auto actors = mAssetManager.GetActors(Render_Layer_All - Render_Layer_Highlight);
		float distViewMin = FLT_MAX;

		for (auto actor : actors)
		{
			TLOG(actor->ToString().c_str());
			TLOG(L"\n");

			// Skip invisible render-items.
			if (actor->Visible == false)
				continue;

			DirectX::XMVECTOR rayLocalOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			DirectX::XMVECTOR rayLocalDir = DirectX::XMVector3Normalize(DirectX::XMVectorSet(vx, vy, 1.0f, 0.0f));

			DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&actor->Instance.World);
			auto d = DirectX::XMMatrixDeterminant(W);
			DirectX::XMMATRIX invWorld = XMMatrixInverse(&d, W);

			DirectX::XMMATRIX toView = DirectX::XMMatrixMultiply(W, V);
			DirectX::XMMATRIX toLocal = DirectX::XMMatrixMultiply(invView, invWorld);

			rayLocalOrigin = DirectX::XMVector3TransformCoord(rayLocalOrigin, toLocal);
			rayLocalDir = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayLocalDir, toLocal));

			auto submesh = actor->Group->DrawArgs[actor->DrawArg];
			float blah = 0.0f;
			if (submesh.Bound.Intersects(rayLocalOrigin, rayLocalDir, blah))
			{
				float distLocalMin = FLT_MAX;

				// NOTE: For the demo, we know what to cast the vertex/index data to.  If we were mixing
				// formats, some metadata would be needed to figure out what to cast it to.
				auto vertices = (Vertex*)actor->Group->VertexBufferCPU->GetBufferPointer();
				auto indices = (std::uint16_t*)actor->Group->IndexBufferCPU->GetBufferPointer();
				UINT triCount = submesh.IndexCount / 3;

				// Find the nearest ray/triangle intersection.
				for (UINT i = 0; i < triCount; ++i)
				{
					// Indices for this triangle.
					UINT i0 = indices[i * 3 + 0];
					UINT i1 = indices[i * 3 + 1];
					UINT i2 = indices[i * 3 + 2];

					// Vertices for this triangle.
					DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&vertices[i0].Position);
					DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&vertices[i1].Position);
					DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&vertices[i2].Position);

					// We have to iterate over all the triangles in order to find the nearest intersection.
					float distLocal = FLT_MAX;
					if (DirectX::TriangleTests::Intersects(rayLocalOrigin, rayLocalDir, v0, v1, v2, distLocal))
					{
						if (distLocal < distLocalMin)
						{
							// This is the new nearest picked triangle in the local space.
							distLocalMin = distLocal;

							v0 = DirectX::XMVector3TransformCoord(v0, toView);
							v1 = DirectX::XMVector3TransformCoord(v1, toView);
							v2 = DirectX::XMVector3TransformCoord(v2, toView);

							float distView = FLT_MAX;
							if (DirectX::TriangleTests::Intersects(rayViewOrigin, rayViewDir, v0, v1, v2, distView))
							{
								if (distView < distViewMin)
								{
									// This is the new nearest picked triangle in the view space.
									distViewMin = distView;

									mPickedActor->Visible = true;
									mPickedActor->Group = actor->Group;
									mPickedActor->Group->DrawArgs[mPickedActor->DrawArg].IndexCount = 3;
									mPickedActor->Group->DrawArgs[mPickedActor->DrawArg].BaseVertexLocation = 0;
									mPickedActor->Group->DrawArgs[mPickedActor->DrawArg].StartIndexLocation = 3 * i;
									mPickedActor->Instance.World = actor->Instance.World;
									mPickedActor->Instance.TexTransform = actor->Instance.TexTransform;
								}
							}
						}
					}
				}
			}
		}
	}

}
