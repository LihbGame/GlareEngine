#include "Direct3D12Window.h"
#include "Grass.h"
#include "ModelMesh.h"
#include "resource.h"

using namespace GlareEngine;

Direct3D12Window::Direct3D12Window()
{
	transforms.resize(96);

	mObjCBByteSize = EngineUtility::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	mSkinnedCBByteSize = EngineUtility::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
	mTerrainCBByteSize = EngineUtility::CalcConstantBufferByteSize(sizeof(TerrainConstants));

}

Direct3D12Window::~Direct3D12Window()
{
	if (d3dDevice != nullptr)
		FlushCommandQueue();
	 
	for (auto& e : Shaders)
	{
		delete e.second;
		e.second = nullptr;
	}
}

bool Direct3D12Window::Initialize()
{
	if (!InitDirect3D())
		return false;

	// 将命令列表重置为准备初始化命令。
	ThrowIfFailed(CommandList->Reset(DirectCmdListAlloc.Get(), nullptr));

	// 获取此堆类型中描述符的增量大小。 这是特定于硬件的，因此我们必须查询此信息。
	CbvSrvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//texture manage
	textureManage = std::make_unique<TextureManage>(d3dDevice.Get(), CommandList.Get(), CbvSrvDescriptorSize, gNumFrameResources);
	//pso init
	PSOs = std::make_unique<PSO>();
	//wave :not use
	mWaves = std::make_unique<Waves>(256, 256, 4.0f, 0.03f, 8.0f, 0.2f);
	//ShockWaveWater
	mShockWaveWater = std::make_unique<ShockWaveWater>(d3dDevice.Get(), i_Width, i_Height, m4xMsaaState, textureManage.get());
	//camera
	Camera.LookAt(XMFLOAT3(-600.0f, 100.0f, 500.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
	//sky
	sky = std::make_unique<Sky>(d3dDevice.Get(), CommandList.Get(), 5.0f, 20, 20, textureManage.get());
	//shadow map
	shadowMap = std::make_unique<ShadowMap>(d3dDevice.Get(), ShadowMapSize, ShadowMapSize, textureManage.get());
	//Simple Geometry Instance Draw
	simpleGeoInstance = std::make_unique<SimpleGeoInstance>(CommandList.Get(), d3dDevice.Get(), textureManage.get());
	//init model loader
	modelLoder = std::make_unique<ModelLoader>(i_hWnd, d3dDevice.Get(), CommandList.Get(), textureManage.get());
	//init HeightMap Terrain
	mHeightMapTerrain = std::make_unique<HeightmapTerrain>(d3dDevice.Get(), CommandList.Get(), textureManage.get(), HeightmapTerrainInit(), nullptr);

	{
		LoadModel();
		BuildAllMaterials();
		CreateDescriptorHeaps();
		BuildRootSignature();
		BuildShadersAndInputLayout();
		BuildSimpleGeometry();
		BuildLandGeometry();
		BuildWavesGeometryBuffers();
		BuildRenderItems();
		BuildModelGeoInstanceItems();
		BuildFrameResources();
		BuildPSOs();
	}

	//init UI
	EngineUI.InitGUI(i_hWnd, d3dDevice.Get(), CommandList.Get(), mGUISrvDescriptorHeap.Get(), gNumFrameResources);

	//Wire Frame State
	mOldWireFrameState = EngineUI.IsWireframe();

	// 执行初始化命令。
	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 等待初始化完成。
	FlushCommandQueue();

	return true;
}

bool Direct3D12Window::InitDirect3D()
{
	CreateDirectDevice();
	CreateCommandQueueAndSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	OnResize();

	return true;
}

bool Direct3D12Window::IsCreate()
{
	return d3dDevice.Get();
}

void Direct3D12Window::OnResize()
{
	assert(d3dDevice);
	assert(SwapChain);
	assert(DirectCmdListAlloc);

	// 在更改任何资源之前刷新。
	FlushCommandQueue();

	// 重置 command list
	ThrowIfFailed(CommandList->Reset(DirectCmdListAlloc.Get(), nullptr));

	// 释放我们将重新创建的先前资源。
	for (int i = 0; i < SwapChainBufferCount; ++i)
		SwapChainBuffer[i].Reset();

	DepthStencilBuffer.Reset();//深度缓冲只有一个

	// 调整交换链的大小。
	ThrowIfFailed(SwapChain->ResizeBuffers(
		SwapChainBufferCount,
		i_Width, i_Height,
		BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	// 每次调整大小，需要将后缓冲置为0。
	mCurrBackBuffer = 0;

	//创建渲染目标视图（RTV）。
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	// 为每个帧创建一个RTV。
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i])));
		d3dDevice->CreateRenderTargetView(SwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, RtvDescriptorSize);
	}

	D3D12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// MSAA RTV
	if (m4xMsaaState)
	{
		// 创建一个 MSAA 渲染目标。
		D3D12_RESOURCE_DESC msaaRTDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			BackBufferFormat,
			i_Width,
			i_Height,
			1, // 这个渲染目标视图只有一个纹理。
			1, // Use a single mipmap level
			4
		);
		msaaRTDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE msaaOptimizedClearValue = {};
		msaaOptimizedClearValue.Format = BackBufferFormat;
		msaaOptimizedClearValue.Color[0] = { 0.0f };
		msaaOptimizedClearValue.Color[1] = { 0.0f };
		msaaOptimizedClearValue.Color[2] = { 0.0f };
		msaaOptimizedClearValue.Color[3] = { 0.0f };

		ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&msaaRTDesc,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			&msaaOptimizedClearValue,
			IID_PPV_ARGS(MSAARenderTargetBuffer.ReleaseAndGetAddressOf())
		));

		MSAARenderTargetBuffer->SetName(L"MSAA Render Target");

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = BackBufferFormat;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

		d3dDevice->CreateRenderTargetView(
			MSAARenderTargetBuffer.Get(), &rtvDesc,
			MSAARTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	}


	// 创建深度/模板缓冲区和视图。
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = i_Width;
	depthStencilDesc.Height = i_Height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&HeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(DepthStencilBuffer.GetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, RtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// 使用资源的格式创建描述符以 mip 级别 0 的整个资源。
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.Texture2D.MipSlice = 0;
	d3dDevice->CreateDepthStencilView(DepthStencilBuffer.Get(), &dsvDesc, dsvHandle);

	// 将资源从其初始状态转换为用作深度缓冲区。
	D3D12_RESOURCE_BARRIER Barriers = CD3DX12_RESOURCE_BARRIER::Transition(DepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	CommandList->ResourceBarrier(1, &Barriers);

	// Execute the resize commands.
	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// 此处更新窗口大小和裁剪大小
	ScreenViewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, static_cast<float>(i_Width), static_cast<float>(i_Height),0.0f,1.0f };
	ScissorRect = CD3DX12_RECT{ 0, 0, i_Width, i_Height };

	//窗口调整大小，因此更新宽高比并重新计算投影矩阵;
	Camera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 400000.0f);
	//窗口调整大小重新计算视锥包围体。
	BoundingFrustum::CreateFromMatrix(mCamFrustum, Camera.GetProj());
	//Shock Wave Water map resize
	if (mShockWaveWater != nullptr)
	{
		mShockWaveWater->OnResize(i_Width,i_Height);
	}
}

void Direct3D12Window::Update()
{
	mTimer.Tick();
	
	mNewWireFrameState = EngineUI.IsWireframe();

	UpdateCamera();

	// 循环遍历循环框架资源数组。
	CurrFrameResourceIndex = (CurrFrameResourceIndex + 1) % gNumFrameResources;
	CurrFrameResource = FrameResources[CurrFrameResourceIndex].get();

	// GPU是否已完成对当前帧资源的命令的处理？
	//如果没有，请等到GPU完成命令直到该防护点为止。
	if (CurrFrameResource->Fence != 0 && Fence->GetCompletedValue() < CurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, (LPCWSTR)false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(Fence->SetEventOnCompletion(CurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}


	UpdateObjectCBs();
	UpdateInstanceCBs();
	UpdateMaterialCBs();
	shadowMap->UpdateShadowTransform();
	UpdateMainPassCB();
	UpdateShadowPassCB();
	UpdateTerrainPassCB();

	//UpdateAnimation();
	//UpdateWaves();
}

void Direct3D12Window::Render()
{
	auto cmdListAlloc = CurrFrameResource->CmdListAlloc;

	//重新使用与命令记录关联的内存。
	//只有关联的命令列表在GPU上完成执行后，我们才能重置。
	ThrowIfFailed(cmdListAlloc->Reset());
	//通过ExecuteCommandList将命令列表添加到命令队列后，可以将其重置。
	//重用命令列表将重用内存。
	ThrowIfFailed(CommandList->Reset(cmdListAlloc.Get(), PSOs.get()->GetPSO(PSOName::Opaque).Get()));

	CommandList->RSSetViewports(1, &ScreenViewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	//设置SRV描述符堆
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
		CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		//Set Graphics Root Signature
		CommandList->SetGraphicsRootSignature(RootSignature.Get());
	}

	// 根描述符
	{
		// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
		// set as a root descriptor.
		auto matBuffer = CurrFrameResource->MaterialBuffer->Resource();
		CommandList->SetGraphicsRootShaderResourceView(4, matBuffer->GetGPUVirtualAddress());
		// Bind the sky cube map.  For our demos, we just use one "world" cube map representing the environment
		// from far away, so all objects will use the same cube map and we only need to set it once per-frame.  
		// If we wanted to use "local" cube maps, we would have to change them per-object, or dynamically
		// index into an array of cube maps.
		CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		skyTexDescriptor.Offset(Materials::GetMaterialInstance()->GetMaterial(L"sky")->PBRSrvHeapIndex[PBRTextureType::DiffuseSrvHeapIndex], CbvSrvDescriptorSize);
		CommandList->SetGraphicsRootDescriptorTable(5, skyTexDescriptor);


		// Bind all the textures used in this scene.  Observe
		// that we only have to specify the first descriptor in the table.  
		// The root signature knows how many descriptors are expected in the table.
		CommandList->SetGraphicsRootDescriptorTable(6, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	}

	//Draw shadow
	if (EngineUI.IsShowShadow())
	{
		if (Direct3D12Window::RedrawShadowMap)
		{
			DrawSceneToShadowMap();
			Direct3D12Window::RedrawShadowMap = false;
		}
	}
	else
	{
		if (!Direct3D12Window::RedrawShadowMap)
		{
			CommandList->ClearDepthStencilView(shadowMap->Dsv(),
				D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			Direct3D12Window::RedrawShadowMap = true;
		}
	}

	//放在绘制阴影的后面
	CommandList->RSSetViewports(1, &ScreenViewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	if (m4xMsaaState)//MSAA
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			MSAARenderTargetBuffer.Get(),
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		CommandList->ResourceBarrier(1, &barrier);

		auto rtvDescriptor = MSAARTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		auto dsvDescriptor = DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		CommandList->OMSetRenderTargets(1, &rtvDescriptor, false, &dsvDescriptor);

		CommandList->ClearRenderTargetView(rtvDescriptor, Colors::Transparent, 0, nullptr);
		CommandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		//const D3D12_SHADING_RATE_COMBINER combiner[] = { D3D12_SHADING_RATE_COMBINER_PASSTHROUGH , D3D12_SHADING_RATE_COMBINER_MIN };
		//mCommandList->RSSetShadingRate(D3D12_SHADING_RATE_2X2, combiner);
	}
	else
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, RtvDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		
		// Indicate a state transition on the resource usage.
		D3D12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(SwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		CommandList->ResourceBarrier(1, &Barrier);
		// Clear the back buffer and depth buffer.
		CommandList->ClearRenderTargetView(rtvHandle, Colors::LightSteelBlue, 0, nullptr);
		CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		// Specify the buffers we are going to render to.
		CommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);
	}


	//Draw Main
	{
		//Set Graphics Root CBV in slot 2
		auto passCB = CurrFrameResource->PassCB->Resource();
		CommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());


		//Draw Render Items (Opaque)
		if (EngineUI.IsShowLand())
		{
			CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::Opaque).Get());
			PIXBeginEvent(CommandList.Get(), 0, "Draw Main::RenderLayer::Opaque");
			DrawRenderItems(CommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
			PIXEndEvent(CommandList.Get());
		}
		//Draw Instanse 
		if (EngineUI.IsShowModel())
		{
			CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::SkinAnime).Get());
			PIXBeginEvent(CommandList.Get(), 0, "Draw Main::RenderLayer::InstanceSimpleItems");
			DrawRenderItems(CommandList.Get(), mRitemLayer[(int)RenderLayer::InstanceSimpleItems]);
			PIXEndEvent(CommandList.Get());
		}
		//Draw Sky box
		if (EngineUI.IsShowSky())
		{
			CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::Sky).Get());
			PIXBeginEvent(CommandList.Get(), 0, "Draw Main::RenderLayer::Sky");
			DrawRenderItems(CommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);
			PIXEndEvent(CommandList.Get());
		}
	}

	//Draw Height Map Terrain
	if (EngineUI.IsShowTerrain())
	{
		//Draw Height Map Terrain
		DrawHeightMapTerrain();
	}

	//Draw Grass
	if (EngineUI.IsShowGrass())
	{
		DrawGrass();
	}

	//Draw Shock Wave Water
	if (EngineUI.IsShowWater())
	{
		//Draw Shock Wave Water
		PIXBeginEvent(CommandList.Get(), 0, "Draw Shock Wave Water");
		DrawShockWaveWater();
		PIXEndEvent(CommandList.Get());
	}

	//设置UI SRV描述符堆
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { mGUISrvDescriptorHeap.Get() };
		CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		// Set Graphics Root Signature
		CommandList->SetGraphicsRootSignature(RootSignature.Get());
	}

	//Draw UI
	PIXBeginEvent(CommandList.Get(), 0, "Draw UI");
	EngineUI.Draw(CommandList.Get());
	PIXEndEvent(CommandList.Get());

	if (m4xMsaaState)
	{
		D3D12_RESOURCE_BARRIER Barriers[2] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(
				MSAARenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(
				SwapChainBuffer[mCurrBackBuffer].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
		};

		CommandList->ResourceBarrier(2, Barriers);
		CommandList->ResolveSubresource(SwapChainBuffer[mCurrBackBuffer].Get(), 0, MSAARenderTargetBuffer.Get(), 0, BackBufferFormat);
	}
	else
	{
		D3D12_RESOURCE_BARRIER Barriers = CD3DX12_RESOURCE_BARRIER::Transition(SwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

		CommandList->ResourceBarrier(1, &Barriers);
	}

	// 指示资源使用的状态转换。
	D3D12_RESOURCE_BARRIER Barriers = CD3DX12_RESOURCE_BARRIER::Transition(SwapChainBuffer[mCurrBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
	CommandList->ResourceBarrier(1, &Barriers);

	// Done recording commands.
	ThrowIfFailed(CommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(SwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	CurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	CommandQueue->Signal(Fence.Get(), mCurrentFence);
}

void Direct3D12Window::CreateDescriptorHeaps()
{
	int SRVIndex = 0;
	//SRV heap Size
	UINT PBRTextureNum = (UINT)simpleGeoInstance->GetTextureNames().size() * 6;
	UINT ModelTextureNum = (UINT)modelLoder->GetAllModelTextures().size() * 6;
	UINT ShockWaveWater = 3;
	UINT HeightMapTerrain = 32 + 2;
	UINT Grass = 6;
	UINT Noise = 1;

	// Create the SRV heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = PBRTextureNum + ModelTextureNum + ShockWaveWater + HeightMapTerrain + Noise + Grass;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mGUISrvDescriptorHeap)));

	// Fill out the heap with actual descriptors.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

#pragma region simple geometry PBR Texture
	simpleGeoInstance->FillSRVDescriptorHeap(&SRVIndex, &hDescriptor);
#pragma endregion

#pragma region Model SRV
	modelLoder->FillSRVDescriptorHeap(&SRVIndex, &hDescriptor);
#pragma endregion

#pragma region Shadow Map
	auto dsvCpuStart = DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	shadowMap->FillSRVDescriptorHeap(&SRVIndex, &hDescriptor, dsvCpuStart, DsvDescriptorSize);
	mShadowMapIndex = shadowMap->GetShadowMapIndex();
#pragma endregion

#pragma region ShockWaveWater
	mShockWaveWater->FillSRVDescriptorHeap(&SRVIndex, &hDescriptor);
	mWaterReflectionMapIndex = mShockWaveWater->GetWaterReflectionMapIndex();
	mWaterRefractionMapIndex = mShockWaveWater->GetWaterRefractionMapIndex();
	mWaterDumpWaveIndex = mShockWaveWater->GetWaterDumpWaveIndex();
#pragma region

#pragma region height map terrain
	mHeightMapTerrain->FillSRVDescriptorHeap(&SRVIndex, &hDescriptor);
	mHeightMapIndex = mHeightMapTerrain->GetHeightMapIndex();
	mBlendMapIndex = mHeightMapTerrain->GetBlendMapIndex();
#pragma endregion

#pragma region Grass
	mHeightMapTerrain->GetGrass()->FillSRVDescriptorHeap(&SRVIndex, &hDescriptor);
	mRGBNoiseMapIndex = mHeightMapTerrain->GetGrass()->GetRGBNoiseMapIndex();
#pragma endregion

	//for root Signature
	mSRVSize = SRVIndex;

#pragma region HDR Sky
	sky->FillSRVDescriptorHeap(&SRVIndex, &hDescriptor);
#pragma endregion

}

void Direct3D12Window::OnLButtonDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(i_hWnd);
}

void Direct3D12Window::OnLButtonUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void Direct3D12Window::OnRButtonDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(i_hWnd);
}

void Direct3D12Window::OnRButtonUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void Direct3D12Window::OnMButtonDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(i_hWnd);
}

void Direct3D12Window::OnMButtonUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void Direct3D12Window::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_MBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		Camera.Pitch(dy);
		Camera.RotateY(dx);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		Camera.Move(mLastMousePos.x - x, mLastMousePos.y - y, 0);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void Direct3D12Window::OnMouseWheel(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_MBUTTON) == 0)	// 先判断中键是否被按下，如果没有按下责执行滚轮滚动的代码
	{
		// 判断滚轮是上滚还是下滚
		short nOffset = HIWORD(btnState);

		if (nOffset > 0)
		{
			// 根据输入更新摄像机半径
			Camera.FocalLength(-0.008f);
		}
		else if (nOffset < 0)
		{
			// 根据输入更新摄像机半径
			Camera.FocalLength(+0.008f);
		}
	}
}

void Direct3D12Window::ActiveSystemDesktopSwap()
{
	if (SwapChain)
	{
		SwapChain->SetFullscreenState(false, nullptr);
	}
}

void Direct3D12Window::CalculateFrameStats()
{
	//代码计算每秒的平均帧数，
	//以及渲染一帧所需的平均时间。

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

		std::wstring Text =
			L"    FPS: " + fpsStr +
			L"   MSPF: " + mspfStr;

		EngineUI.SetFPSText(Text);

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void Direct3D12Window::OnKeyDown(UINT8 key)
{
	float CameraModeSpeed = EngineUI.GetCameraModeSpeed();
	float x = 0.0f, y = 0.0f, z = 0.0f;

	// 控制输入
	{
	}
	// 命令输入
	{
	}
	// 相机部分 
	{
		if (GetAsyncKeyState(VK_LEFT) & 0x8000)
			x = -CameraModeSpeed / mTimer.TotalTime();
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
			x = CameraModeSpeed / mTimer.TotalTime();
		if (GetAsyncKeyState(VK_UP) & 0x8000)
			z = CameraModeSpeed / mTimer.TotalTime();
		if (GetAsyncKeyState(VK_DOWN) & 0x8000)
			z = -CameraModeSpeed / mTimer.TotalTime();
		if (key == VK_SPACE);
	}

	Camera.Move(x, y, z);
	XMFLOAT3 camPos = Camera.GetPosition3f();
	static float width = 2048.0f;
	if (camPos.x<-width || camPos.x >width ||
		camPos.z < -width || camPos.z >width)
	{
	}
	else
	{

		float y = mHeightMapTerrain->GetHeight(camPos.x, camPos.z) + 2.0f;
		if (camPos.y <= y)
		{
			Camera.SetPosition(camPos.x, y, camPos.z);
		}
	}

	EngineUI.SetCameraPosition(camPos);
}

void Direct3D12Window::OnKeyUp(UINT8 key)
{
	switch (key)
	{
	case VK_LEFT:
		break;
	case VK_RIGHT:
		break;
	case VK_UP:
		break;
	case VK_DOWN:
		break;
	}
}


void Direct3D12Window::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (DXGIFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void Direct3D12Window::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());
		EngineLog::AddLog(text.c_str());
		LogOutputDisplayModes(output, BackBufferFormat);
		ReleaseCom(output);

		++i;
	}
}

void Direct3D12Window::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";
		EngineLog::AddLog(text.c_str());
		::OutputDebugString(text.c_str());
	}
}

void Direct3D12Window::FlushCommandQueue()
{
	// 创建围栏
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));

	// 提前围栏值以标记到此围栏点的命令.
	mCurrentFence++;

	// 向命令队列添加指令以设置新的栅栏点。 因为我们在GPU时间轴上，
	//所以在GPU完成处理此Signal（）之前的所有命令之前，不会设置新的栅栏点。
	ThrowIfFailed(CommandQueue->Signal(Fence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (Fence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, (LPCWSTR)false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(Fence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);//等待命令执行完成
		CloseHandle(eventHandle);
	}
}

//not use(球面坐标)
void Direct3D12Window::UpdateCamera()
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	Camera.UpdateViewMatrix();
}

void Direct3D12Window::UpdateObjectCBs()
{
	auto currObjectCB = CurrFrameResource->SimpleObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->ObjCBIndex >= 0)
		{
			// Only update the cbuffer data if the constants have changed.  
			// This needs to be tracked per frame resource.
			if (e->NumFramesDirty > 0)
			{
				XMMATRIX world = XMLoadFloat4x4(&e->World);
				XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

				ObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
				objConstants.MaterialIndex = e->Mat->MatCBIndex;
				currObjectCB->CopyData(e->ObjCBIndex, objConstants);

				// Next FrameResource need to be updated too.
				e->NumFramesDirty--;
			}
		}
	}
}

void Direct3D12Window::UpdateInstanceCBs()
{
	XMMATRIX View = Camera.GetView();
	XMVECTOR Determinant = XMMatrixDeterminant(View);
	XMMATRIX InvView = XMMatrixInverse(&Determinant, View);


	for (int index = 0; index < mRitemLayer[(int)RenderLayer::InstanceSimpleItems].size(); ++index)
	{
		const auto& instanceData = mRitemLayer[(int)RenderLayer::InstanceSimpleItems][index]->Instances;
		const auto& ReflectioninstanceData = mReflectionWaterLayer[(int)RenderLayer::InstanceSimpleItems][index]->Instances;
		int visibleInstanceCount = 0;

		std::vector<std::wstring> ModelTextureNames = modelLoder->GetModelTextureNames(L"Blue_Tree_02a");

		for (int matNum = 0; matNum < ModelTextureNames.size(); ++matNum)
		{
			visibleInstanceCount = 0;
			auto currInstanceBuffer = CurrFrameResource->InstanceSimpleObjectCB[matNum].get();
			auto ReflectioncurrInstanceBuffer = CurrFrameResource->ReflectionInstanceSimpleObjectCB[matNum].get();
			for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
			{
				XMMATRIX World = XMLoadFloat4x4(&instanceData[i].World);
				XMMATRIX ReflectionWorld = XMLoadFloat4x4(&ReflectioninstanceData[i].World);

				XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

				Determinant = XMMatrixDeterminant(World);
				XMMATRIX InvWorld = XMMatrixInverse(&Determinant, World);

				// 视图空间转换到局部空间
				XMMATRIX viewToLocal = XMMatrixMultiply(InvView, InvWorld);

				// 将摄像机视锥从视图空间转换为对象的局部空间。
				BoundingFrustum localSpaceFrustum;
				mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

				// 在局部空间中执行盒子/视锥相交测试。
				if ((localSpaceFrustum.Contains(mRitemLayer[(int)RenderLayer::InstanceSimpleItems][index]->Bounds) != DirectX::DISJOINT) || (mFrustumCullingEnabled == false))
				{
					InstanceConstants data;
					XMStoreFloat4x4(&data.World, XMMatrixTranspose(World));
					XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));

					data.MaterialIndex = Materials::GetMaterialInstance()->GetMaterial(std::wstring(ModelTextureNames[matNum].begin(), ModelTextureNames[matNum].end()))->MatCBIndex;

					InstanceConstants Reflectiondata = data;
					XMStoreFloat4x4(&Reflectiondata.World, XMMatrixTranspose(ReflectionWorld));
					///将实例数据写入可见对象的结构化缓冲区。
					//反射场景
					ReflectioncurrInstanceBuffer->CopyData(visibleInstanceCount, Reflectiondata);
					//正常场景
					currInstanceBuffer->CopyData(visibleInstanceCount++, data);
				}
			}
		}
		mRitemLayer[(int)RenderLayer::InstanceSimpleItems][index]->InstanceCount = visibleInstanceCount;
		mReflectionWaterLayer[(int)RenderLayer::InstanceSimpleItems][index]->InstanceCount = visibleInstanceCount;
	}
}

void Direct3D12Window::UpdateMaterialCBs()
{
	auto currMaterialBuffer = CurrFrameResource->MaterialBuffer.get();
	for (auto& e : Materials::GetMaterialInstance()->GetAllMaterial())
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		::Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.NormalSrvHeapIndex = mat->PBRSrvHeapIndex[PBRTextureType::NormalSrvHeapIndex];
			matConstants.RoughnessSrvHeapIndex = mat->PBRSrvHeapIndex[PBRTextureType::RoughnessSrvHeapIndex];
			matConstants.MetallicSrvHeapIndex = mat->PBRSrvHeapIndex[PBRTextureType::MetallicSrvHeapIndex];
			matConstants.AOSrvHeapIndex = mat->PBRSrvHeapIndex[PBRTextureType::AoSrvHeapIndex];
			matConstants.DiffuseMapIndex = mat->PBRSrvHeapIndex[PBRTextureType::DiffuseSrvHeapIndex];
			matConstants.HeightSrvHeapIndex = mat->PBRSrvHeapIndex[PBRTextureType::HeightSrvHeapIndex];
			matConstants.height_scale = mat->height_scale;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialBuffer->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void Direct3D12Window::UpdateMainPassCB()
{
	XMFLOAT4X4 View4x4f = Camera.GetView4x4f();
	XMMATRIX View = XMLoadFloat4x4(&View4x4f);
	XMFLOAT4X4 Proj4x4f = Camera.GetProj4x4f();
	XMMATRIX Proj = XMLoadFloat4x4(&Proj4x4f);

	XMVECTOR Determinant;
	XMMATRIX ViewProj = XMMatrixMultiply(View, Proj);
	Determinant = XMMatrixDeterminant(View);
	XMMATRIX InvView = XMMatrixInverse(&Determinant, View);
	Determinant = XMMatrixDeterminant(Proj);
	XMMATRIX InvProj = XMMatrixInverse(&Determinant, Proj);
	Determinant = XMMatrixDeterminant(ViewProj);
	XMMATRIX InvViewProj = XMMatrixInverse(&Determinant, ViewProj);

	XMMATRIX Float4x4ShadowTransform = XMLoadFloat4x4(&shadowMap->mShadowTransform);
	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(View));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(InvView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(Proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(InvProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(ViewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(InvViewProj));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(Float4x4ShadowTransform));

	mMainPassCB.EyePosW = Camera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)i_Width, (float)i_Height);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / i_Width, 1.0f / i_Height);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = mTimer.TotalTime();
	mMainPassCB.DeltaTime = mTimer.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };


	mMainPassCB.Lights[0].Direction = shadowMap->mRotatedLightDirections[0];
	mMainPassCB.Lights[0].Strength = { 1.0f, 1.0f, 1.0f };
	mMainPassCB.Lights[0].FalloffStart = 1.0f;
	mMainPassCB.Lights[0].FalloffEnd = 50.0f;
	mMainPassCB.Lights[0].Position = { 20,20,20 };

	mMainPassCB.Lights[1].Direction = shadowMap->mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 1.0f, 1.0f, 1.0f };
	mMainPassCB.Lights[1].FalloffStart = 1.0f;
	mMainPassCB.Lights[1].FalloffEnd = 50.0f;
	mMainPassCB.Lights[1].Position = { 20,20,20 };

	mMainPassCB.Lights[2].Direction = shadowMap->mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 1.0f, 1.0f, 1.0f };
	mMainPassCB.Lights[2].FalloffStart = 1.0f;
	mMainPassCB.Lights[2].FalloffEnd = 50.0f;
	mMainPassCB.Lights[2].Position = { 20,20,20 };

	//SRV Index
	mMainPassCB.ShadowMapIndex = mShadowMapIndex;
	mMainPassCB.WaterDumpWaveIndex = mWaterDumpWaveIndex;
	mMainPassCB.WaterReflectionMapIndex = mWaterReflectionMapIndex;
	mMainPassCB.WaterRefractionMapIndex = mWaterRefractionMapIndex;
	//Fog
	mMainPassCB.FogStart = EngineUI.GetFogStart();
	mMainPassCB.FogRange = EngineUI.GetFogRange();
	mMainPassCB.FogColor = { 1.0f, 1.0f, 1.0f,1.0f };
	mMainPassCB.FogEnabled = (int)EngineUI.IsShowFog();
	mMainPassCB.ShadowEnabled = (int)EngineUI.IsShowShadow();

	auto currPassCB = CurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);//index 0 main pass
}

void Direct3D12Window::UpdateWaves()
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(mTimer.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = CurrFrameResource->WavesVB.get();
	for (int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertices::PosNormalTexc v;

		v.Position = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mSphereRitem->Geo[0]->VertexBufferGPU = currWavesVB->Resource();
}

void Direct3D12Window::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, mSRVSize, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[7];
	// Perfomance TIP: Order from most frequent to least frequent.
	// Create root CBV.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsConstantBufferView(2);
	slotRootParameter[3].InitAsShaderResourceView(0, 1);
	slotRootParameter[4].InitAsShaderResourceView(1, 1);


	slotRootParameter[5].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[6].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_ALL);

	auto staticSamplers = GetStaticSamplers();
	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(7, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(RootSignature.GetAddressOf())));
}

void Direct3D12Window::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO SkinAnimeDefines[] =
	{
		"ALPHA_TEST", "1",
		"SKINNED","1",
		NULL, NULL
	};
	 
	std::wstring ShaderPath = L"Shaders\\";

	Shaders[L"GerstnerWave"] = new GerstnerWaveShader(ShaderPath+L"DefaultVS.hlsl", ShaderPath + L"DefaultPS.hlsl");

	Shaders[L"Sky"] = new SkyShader(ShaderPath + L"SkyVS.hlsl", ShaderPath + L"SkyPS.hlsl");

	Shaders[L"Instance"] = new SimpleGeometryInstanceShader(ShaderPath + L"SimpleGeoInstanceVS.hlsl", ShaderPath + L"SimpleGeoInstancePS.hlsl");

	Shaders[L"InstanceSimpleGeoShadowMap"] = new SimpleGeometryShadowMapShader(ShaderPath + L"SimpleGeoInstanceShadowVS.hlsl", ShaderPath + L"SimpleGeoInstanceShadowPS.hlsl", L"", L"", L"", alphaTestDefines);

	Shaders[L"StaticComplexModelInstance"] = new ComplexStaticModelInstanceShader(ShaderPath + L"SimpleGeoInstanceVS.hlsl", ShaderPath + L"ComplexModelInstancePS.hlsl", L"", L"", L"", alphaTestDefines);

	Shaders[L"SkinAnime"] = new SkinAnimeShader(ShaderPath + L"SimpleGeoInstanceVS.hlsl", ShaderPath + L"ComplexModelInstancePS.hlsl", L"", L"", L"", SkinAnimeDefines);

	Shaders[L"WaterRefractionMask"] = new WaterRefractionMaskShader(ShaderPath + L"WaterRefractionMaskVS.hlsl", ShaderPath + L"WaterRefractionMaskPS.hlsl");

	Shaders[L"ShockWaveWater"] = new ShockWaveWaterShader(ShaderPath + L"ShockWaveWaterVS.hlsl", ShaderPath + L"ShockWaveWaterPS.hlsl");

	Shaders[L"HeightMapTerrain"] = new HeightMapTerrainShader(ShaderPath + L"HeightMapTerrainVS.hlsl", ShaderPath + L"HeightMapTerrainPS.hlsl", ShaderPath + L"HeightMapTerrainHS.hlsl", ShaderPath + L"HeightMapTerrainDS.hlsl");

	Shaders[L"Grass"] = new GrassShader(ShaderPath + L"GrassVS.hlsl", ShaderPath + L"GrassPS.hlsl", L"", L"", ShaderPath + L"GrassGS.hlsl");

	Shaders[L"GrassShadow"] = new GrassShader(ShaderPath + L"GrassShadowVS.hlsl", L"", L"", L"", ShaderPath + L"GrassShadowGS.hlsl");
}

void Direct3D12Window::BuildSimpleGeometry()
{
	//box mesh
	mGeometries[L"Box"] = std::move(simpleGeoInstance->GetSimpleGeoMesh(SimpleGeometry::Box));

	//build sky geometrie
	sky->BuildSkyMesh();
	mGeometries[L"Sky"] = std::move(sky->GetSkyMesh());
}

void Direct3D12Window::BuildLandGeometry()
{
	GeometryGenerator geoGen;
	MeshData grid = geoGen.CreateGrid(200.0f, 200.0f, 2, 2);

	//
	// Extract the vertex elements we are interested and apply the height function to
	// each vertex.  In addition, color the vertices based on their height so we have
	// sandy looking beaches, grassy low hills, and snow mountain peaks.
	//

	std::vector<Vertices::PosNormalTangentTexc> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		vertices[i].Position = grid.Vertices[i].Position;
		vertices[i].Normal = grid.Vertices[i].Normal;
		vertices[i].Tangent = grid.Vertices[i].Tangent;
		vertices[i].Texc = grid.Vertices[i].Texc;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertices::PosNormalTangentTexc);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<::MeshGeometry>();
	geo->Name = L"landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = EngineUtility::CreateDefaultBuffer(d3dDevice.Get(),
		CommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = EngineUtility::CreateDefaultBuffer(d3dDevice.Get(),
		CommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertices::PosNormalTangentTexc);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	::SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	geo->DrawArgs[L"grid"] = submesh;

	mGeometries[L"landGeo"] = std::move(geo);
}

void Direct3D12Window::BuildWavesGeometryBuffers()
{
	std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0xffffffff);

	// Iterate over each quad.
	int m = mWaves->RowCount();
	int n = mWaves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; ++i)
	{
		for (int j = 0; j < n - 1; ++j)
		{
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertices::PosNormalTexc);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<::MeshGeometry>();
	geo->Name = L"waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = EngineUtility::CreateDefaultBuffer(d3dDevice.Get(),
		CommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertices::PosNormalTexc);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	::SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs[L"grid"] = submesh;

	mGeometries[L"waterGeo"] = std::move(geo);
}

void Direct3D12Window::BuildPSOs()
{
	D3D12_RASTERIZER_DESC BaseRasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	if (mNewWireFrameState)
	{
		BaseRasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	}

	// PSO for opaque objects.
#pragma region PSO_for_opaque_objects
	std::vector<D3D12_INPUT_ELEMENT_DESC> Input = Shaders[L"GerstnerWave"]->GetInputLayout();
	DXGI_FORMAT RTVFormats[8];
	RTVFormats[0] = BackBufferFormat;//only one rtv
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::Opaque,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"GerstnerWave"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"GerstnerWave"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"GerstnerWave"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"GerstnerWave"]->GetPSShader()->GetBufferSize() },
		{ 0 },
		{ 0 },
		{ 0 },
		{ 0 },
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		BaseRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{ 0 },
		{});
#pragma endregion 


	// PSO for sky.
#pragma region PSO_for_sky
	Input = Shaders[L"Sky"]->GetInputLayout();
	D3D12_RASTERIZER_DESC RasterizerState = BaseRasterizerState;
	RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	D3D12_DEPTH_STENCIL_DESC  DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::Sky,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"Sky"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"Sky"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"Sky"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"Sky"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		RasterizerState,
		DepthStencilState,
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for Instance .
#pragma region PSO_for_Instance
	Input = Shaders[L"Instance"]->GetInputLayout();
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::Instance,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"Instance"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"Instance"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"Instance"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"Instance"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		BaseRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for Instance  shadow.
#pragma region PSO_for_Instance_shadow
	D3D12_RASTERIZER_DESC ShadowRasterizerState = BaseRasterizerState;
	ShadowRasterizerState.DepthBias = 25000;
	ShadowRasterizerState.DepthBiasClamp = 0.0f;
	ShadowRasterizerState.SlopeScaledDepthBias = 1.0f;
	RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	Input = Shaders[L"InstanceSimpleGeoShadowMap"]->GetInputLayout();
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::InstanceSimpleShadow_Opaque,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"InstanceSimpleGeoShadowMap"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"InstanceSimpleGeoShadowMap"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"InstanceSimpleGeoShadowMap"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"InstanceSimpleGeoShadowMap"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		ShadowRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		0,
		RTVFormats,
		DXGI_FORMAT_D32_FLOAT,
		{ UINT(1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for Complex Model Instance .
#pragma region PSO_for_Complex_Model_Instance
	RTVFormats[0] = BackBufferFormat;//only one rtv
	Input = Shaders[L"StaticComplexModelInstance"]->GetInputLayout();
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::StaticComplexModelInstance,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"StaticComplexModelInstance"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"StaticComplexModelInstance"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"StaticComplexModelInstance"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"StaticComplexModelInstance"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		BaseRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for skin anime .
#pragma region PSO_for_skin_anime
	RTVFormats[0] = BackBufferFormat;//only one rtv
	Input = Shaders[L"SkinAnime"]->GetInputLayout();
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::SkinAnime,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"SkinAnime"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"SkinAnime"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"SkinAnime"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"SkinAnime"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		BaseRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion

	if (mOldWireFrameState == mNewWireFrameState)
	{
		// PSO for Water Refraction Mask.
#pragma region PSO_for_Water_Refraction_Mask

		Input = Shaders[L"WaterRefractionMask"]->GetInputLayout();
		D3D12_BLEND_DESC BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALPHA;
		BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		PSOs->BuildPSO(d3dDevice.Get(),
			PSOName::WaterRefractionMask,
			RootSignature.Get(),
			{ reinterpret_cast<BYTE*>(Shaders[L"WaterRefractionMask"]->GetVSShader()->GetBufferPointer()),
				Shaders[L"WaterRefractionMask"]->GetVSShader()->GetBufferSize() },
			{ reinterpret_cast<BYTE*>(Shaders[L"WaterRefractionMask"]->GetPSShader()->GetBufferPointer()),
			Shaders[L"WaterRefractionMask"]->GetPSShader()->GetBufferSize() },
			{},
			{},
			{},
			{},
			BlendDesc,
			UINT_MAX,
			BaseRasterizerState,
			DepthStencilState,
			{ Input.data(), (UINT)Input.size() },
			{},
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			1,
			RTVFormats,
			DepthStencilFormat,
			{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
			0,
			{},
			{});

#pragma endregion
		// PSO for Shock Wave Water.
#pragma region Shock Wave Water
		Input = Shaders[L"ShockWaveWater"]->GetInputLayout();
		PSOs->BuildPSO(d3dDevice.Get(),
			PSOName::ShockWaveWater,
			RootSignature.Get(),
			{ reinterpret_cast<BYTE*>(Shaders[L"ShockWaveWater"]->GetVSShader()->GetBufferPointer()),
				Shaders[L"ShockWaveWater"]->GetVSShader()->GetBufferSize() },
			{ reinterpret_cast<BYTE*>(Shaders[L"ShockWaveWater"]->GetPSShader()->GetBufferPointer()),
			Shaders[L"ShockWaveWater"]->GetPSShader()->GetBufferSize() },
			{},
			{},
			{},
			{},
			CD3DX12_BLEND_DESC(D3D12_DEFAULT),
			UINT_MAX,
			BaseRasterizerState,
			CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
			{ Input.data(), (UINT)Input.size() },
			{},
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			1,
			RTVFormats,
			DepthStencilFormat,
			{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
			0,
			{},
			{});
#pragma endregion
	}


	// PSO for Height Map Terrain.
#pragma region Height Map Terrain
	Input = Shaders[L"HeightMapTerrain"]->GetInputLayout();
	D3D12_RASTERIZER_DESC HeightMapRasterizerState = BaseRasterizerState;
	//HeightMapRasterizerState.FillMode=D3D12_FILL_MODE_WIREFRAME;
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::HeightMapTerrain,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"HeightMapTerrain"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"HeightMapTerrain"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"HeightMapTerrain"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"HeightMapTerrain"]->GetPSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"HeightMapTerrain"]->GetDSShader()->GetBufferPointer()),
		Shaders[L"HeightMapTerrain"]->GetDSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"HeightMapTerrain"]->GetHSShader()->GetBufferPointer()),
		Shaders[L"HeightMapTerrain"]->GetHSShader()->GetBufferSize() },
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		HeightMapRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});

	HeightMapRasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::HeightMapTerrainRefraction,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"HeightMapTerrain"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"HeightMapTerrain"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"HeightMapTerrain"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"HeightMapTerrain"]->GetPSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"HeightMapTerrain"]->GetDSShader()->GetBufferPointer()),
		Shaders[L"HeightMapTerrain"]->GetDSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"HeightMapTerrain"]->GetHSShader()->GetBufferPointer()),
		Shaders[L"HeightMapTerrain"]->GetHSShader()->GetBufferSize() },
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		HeightMapRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for Grass.
#pragma region PSO_for_Grass
	Input = Shaders[L"Grass"]->GetInputLayout();
	D3D12_RASTERIZER_DESC GrassRasterizerState = BaseRasterizerState;
	GrassRasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::Grass,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"Grass"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"Grass"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"Grass"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"Grass"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{ reinterpret_cast<BYTE*>(Shaders[L"Grass"]->GetGSShader()->GetBufferPointer()),
		Shaders[L"Grass"]->GetGSShader()->GetBufferSize() },
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		GrassRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});

	GrassRasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::GrassReflection,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"Grass"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"Grass"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(Shaders[L"Grass"]->GetPSShader()->GetBufferPointer()),
		Shaders[L"Grass"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{ reinterpret_cast<BYTE*>(Shaders[L"Grass"]->GetGSShader()->GetBufferPointer()),
		Shaders[L"Grass"]->GetGSShader()->GetBufferSize() },
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		GrassRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		1,
		RTVFormats,
		DepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion

	// PSO for Grass  shadow.
#pragma region PSO_for_Grass_shadow
	D3D12_RASTERIZER_DESC GrassShadowRasterizerState = BaseRasterizerState;
	GrassShadowRasterizerState.DepthBias = 100000;
	GrassShadowRasterizerState.DepthBiasClamp = 0.0f;
	GrassShadowRasterizerState.SlopeScaledDepthBias = 1.0f;
	RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	Input = Shaders[L"GrassShadow"]->GetInputLayout();
	PSOs->BuildPSO(d3dDevice.Get(),
		PSOName::GrassShadow,
		RootSignature.Get(),
		{ reinterpret_cast<BYTE*>(Shaders[L"GrassShadow"]->GetVSShader()->GetBufferPointer()),
			Shaders[L"GrassShadow"]->GetVSShader()->GetBufferSize() },
		{},
		{},
		{},
		{ reinterpret_cast<BYTE*>(Shaders[L"GrassShadow"]->GetGSShader()->GetBufferPointer()),
		Shaders[L"GrassShadow"]->GetGSShader()->GetBufferSize() },
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		GrassShadowRasterizerState,
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		0,
		RTVFormats,
		DXGI_FORMAT_D32_FLOAT,
		{ UINT(1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


}

void Direct3D12Window::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		FrameResources.push_back(std::make_unique<FrameResource>(d3dDevice.Get(),
			2, (UINT)modelLoder->GetModelMesh(L"Blue_Tree_02a").size(), 1, (UINT)mAllRitems.size(), (UINT)Materials::GetMaterialSize(), mWaves->VertexCount()));
	}
}

void Direct3D12Window::BuildAllMaterials()
{
	simpleGeoInstance->BuildMaterials();
	modelLoder->BuildMaterials();
	mHeightMapTerrain->BuildMaterials();
	sky->BuildMaterials();
}

void Direct3D12Window::BuildRenderItems()
{
	//index
	int ObjCBIndex = 0;


	//Land 
	{
		RenderItem LandRitem = {};
		LandRitem.World = MathHelper::Identity4x4();
		XMStoreFloat4x4(&LandRitem.World, XMMatrixTranslation(0.0f, 1.0f, 0.0f) * XMMatrixScaling(1.0, 1.0, 1.0));
		XMStoreFloat4x4(&LandRitem.TexTransform, XMMatrixScaling(2.0, 2.0, 2.0));
		LandRitem.ObjCBIndex = ObjCBIndex++;
		LandRitem.Mat = Materials::GetMaterialInstance()->GetMaterial(L"Terrain/grass");
		LandRitem.Geo.push_back(mGeometries[L"landGeo"].get());
		LandRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		LandRitem.IndexCount.push_back(LandRitem.Geo[0]->DrawArgs[L"grid"].IndexCount);
		LandRitem.StartIndexLocation.push_back(LandRitem.Geo[0]->DrawArgs[L"grid"].StartIndexLocation);
		LandRitem.BaseVertexLocation.push_back(LandRitem.Geo[0]->DrawArgs[L"grid"].BaseVertexLocation);
		LandRitem.InstanceCount = 1;
		LandRitem.NumFramesDirty = gNumFrameResources;

#pragma region Reflection
		RenderItem ReflectionLandRitem = LandRitem;
		ReflectionLandRitem.ItemType = RenderItemType::Reflection;
		XMStoreFloat4x4(&ReflectionLandRitem.World, XMMatrixScaling(1.0, -1.0, 1.0));
		ReflectionLandRitem.ObjCBIndex = ObjCBIndex++;
#pragma endregion

		auto Land = std::make_unique<RenderItem>(LandRitem);
		auto ReflectionLand = std::make_unique<RenderItem>(LandRitem);
		mRitemLayer[(int)RenderLayer::Opaque].push_back(Land.get());
		mReflectionWaterLayer[(int)RenderLayer::Opaque].push_back(ReflectionLand.get());
		mAllRitems.push_back(std::move(Land));
		mAllRitems.push_back(std::move(ReflectionLand));
	}


	//Height map terrain
	{
		RenderItem TerrainRitem = {};
		TerrainRitem.World = MathHelper::Identity4x4();
		XMStoreFloat4x4(&TerrainRitem.World, XMMatrixTranslation(0.0f, 0.0f, 0.0f) * XMMatrixScaling(1.0, 1.0, 1.0));
		XMStoreFloat4x4(&TerrainRitem.TexTransform, XMMatrixScaling(1.0, 1.0, 1.0));
		TerrainRitem.ObjCBIndex = ObjCBIndex++;
		TerrainRitem.Mat = Materials::GetMaterialInstance()->GetMaterial(L"Terrain/grass");
		TerrainRitem.Geo.push_back(mHeightMapTerrain->GetMeshGeometry());
		TerrainRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
		TerrainRitem.IndexCount.push_back(TerrainRitem.Geo[0]->DrawArgs[L"HeightMapTerrain"].IndexCount);
		TerrainRitem.StartIndexLocation.push_back(TerrainRitem.Geo[0]->DrawArgs[L"HeightMapTerrain"].StartIndexLocation);
		TerrainRitem.BaseVertexLocation.push_back(TerrainRitem.Geo[0]->DrawArgs[L"HeightMapTerrain"].BaseVertexLocation);
		TerrainRitem.InstanceCount = 1;
		TerrainRitem.NumFramesDirty = gNumFrameResources;

#pragma region Reflection
		RenderItem ReflectionTerrainRitem = TerrainRitem;
		ReflectionTerrainRitem.ItemType = RenderItemType::Reflection;
		XMStoreFloat4x4(&ReflectionTerrainRitem.World, XMMatrixTranslation(0.0f, 0.0f, 0.0f) * XMMatrixScaling(1.0, -1.0, 1.0));
		ReflectionTerrainRitem.ObjCBIndex = ObjCBIndex++;
#pragma endregion

		auto Terrain = std::make_unique<RenderItem>(TerrainRitem);
		auto ReflectionTerrain = std::make_unique<RenderItem>(ReflectionTerrainRitem);
		mRitemLayer[(int)RenderLayer::HeightMapTerrain].push_back(Terrain.get());
		mReflectionWaterLayer[(int)RenderLayer::HeightMapTerrain].push_back(ReflectionTerrain.get());
		mAllRitems.push_back(std::move(Terrain));
		mAllRitems.push_back(std::move(ReflectionTerrain));
	}

	//Grass
	{
		RenderItem GrassRitem = {};
		GrassRitem.World = MathHelper::Identity4x4();
		XMStoreFloat4x4(&GrassRitem.World, XMMatrixScaling(1.0, 1.0, 1.0));
		XMStoreFloat4x4(&GrassRitem.TexTransform, XMMatrixScaling(1.0, 1.0, 1.0));
		GrassRitem.ObjCBIndex = ObjCBIndex++;
		GrassRitem.Mat = Materials::GetMaterialInstance()->GetMaterial(L"PBRGrass");
		GrassRitem.Geo.push_back(mHeightMapTerrain->GetGrass()->GetMeshGeometry());
		GrassRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		GrassRitem.IndexCount.push_back(GrassRitem.Geo[0]->DrawArgs[L"Grass"].IndexCount);
		GrassRitem.VertexCount.push_back(GrassRitem.Geo[0]->DrawArgs[L"Grass"].VertexCount);
		GrassRitem.StartIndexLocation.push_back(GrassRitem.Geo[0]->DrawArgs[L"Grass"].StartIndexLocation);
		GrassRitem.BaseVertexLocation.push_back(GrassRitem.Geo[0]->DrawArgs[L"Grass"].BaseVertexLocation);
		GrassRitem.InstanceCount = 1;
		GrassRitem.NumFramesDirty = gNumFrameResources;

#pragma region Reflection
		RenderItem ReflectionGrassRitem = GrassRitem;
		ReflectionGrassRitem.ItemType = RenderItemType::Reflection;
		XMStoreFloat4x4(&ReflectionGrassRitem.World, XMMatrixScaling(1.0, -1.0, 1.0));
		ReflectionGrassRitem.ObjCBIndex = ObjCBIndex++;
#pragma endregion

		auto Grass = std::make_unique<RenderItem>(GrassRitem);
		auto ReflectionGrass = std::make_unique<RenderItem>(ReflectionGrassRitem);
		mRitemLayer[(int)RenderLayer::Grass].push_back(Grass.get());
		mReflectionWaterLayer[(int)RenderLayer::Grass].push_back(ReflectionGrass.get());
		mAllRitems.push_back(std::move(Grass));
		mAllRitems.push_back(std::move(ReflectionGrass));
	}


	//Shock Wave Water
	{
		RenderItem ShockWaveWaterRitem = {};
		ShockWaveWaterRitem.World = MathHelper::Identity4x4();
		XMStoreFloat4x4(&ShockWaveWaterRitem.World, XMMatrixScaling(20.48f, 1.0f, 20.48f));
		XMStoreFloat4x4(&ShockWaveWaterRitem.TexTransform, XMMatrixScaling(5.0f, 5.0f, 5.0f));
		ShockWaveWaterRitem.ObjCBIndex = ObjCBIndex++;
		ShockWaveWaterRitem.Mat = Materials::GetMaterialInstance()->GetMaterial(L"PBRharshbricks");
		ShockWaveWaterRitem.Geo.push_back(mGeometries[L"landGeo"].get());
		ShockWaveWaterRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ShockWaveWaterRitem.IndexCount.push_back(ShockWaveWaterRitem.Geo[0]->DrawArgs[L"grid"].IndexCount);
		ShockWaveWaterRitem.StartIndexLocation.push_back(ShockWaveWaterRitem.Geo[0]->DrawArgs[L"grid"].StartIndexLocation);
		ShockWaveWaterRitem.BaseVertexLocation.push_back(ShockWaveWaterRitem.Geo[0]->DrawArgs[L"grid"].BaseVertexLocation);
		ShockWaveWaterRitem.InstanceCount = 1;
		ShockWaveWaterRitem.NumFramesDirty = gNumFrameResources;
		
		auto ShockWaveWater = std::make_unique<RenderItem>(ShockWaveWaterRitem);
		mRitemLayer[(int)RenderLayer::ShockWaveWater].push_back(ShockWaveWater.get());
		mAllRitems.push_back(std::move(ShockWaveWater));
	}


	//Sky Box
	{
		RenderItem skyRitem = {};
		XMStoreFloat4x4(&skyRitem.World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
		skyRitem.TexTransform = MathHelper::Identity4x4();
		skyRitem.ObjCBIndex = ObjCBIndex++;
		skyRitem.Mat = Materials::GetMaterialInstance()->GetMaterial(L"sky");
		skyRitem.Geo.push_back(mGeometries[L"Sky"].get());
		skyRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		skyRitem.IndexCount.push_back(skyRitem.Geo[0]->DrawArgs[L"Sphere"].IndexCount);
		skyRitem.StartIndexLocation.push_back(skyRitem.Geo[0]->DrawArgs[L"Sphere"].StartIndexLocation);
		skyRitem.BaseVertexLocation.push_back(skyRitem.Geo[0]->DrawArgs[L"Sphere"].BaseVertexLocation);
		skyRitem.InstanceCount = 1;
		skyRitem.NumFramesDirty = gNumFrameResources;

#pragma region Reflection
		RenderItem reflectionWaterskyRitem = skyRitem;
		reflectionWaterskyRitem.ItemType = RenderItemType::Reflection;
		XMStoreFloat4x4(&reflectionWaterskyRitem.World, XMMatrixScaling(1.0f, -1.0f, 1.0f) * XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
		reflectionWaterskyRitem.ObjCBIndex = ObjCBIndex++;
#pragma endregion

		auto ReflectionWaterskyRitem = std::make_unique<RenderItem>(reflectionWaterskyRitem);
		auto SkyRitem = std::make_unique<RenderItem>(skyRitem);
		mRitemLayer[(int)RenderLayer::Sky].push_back(SkyRitem.get());
		mReflectionWaterLayer[(int)RenderLayer::Sky].push_back(ReflectionWaterskyRitem.get());
		mAllRitems.push_back(std::move(SkyRitem));
		mAllRitems.push_back(std::move(ReflectionWaterskyRitem));
	}


}

void Direct3D12Window::BuildModelGeoInstanceItems()
{
	// MODEL TEST CODE
#pragma region MODEL_TEST_CODE

	BoundingBox lbox;
	lbox.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	lbox.Extents = XMFLOAT3(200.0f, 200.0f, 200.0f);

	RenderItem InstanceModelRitem = {};
	for (auto& e : modelLoder->GetModelMesh(L"Blue_Tree_02a"))
	{
		InstanceModelRitem.Geo.push_back(&e.mMeshGeo);
		InstanceModelRitem.IndexCount.push_back(e.mMeshGeo.DrawArgs[L"Model Mesh"].IndexCount);
		InstanceModelRitem.StartIndexLocation.push_back(e.mMeshGeo.DrawArgs[L"Model Mesh"].StartIndexLocation);
		InstanceModelRitem.BaseVertexLocation.push_back(e.mMeshGeo.DrawArgs[L"Model Mesh"].BaseVertexLocation);
	}

	InstanceModelRitem.World = MathHelper::Identity4x4();
	InstanceModelRitem.TexTransform = MathHelper::Identity4x4();
	InstanceModelRitem.ObjCBIndex = -1;//not important in instance item
	InstanceModelRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	InstanceModelRitem.Bounds = lbox;

	// Generate instance data.
	///Hard code
	const int n = 5;
	InstanceModelRitem.Instances.resize(n * n);
	InstanceModelRitem.InstanceCount = 0;//init count

	float width = 50.0f;
	float height = 50.0f;

	float x = -0.5f * width - 200;
	float y = -0.5f * height - 200;

	float dx = width / (n - 1);
	float dy = height / (n - 1);

	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < n; ++j)
		{
			int index = i * n + j;
			// Position instanced along a 3D grid.
			XMFLOAT4X4 M(
				0.1f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.1f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.1f, 0.0f,
				x + j * dx, mHeightMapTerrain->GetHeight(x + j * dx, y + i * dy), y + i * dy, 1.0f);
			XMStoreFloat4x4(&InstanceModelRitem.Instances[index].World, /*XMMatrixRotationX(MathHelper::Pi/2)**/XMMatrixRotationY(MathHelper::RandF() * MathHelper::Pi) * XMLoadFloat4x4(&M));

			InstanceModelRitem.Instances[index].TexTransform = MathHelper::Identity4x4();
		}
	}
	//Reflection Instance Model items
#pragma region Reflection
	RenderItem ReflectionInstanceModelRitem = InstanceModelRitem;
	ReflectionInstanceModelRitem.ItemType = RenderItemType::Reflection;
	XMMATRIX WaterMAT;
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < n; ++j)
		{
			int index = i * n + j;
			WaterMAT = XMLoadFloat4x4(&ReflectionInstanceModelRitem.Instances[index].World);
			XMStoreFloat4x4(&ReflectionInstanceModelRitem.Instances[index].World, WaterMAT * XMMatrixScaling(1.0f, -1.0f, 1.0f));
		}
	}
#pragma endregion

	auto InstanceModel = std::make_unique<RenderItem>(InstanceModelRitem);
	auto ReflectionInstanceModel = std::make_unique<RenderItem>(ReflectionInstanceModelRitem);
	mRitemLayer[(int)RenderLayer::InstanceSimpleItems].push_back(InstanceModel.get());
	mReflectionWaterLayer[(int)RenderLayer::InstanceSimpleItems].push_back(ReflectionInstanceModel.get());

	mAllRitems.push_back(std::move(InstanceModel));
	mAllRitems.push_back(std::move(ReflectionInstanceModel));

#pragma endregion
}

void Direct3D12Window::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems, bool IsIndexInstanceDraw)
{
	auto objectCB = CurrFrameResource->SimpleObjectCB->Resource();
	auto skinnedCB = CurrFrameResource->SkinnedCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress;
		if (ri->ObjCBIndex >= 0)
		{
			objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * mObjCBByteSize;
		}
		else//instance draw call
		{
			objCBAddress = objectCB->GetGPUVirtualAddress();
		}
		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);


		for (int MeshNum = 0; MeshNum < ri->Geo.size(); ++MeshNum)
		{
			D3D12_VERTEX_BUFFER_VIEW Views;
			if (ri->Geo.size() > 1)//test
			{
				D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress();
				cmdList->SetGraphicsRootConstantBufferView(2, skinnedCBAddress);
				//const D3D12_VERTEX_BUFFER_VIEW* pViews[] = { &ri->Geo[MeshNum]->VertexBufferView(),&mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].mBoneMeshs[MeshNum].mBoneGeo.VertexBufferView() };
				Views = ri->Geo[MeshNum]->VertexBufferView();
				cmdList->IASetVertexBuffers(0, 1, &Views);
				Views = modelLoder->mAnimations[L"TraumaGuard"][L"ActiveIdleLoop"].mBoneMeshs[MeshNum].mBoneGeo.VertexBufferView();
				cmdList->IASetVertexBuffers(1, 1, &Views);
			}
			else
			{
				Views = ri->Geo[MeshNum]->VertexBufferView();
				cmdList->IASetVertexBuffers(0, 1, &Views);
			}

			cmdList->IASetPrimitiveTopology(ri->PrimitiveType);


			// Set the instance buffer to use for this render-item.  For structured buffers, we can bypass 
			// the heap and set as a root descriptor.
			ID3D12Resource* instanceBuffer = nullptr;
			if (ritems[i]->ItemType == RenderItemType::Normal)
			{
				instanceBuffer = CurrFrameResource->InstanceSimpleObjectCB[MeshNum]->Resource();
			}
			else if (ritems[i]->ItemType == RenderItemType::Reflection)
			{
				instanceBuffer = CurrFrameResource->ReflectionInstanceSimpleObjectCB[MeshNum]->Resource();
			}
			else {}

			CommandList->SetGraphicsRootShaderResourceView(3, instanceBuffer->GetGPUVirtualAddress());

			if (IsIndexInstanceDraw)
			{
				D3D12_INDEX_BUFFER_VIEW View = ri->Geo[MeshNum]->IndexBufferView();
				cmdList->IASetIndexBuffer(&View);
				cmdList->DrawIndexedInstanced(ri->IndexCount[MeshNum], ri->InstanceCount, ri->StartIndexLocation[MeshNum], ri->BaseVertexLocation[MeshNum], 0);
			}
			else
			{
				cmdList->DrawInstanced(ri->VertexCount[MeshNum], ri->InstanceCount, ri->BaseVertexLocation[MeshNum], 0);
			}
		}
	}
}

float Direct3D12Window::GetHillsHeight(float x, float z)const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

void Direct3D12Window::DrawShockWaveWater()
{
	DrawWaterReflectionMap();

	DrawWaterRefractionMap();


	CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::ShockWaveWater).Get());
	D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetDescriptors = MSAARTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilDescriptor = DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	CommandList->OMSetRenderTargets(1, &RenderTargetDescriptors, false, &DepthStencilDescriptor);
	D3D12_RESOURCE_BARRIER barriers[1] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(
			MSAARenderTargetBuffer.Get(),
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET)
	};
	CommandList->ResourceBarrier(1, barriers);

	//Draw Shock Wave Water
	PIXBeginEvent(CommandList.Get(), 0, "Draw Water::RenderLayer::ShockWaveWater");
	DrawRenderItems(CommandList.Get(), mRitemLayer[(int)RenderLayer::ShockWaveWater]);
	PIXEndEvent(CommandList.Get());
}

void Direct3D12Window::DrawWaterReflectionMap()
{
	// Clear the back buffer and depth buffer.
	//test Reflection sence no shadow
	{
		CommandList->ClearDepthStencilView(shadowMap->Dsv(),
			D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		Direct3D12Window::RedrawShadowMap = true;
	}

	if (m4xMsaaState)//MSAA
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mShockWaveWater->ReflectionRTV(),
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		CommandList->ResourceBarrier(1, &barrier);

		auto RTVDescriptor = mShockWaveWater->ReflectionDescriptor();
		auto DSVDescriptor = mShockWaveWater->ReflectionDepthMapDSVDescriptor();

		CommandList->OMSetRenderTargets(1, &RTVDescriptor, false, &DSVDescriptor);

		CommandList->ClearRenderTargetView(RTVDescriptor, Colors::Transparent, 0, nullptr);
		CommandList->ClearDepthStencilView(DSVDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}
	else
	{
		//not do anythings
	}

	//Draw Render Items (Opaque)
	if (EngineUI.IsShowLand())
	{
		CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::Opaque).Get());
		PIXBeginEvent(CommandList.Get(), 0, "Draw Water::RenderLayer::Opaque");
		DrawRenderItems(CommandList.Get(), mReflectionWaterLayer[(int)RenderLayer::Opaque]);
		PIXEndEvent(CommandList.Get());
	}
	//Draw Instanse 
	if (EngineUI.IsShowModel())
	{
		CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::SkinAnime).Get());
		PIXBeginEvent(CommandList.Get(), 0, "Draw Water::RenderLayer::InstanceSimpleItems");
		DrawRenderItems(CommandList.Get(), mReflectionWaterLayer[(int)RenderLayer::InstanceSimpleItems]);
		PIXEndEvent(CommandList.Get());
	}
	//Draw Height Map Terrain
	if (EngineUI.IsShowTerrain())
	{
		DrawHeightMapTerrain(true);
	}

	//Draw Grass
	if (EngineUI.IsShowGrass())
	{
		DrawGrass(true);
	}

	//Draw Sky box
	if (EngineUI.IsShowSky())
	{
		CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::Sky).Get());
		PIXBeginEvent(CommandList.Get(), 0, "Draw Water::RenderLayer::Sky");
		DrawRenderItems(CommandList.Get(), mReflectionWaterLayer[(int)RenderLayer::Sky]);
		PIXEndEvent(CommandList.Get());
	}

	D3D12_RESOURCE_BARRIER barriers[1] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(
			mShockWaveWater->ReflectionRTV(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
	};
	CommandList->ResourceBarrier(1, barriers);
	CommandList->ResolveSubresource(mShockWaveWater->SingleReflectionSRV(), 0, mShockWaveWater->ReflectionRTV(), 0, BackBufferFormat);
}

void Direct3D12Window::DrawWaterRefractionMap()
{
	auto rtvDescriptor = MSAARTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvDescriptor = DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	CommandList->OMSetRenderTargets(1, &rtvDescriptor, false, &dsvDescriptor);

	//Draw Shock Wave Water
	CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::WaterRefractionMask).Get());
	PIXBeginEvent(CommandList.Get(), 0, "Draw Main::RenderLayer::WaterRefractionMask");
	DrawRenderItems(CommandList.Get(), mRitemLayer[(int)RenderLayer::ShockWaveWater]);
	PIXEndEvent(CommandList.Get());

	D3D12_RESOURCE_BARRIER barriers[1] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(
			MSAARenderTargetBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
	};
	CommandList->ResourceBarrier(1, barriers);

	CommandList->ResolveSubresource(mShockWaveWater->SingleRefractionSRV(), 0, MSAARenderTargetBuffer.Get(), 0, BackBufferFormat);
}

void Direct3D12Window::DrawHeightMapTerrain(bool IsReflection)
{
	auto TerrainCB = CurrFrameResource->TerrainCB->Resource();

	if (IsReflection)
	{
		CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::HeightMapTerrainRefraction).Get());
		D3D12_GPU_VIRTUAL_ADDRESS TerrainCBAddress = TerrainCB->GetGPUVirtualAddress() + mTerrainCBByteSize;
		CommandList->SetGraphicsRootConstantBufferView(2, TerrainCBAddress);

		PIXBeginEvent(CommandList.Get(), 0, "Draw Reflection Height Map Terrain");
		DrawRenderItems(CommandList.Get(), mReflectionWaterLayer[(int)RenderLayer::HeightMapTerrain]);
		PIXEndEvent(CommandList.Get());
	}
	else
	{
		CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::HeightMapTerrain).Get());
		D3D12_GPU_VIRTUAL_ADDRESS TerrainCBAddress = TerrainCB->GetGPUVirtualAddress();
		CommandList->SetGraphicsRootConstantBufferView(2, TerrainCBAddress);

		PIXBeginEvent(CommandList.Get(), 0, "Draw Height Map Terrain");
		DrawRenderItems(CommandList.Get(), mRitemLayer[(int)RenderLayer::HeightMapTerrain]);
		PIXEndEvent(CommandList.Get());
	}
}

void Direct3D12Window::DrawGrass(bool IsReflection)
{
	auto TerrainCB = CurrFrameResource->TerrainCB->Resource();

	if (!IsReflection)
	{
		CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::Grass).Get());
		D3D12_GPU_VIRTUAL_ADDRESS TerrainCBAddress = TerrainCB->GetGPUVirtualAddress();
		CommandList->SetGraphicsRootConstantBufferView(2, TerrainCBAddress);
		PIXBeginEvent(CommandList.Get(), 0, "Draw Grass");
		DrawRenderItems(CommandList.Get(), mRitemLayer[(int)RenderLayer::Grass], false);
		PIXEndEvent(CommandList.Get());
	}
	else
	{
		CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::GrassReflection).Get());
		D3D12_GPU_VIRTUAL_ADDRESS TerrainCBAddress = TerrainCB->GetGPUVirtualAddress() + mTerrainCBByteSize;
		CommandList->SetGraphicsRootConstantBufferView(2, TerrainCBAddress);
		PIXBeginEvent(CommandList.Get(), 0, "Draw Reflection Grass");
		DrawRenderItems(CommandList.Get(), mReflectionWaterLayer[(int)RenderLayer::Grass], false);
		PIXEndEvent(CommandList.Get());
	}
}

HeightmapTerrain::InitInfo Direct3D12Window::HeightmapTerrainInit()
{
	HeightmapTerrain::InitInfo TerrainInfo;
	TerrainInfo.HeightMapFilename = "../Resource/Textures/Terrain/heightmap.png";
	TerrainInfo.LayerMapFilename[0] = "Terrain/grass";
	TerrainInfo.LayerMapFilename[1] = "Terrain/darkdirt";
	TerrainInfo.LayerMapFilename[2] = "Terrain/stone";
	TerrainInfo.LayerMapFilename[3] = "Terrain/lightdirt";
	TerrainInfo.LayerMapFilename[4] = "Terrain/snow";
	TerrainInfo.BlendMapFilename = "Terrain/blend";
	TerrainInfo.HeightScale = 1000.0f;
	TerrainInfo.HeightmapWidth = 2048;
	TerrainInfo.HeightmapHeight = 2048;
	TerrainInfo.CellSpacing = 10.0f;

	return TerrainInfo;
}

float Direct3D12Window::AspectRatio() const
{
	return static_cast<float>(i_Width) / i_Height;
}


//Samplers
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> Direct3D12Window::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

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
		D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow
	};
}

void Direct3D12Window::LoadModel()
{
	modelLoder->LoadModel(L"BlueTree/Blue_Tree_03a.fbx");
	modelLoder->LoadModel(L"BlueTree/Blue_Tree_03b.fbx");
	modelLoder->LoadModel(L"BlueTree/Blue_Tree_03c.fbx");
	modelLoder->LoadModel(L"BlueTree/Blue_Tree_03d.fbx");
	modelLoder->LoadModel(L"BlueTree/Blue_Tree_02a.fbx");
	modelLoder->LoadModel(L"TraumaGuard/TraumaGuard.fbx");

	//AnimationPlayback["TraumaGuard@ActiveIdleLoop"].OnInitialize();
	//mModelLoder->LoadAnimation("TraumaGuard/TraumaGuard@ActiveIdleLoop.fbx");

}

void Direct3D12Window::UpdateShadowPassCB()
{
	XMMATRIX View = XMLoadFloat4x4(&shadowMap->mLightView);
	XMMATRIX Proj = XMLoadFloat4x4(&shadowMap->mLightProj);

	XMVECTOR Determinant;
	XMMATRIX ViewProj = XMMatrixMultiply(View, Proj);
	Determinant = XMMatrixDeterminant(View);
	XMMATRIX InvView = XMMatrixInverse(&Determinant, View);
	Determinant = XMMatrixDeterminant(Proj);
	XMMATRIX InvProj = XMMatrixInverse(&Determinant, Proj);
	Determinant = XMMatrixDeterminant(ViewProj);
	XMMATRIX InvViewProj = XMMatrixInverse(&Determinant, ViewProj);


	UINT w = shadowMap->Width();
	UINT h = shadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(View));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(InvView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(Proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(InvProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(ViewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(InvViewProj));
	mShadowPassCB.EyePosW = shadowMap->mLightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = shadowMap->mLightNearZ;
	mShadowPassCB.FarZ = shadowMap->mLightFarZ;

	auto currPassCB = CurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void Direct3D12Window::UpdateTerrainPassCB()
{
	XMMATRIX viewProj = Camera.GetView() * Camera.GetProj();

	XMFLOAT4 worldPlanes[6];
	ExtractFrustumPlanes(worldPlanes, viewProj);


	mHeightMapTerrain->GetTerrainConstant(mTerrainConstant);

	for (int i = 0; i < 6; ++i)
	{
		mTerrainConstant.gWorldFrustumPlanes[i] = worldPlanes[i];
	}

	mTerrainConstant.isReflection = false;
	mTerrainConstant.gBlendMapIndex = mBlendMapIndex;
	mTerrainConstant.gHeightMapIndex = mHeightMapIndex;
	mTerrainConstant.gRGBNoiseMapIndex = mRGBNoiseMapIndex;
	mTerrainConstant.gMinWind = EngineUI.GetGrassMinWind();
	mTerrainConstant.gMaxWind = EngineUI.GetGrassMaxWind();
	mTerrainConstant.gGrassColor = EngineUI.GetGrassColor();
	mTerrainConstant.gPerGrassHeight = EngineUI.GetPerGrassHeight();
	mTerrainConstant.gPerGrassWidth = EngineUI.GetPerGrassWidth();
	mTerrainConstant.isGrassRandom = EngineUI.IsGrassRandom();
	mTerrainConstant.gWaterTransparent = EngineUI.GetWaterTransparent();

	auto currPassCB = CurrFrameResource->TerrainCB.get();
	currPassCB->CopyData(0, mTerrainConstant);
	//地形的水面反射
	mTerrainConstant.isReflection = true;
	currPassCB->CopyData(1, mTerrainConstant);
}

void Direct3D12Window::UpdateAnimation()
{
	auto currSkinnedCB = CurrFrameResource->SkinnedCB.get();
	static float time = 0.0f;
	double time_in_sec = mTimer.TotalTime();
	time += mTimer.DeltaTime();
	static int j = 0;
	/*if (time > 0.3)
	{*/
	//transforms = AnimationPlayback["TraumaGuard@ActiveIdleLoop"].GetModelTransforms();
	time = 0;
	//}



	/*if (time >= 3.0f)
	{*/
	modelLoder->mAnimations[L"TraumaGuard"][L"ActiveIdleLoop"].UpadateBoneTransform(time_in_sec, transforms);
	//mModelLoder->mAnimations[L"TraumaGuard"][L"ActiveIdleLoop"].Calculate(time_in_sec);
	//time -= 3.0f;
	//mModelLoder->mAnimations[L"TraumaGuard"][L"ActiveIdleLoop"].GetBoneMatrices(mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].pAnimeScene->mRootNode,x);
	/*int i = 0;
	for (auto e : mModelLoder->mAnimations[L"TraumaGuard"][L"ActiveIdleLoop"].mTransforms)
	{
		transforms[i++]=(mModelLoder->mAnimations[L"TraumaGuard"][L"ActiveIdleLoop"].AiToXM(e));
	}*/

	SkinnedConstants skinnedConstants;
	std::copy(
		std::begin(transforms),
		std::end(transforms),
		&skinnedConstants.BoneTransforms[0]);

	currSkinnedCB->CopyData(0, skinnedConstants);

	//}


}

void Direct3D12Window::CreateDirectDevice()
{
	//加载渲染管道依赖项。
	UINT dxgiFactoryFlags = 0;

#if defined(DEBUG) || defined(_DEBUG) 
	// 启用D3D12调试层。
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();

		// Enable additional debug layers.
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	// 创建工厂
	// 使用DXGI 1.1工厂生成枚举适配器，创建交换链以及将窗口与alt + enter键序列相关联的对象，
	// 以便切换到全屏显示模式和从全屏显示模式切换。
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&DXGIFactory)));

	// 枚举适配器，并选择合适的适配器来创建3D设备对象
	for (UINT adapterIndex = 1;
		DXGI_ERROR_NOT_FOUND != DXGIFactory->EnumAdapters1(adapterIndex, &DeviceAdapter);
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc = {};
		DeviceAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{//跳过软件虚拟适配器设备
			continue;
		}
		//检查适配器对D3D支持的兼容级别，这里直接要求支持12.1的能力，注意返回接口的那个参数被置为了nullptr，这样
		//就不会实际创建一个设备了，也不用我们啰嗦的再调用release来释放接口。这也是一个重要的技巧，请记住！
		if (SUCCEEDED(D3D12CreateDevice(DeviceAdapter.Get(),
			D3D_FEATURE_LEVEL_12_1,
			_uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	// 尝试创建D3D硬件设备
	HRESULT hardwareResult = D3D12CreateDevice(
		DeviceAdapter.Get(),                        // 默认适配器
		D3D_FEATURE_LEVEL_12_0,                     // 最小 feature level
		IID_PPV_ARGS(&d3dDevice));

	//获取各种视图大小
	RtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DsvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CbvSrvUavDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);



	// 检查4X MSAA质量对我们的后缓冲区格式的支持。
	// 所有支持Direct3D 11的设备对所有渲染目标格式都支持4X MSAA，
	// 因此我们只需要检查质量支持。
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = BackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_TILED_RESOURCE;
	msQualityLevels.NumQualityLevels = 1;
	ThrowIfFailed(d3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "意外的MSAA质量水平。");

#ifdef _DEBUG
	LogAdapters();
#endif
}

void Direct3D12Window::CreateCommandQueueAndSwapChain()
{
	//描述并创建命令队列。
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&CommandQueue)));//创建命令队列

	ThrowIfFailed(d3dDevice->CreateCommandAllocator(										//创建命令分配器
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(DirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(d3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		DirectCmdListAlloc.Get(), // 关联的命令分配器
		nullptr,                   // 初始的管道状态对象
		IID_PPV_ARGS(CommandList.GetAddressOf())));

	//在关闭状态下开始。 这是因为我们第一次引用命令列表时会将其重置，并且需要在调用Reset之前将其关闭。
	CommandList->Close();

	// 释放我们将重新创建的先前交换链。
	SwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = i_Width;
	sd.BufferDesc.Height = i_Height;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = BackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = i_hWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	//注意：交换链使用队列执行刷新。
	ThrowIfFailed(DXGIFactory->CreateSwapChain(
		CommandQueue.Get(),
		&sd,
		SwapChain.GetAddressOf()));
}

void Direct3D12Window::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;//渲染目标视图的堆描述
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(RTVDescriptorHeap.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;//深度视图的堆描述
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(DSVDescriptorHeap.GetAddressOf())));


	// Create descriptor heaps for MSAA render target viewsand depth stencil views.
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
		rtvDescriptorHeapDesc.NumDescriptors = 1;
		rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

		ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc,
			IID_PPV_ARGS(MSAARTVDescriptorHeap.ReleaseAndGetAddressOf())));
	}
}

void Direct3D12Window::DrawSceneToShadowMap()
{
	D3D12_VIEWPORT Viewports = shadowMap->Viewport();
	CommandList->RSSetViewports(1, &Viewports);
	D3D12_RECT Rects = shadowMap->ScissorRect();
	CommandList->RSSetScissorRects(1, &Rects);

	UINT passCBByteSize = EngineUtility::CalcConstantBufferByteSize(sizeof(PassConstants));


	// Set null render target because we are only going to draw to
	// depth buffer.  Setting a null render target will disable color writes.
	// Note the active PSO also must specify a render target count of 0.
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilDescriptor = shadowMap->Dsv();
	CommandList->OMSetRenderTargets(0, nullptr, false, &DepthStencilDescriptor);

	// Clear the back buffer and depth buffer.
	CommandList->ClearDepthStencilView(shadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);



	// Bind the pass constant buffer for the shadow map pass.
	auto passCB = CurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	CommandList->SetGraphicsRootConstantBufferView(1, passCBAddress);

	//Simple Instance  Shadow map
	//mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::InstanceSimpleShadow_Opaque).Get());
	PIXBeginEvent(CommandList.Get(), 0, "Draw Shadow::RenderLayer::InstanceSimpleItems");
	//DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::InstanceSimpleItems]);
	PIXEndEvent(CommandList.Get());

	PIXBeginEvent(CommandList.Get(), 0, "Draw Shadow::RenderLayer::Opaque");
	//DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
	PIXEndEvent(CommandList.Get());

	PIXBeginEvent(CommandList.Get(), 0, "Draw Shadow::RenderLayer::Terrain");
	//DrawHeightMapTerrain(gr);
	PIXEndEvent(CommandList.Get());


	PIXBeginEvent(CommandList.Get(), 0, "Draw Grass Shadow");
	auto TerrainCB = CurrFrameResource->TerrainCB->Resource();
	CommandList->SetPipelineState(PSOs.get()->GetPSO(PSOName::GrassShadow).Get());
	D3D12_GPU_VIRTUAL_ADDRESS TerrainCBAddress = TerrainCB->GetGPUVirtualAddress();
	CommandList->SetGraphicsRootConstantBufferView(2, TerrainCBAddress);
	DrawRenderItems(CommandList.Get(), mRitemLayer[(int)RenderLayer::Grass], false);
	PIXEndEvent(CommandList.Get());
}
