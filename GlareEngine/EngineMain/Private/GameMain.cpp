#include "GameMain.h"
//lib
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")



#define ShadowMapSize 2048

const int gNumFrameResources = 3;

bool GameApp::RedrawShadowMap = true;

//[System::STAThreadAttribute]
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		GameApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

GameApp::GameApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
	mEngineUI = make_unique<EngineGUI>();
	transforms.resize(96);
}

GameApp::~GameApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();

	for (auto& e : mShaders)
	{
		delete e.second;
		e.second = nullptr;
	}


	
}

bool GameApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	mLastMousePos.x = mClientWidth / 2;
	mLastMousePos.y = mClientHeight / 2;

	// 将命令列表重置为准备初始化命令。
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// 获取此堆类型中描述符的增量大小。 这是特定于硬件的，因此我们必须查询此信息。
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//texture manage
	mTextureManage = std::make_unique<L3DTextureManage>(md3dDevice.Get(), mCommandList.Get());
	//pso init
	mPSOs = std::make_unique<PSO>();
	//wave :not use
	mWaves = std::make_unique<Waves>(256,256, 4.0f, 0.03f, 8.0f, 0.2f);
	//ShockWaveWater
	mShockWaveWater = std::make_unique<ShockWaveWater>(md3dDevice.Get(), mClientWidth, mClientHeight,m4xMsaaState, mTextureManage.get());
	//camera
	mCamera.LookAt(XMFLOAT3(20.0f, 20.0f, 20.0f), XMFLOAT3(0.0f,0.0f,0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
	//sky
	mSky= std::make_unique<Sky>(md3dDevice.Get(), mCommandList.Get(),5.0f,20,20);
	//shadow map
	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), ShadowMapSize, ShadowMapSize);
	//Simple Geometry Instance Draw
	mSimpleGeoInstance = std::make_unique<SimpleGeoInstance>(mCommandList.Get(), md3dDevice.Get());
	//init model loader
	mModelLoder = std::make_unique<ModelLoader>(mhMainWnd, md3dDevice.Get(), mCommandList.Get(), mTextureManage.get());
	//init HeightMap Terrain
	mHeightMapTerrain = std::make_unique<HeightmapTerrain>(md3dDevice.Get(), mCommandList.Get(), mTextureManage.get(),HeightmapTerrainInit(),nullptr);
	
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
	mEngineUI->InitGUI(mhMainWnd, md3dDevice.Get(), mGUISrvDescriptorHeap.Get());
	
	// 执行初始化命令。
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 等待初始化完成。
	FlushCommandQueue();

	return true;
}

void GameApp::OnResize()
{
	D3DApp::OnResize();

	//窗口调整大小，因此更新宽高比并重新计算投影矩阵;
	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	//窗口调整大小重新计算视锥包围体。
	BoundingFrustum::CreateFromMatrix(mCamFrustum, mCamera.GetProj());
	//Shock Wave Water map resize
	if (mShockWaveWater != nullptr)
	{
		mShockWaveWater->OnResize(mClientWidth, mClientHeight);
	}
}

void GameApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	//UpdateCamera(gt);

	// 循环遍历循环框架资源数组。
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// GPU是否已完成对当前帧资源的命令的处理？
	//如果没有，请等到GPU完成命令直到该防护点为止。
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, (LPCWSTR)false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}


	UpdateObjectCBs(gt);
	UpdateInstanceCBs(gt);
	UpdateMaterialCBs(gt);
	
	mShadowMap->UpdateShadowTransform(gt);
	UpdateMainPassCB(gt);
	UpdateShadowPassCB(gt);

	//UpdateAnimation(gt);
	//UpdateWaves(gt);
}

void GameApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	//重新使用与命令记录关联的内存。
	//只有关联的命令列表在GPU上完成执行后，我们才能重置。
	ThrowIfFailed(cmdListAlloc->Reset());
	//通过ExecuteCommandList将命令列表添加到命令队列后，可以将其重置。
	//重用命令列表将重用内存。
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs.get()->GetPSO(PSOName::Opaque).Get()));

	{
		//SET SRV Descriptor heap
		ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		//Set Graphics Root Signature
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	}

	//root descriptors
	{
		// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
		// set as a root descriptor.
		auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
		mCommandList->SetGraphicsRootShaderResourceView(4, matBuffer->GetGPUVirtualAddress());
		// Bind the sky cube map.  For our demos, we just use one "world" cube map representing the environment
		// from far away, so all objects will use the same cube map and we only need to set it once per-frame.  
		// If we wanted to use "local" cube maps, we would have to change them per-object, or dynamically
		// index into an array of cube maps.
		CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		skyTexDescriptor.Offset(mMaterials[L"Sky"]->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(5, skyTexDescriptor);


		// Bind all the textures used in this scene.  Observe
		// that we only have to specify the first descriptor in the table.  
		// The root signature knows how many descriptors are expected in the table.
		mCommandList->SetGraphicsRootDescriptorTable(6, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	}

	//Draw shadow
	if (mEngineUI->IsShowShadow())
	{
		if (GameApp::RedrawShadowMap)
		{
			DrawSceneToShadowMap();
			GameApp::RedrawShadowMap = false;
		}
	}
	else
	{
		if (!GameApp::RedrawShadowMap)
		{
			mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
				D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			GameApp::RedrawShadowMap = true;
		}
	}

	//放在绘制阴影的后面
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	if (m4xMsaaState)//MSAA
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mMSAARenderTargetBuffer.Get(),
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &barrier);

		auto rtvDescriptor = m_msaaRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		auto dsvDescriptor = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

		mCommandList->OMSetRenderTargets(1, &rtvDescriptor, false, &dsvDescriptor);

		mCommandList->ClearRenderTargetView(rtvDescriptor, Colors::Transparent, 0, nullptr);
		mCommandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	}
	else
	{
		// Indicate a state transition on the resource usage.
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		// Clear the back buffer and depth buffer.
		mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		// Specify the buffers we are going to render to.
		mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	}
	//Draw Main
	{
		//Set Graphics Root CBV in slot 2
		auto passCB = mCurrFrameResource->PassCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());


		//Draw Render Items (Opaque)
		if (mEngineUI->IsShowLand())
		{
			mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::Opaque).Get());
			PIXBeginEvent(mCommandList.Get(), 0, "Draw Main::RenderLayer::Opaque");
			DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
			PIXEndEvent(mCommandList.Get());
		}
		//Draw Instanse 
		if (mEngineUI->IsShowModel())
		{
			mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::SkinAnime).Get());
			PIXBeginEvent(mCommandList.Get(), 0, "Draw Main::RenderLayer::InstanceSimpleItems");
			DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::InstanceSimpleItems]);
			PIXEndEvent(mCommandList.Get());
		}
		//Draw Sky box
		if (mEngineUI->IsShowSky())
		{
			mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::Sky).Get());
			PIXBeginEvent(mCommandList.Get(), 0, "Draw Main::RenderLayer::Sky");
			DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);
			PIXEndEvent(mCommandList.Get());
		}
	}

	//Draw Shock Wave Water
	if (mEngineUI->IsShowWater())
	{
		DrawShockWaveWater(gt);
	}

	if (m4xMsaaState)
	{
		D3D12_RESOURCE_BARRIER barriers[2] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(
				mMSAARenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RESOLVE_DEST)
		};
		
	//Draw UI
	PIXBeginEvent(mCommandList.Get(), 0, "Draw UI");
	mEngineUI->DrawUI(mCommandList.Get());
	PIXEndEvent(mCommandList.Get());
		
		mCommandList->ResourceBarrier(2, barriers);
		mCommandList->ResolveSubresource(CurrentBackBuffer(), 0, mMSAARenderTargetBuffer.Get(), 0, mBackBufferFormat);
	}

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT));


	///mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), false, NULL);


    // Indicate a state transition on the resource usage.
	//mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		//D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));


	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void GameApp::CreateDescriptorHeaps()
{
	int SRVIndex = 0;
	UINT PBRTextureNum = (UINT)(mPBRTextureName.size() * 6);
	UINT ModelTextureNum = (UINT)mModelLoder->GetAllModelTextures().size() * 6;
	UINT ShockWaveWater = 3;
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	//PBR Textures
	//sky+shadow map=2
	//model PBR texture
	//ShockWaveWater map
	srvHeapDesc.NumDescriptors = PBRTextureNum+ ModelTextureNum+ ShockWaveWater+2;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	unordered_map<string, ID3D12Resource*> PBRTexResource;
#pragma region simple geometry PBR Texture
	for (auto &e : mPBRTextureName)
	{
		PBRTexResource["Diffuse"] =mTextureManage->GetTexture(e + L"\\" + e + L"_albedo")->Resource.Get();
		PBRTexResource["Normal"] = mTextureManage->GetTexture(e + L"\\" + e + L"_normal")->Resource.Get();
		PBRTexResource["AO"] = mTextureManage->GetTexture(e + L"\\" + e + L"_ao")->Resource.Get();
		PBRTexResource["Metallic"] = mTextureManage->GetTexture(e + L"\\" + e + L"_metallic")->Resource.Get();
		PBRTexResource["Roughness"] = mTextureManage->GetTexture(e + L"\\" + e + L"_roughness")->Resource.Get();
		PBRTexResource["Height"] = mTextureManage->GetTexture(e + L"\\" + e + L"_height")->Resource.Get();

		CreatePBRSRVinDescriptorHeap(PBRTexResource, &SRVIndex, &hDescriptor,e);
	}
#pragma endregion

#pragma region Load model 
	for (auto& e : mModelLoder->GetAllModelTextures())
	{
		PBRTexResource["Diffuse"] = e.second[0]->Resource.Get();
		PBRTexResource["AO"] = e.second[1]->Resource.Get();
		PBRTexResource["Metallic"] = e.second[2]->Resource.Get();
		PBRTexResource["Normal"] = e.second[3]->Resource.Get();
		PBRTexResource["Roughness"] = e.second[4]->Resource.Get();
		PBRTexResource["Height"] = e.second[5]->Resource.Get();

		CreatePBRSRVinDescriptorHeap(PBRTexResource, &SRVIndex, &hDescriptor, wstring(e.first.begin(), e.first.end()));
	}
#pragma endregion

#pragma region Shadow Map
	{
		auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
		mShadowMap->BuildDescriptors(hDescriptor,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, mDsvDescriptorSize));//把shadow map的DSV存放在DSV堆中的第二个位置
		mShadowMapIndex = SRVIndex++;
		// next descriptor
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}
#pragma endregion

#pragma region ShockWaveWater
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE RefractionSRVDescriptor = hDescriptor;
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
		mWaterRefractionMapIndex = SRVIndex++;

		CD3DX12_CPU_DESCRIPTOR_HANDLE ReflectionRTVDescriptor = hDescriptor;
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
		mWaterReflectionMapIndex = SRVIndex++;

		CD3DX12_CPU_DESCRIPTOR_HANDLE WavesBumpDescriptor = hDescriptor;
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
		mWaterDumpWaveIndex = SRVIndex++;

		mShockWaveWater->BuildDescriptors(
			RefractionSRVDescriptor,
			ReflectionRTVDescriptor,
			WavesBumpDescriptor
		);
	}
#pragma region

#pragma region height map terrain
	for (auto& e : mHeightMapTerrain->GetAllTerrainTextures())
	{
		PBRTexResource["Diffuse"] = e.second[0]->Resource.Get();
		PBRTexResource["AO"] = e.second[1]->Resource.Get();
		PBRTexResource["Metallic"] = e.second[2]->Resource.Get();
		PBRTexResource["Normal"] = e.second[3]->Resource.Get();
		PBRTexResource["Roughness"] = e.second[4]->Resource.Get();
		PBRTexResource["Height"] = e.second[5]->Resource.Get();

		CreatePBRSRVinDescriptorHeap(PBRTexResource, &SRVIndex, &hDescriptor, wstring(e.first.begin(), e.first.end()));
	}
	CD3DX12_CPU_DESCRIPTOR_HANDLE BlendMapDescriptor = hDescriptor;
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	mBlendMapIndex = SRVIndex++;

	CD3DX12_CPU_DESCRIPTOR_HANDLE HeightMapDescriptor = hDescriptor;
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	mHeightMapIndex = SRVIndex++;

	mHeightMapTerrain->BuildHeightmapSRV(BlendMapDescriptor, HeightMapDescriptor);
#pragma endregion

	mSRVSize = SRVIndex;

#pragma region HDR Sky
	{
		auto SkyTex = mTextureManage->GetTexture(L"HDRSky\\Sky")->Resource;

		D3D12_SHADER_RESOURCE_VIEW_DESC SkysrvDesc = {};
		SkysrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SkysrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		SkysrvDesc.TextureCube.MostDetailedMip = 0;
		SkysrvDesc.TextureCube.MipLevels = SkyTex->GetDesc().MipLevels;
		SkysrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		SkysrvDesc.Format = SkyTex->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(SkyTex.Get(), &SkysrvDesc, hDescriptor);
		mMaterials[L"Sky"]->DiffuseSrvHeapIndex = SRVIndex++;
		// next descriptor
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}
#pragma endregion

#pragma region GUI
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mGUISrvDescriptorHeap)));
	}
#pragma endregion
}

void GameApp::CreatePBRSRVinDescriptorHeap(
	unordered_map<std::string,ID3D12Resource*> TexResource,
	int* SRVIndex, 
	CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor,
	wstring MaterialName)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = TexResource["Diffuse"]->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = TexResource["Diffuse"]->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(TexResource["Diffuse"], &srvDesc, *hDescriptor);
	mMaterials[MaterialName]->DiffuseSrvHeapIndex = (*SRVIndex)++;

	// next descriptor
	hDescriptor->Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = TexResource["Normal"]->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = TexResource["Normal"]->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(TexResource["Normal"], &srvDesc, *hDescriptor);
	mMaterials[MaterialName]->NormalSrvHeapIndex = (*SRVIndex)++;


	// next descriptor
	hDescriptor->Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = TexResource["AO"]->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = TexResource["AO"]->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(TexResource["AO"], &srvDesc, *hDescriptor);
	mMaterials[MaterialName]->AoSrvHeapIndex = (*SRVIndex)++;


	// next descriptor
	hDescriptor->Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = TexResource["Metallic"]->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = TexResource["Metallic"]->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(TexResource["Metallic"], &srvDesc, *hDescriptor);
	mMaterials[MaterialName]->MetallicSrvHeapIndex = (*SRVIndex)++;


	// next descriptor
	hDescriptor->Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = TexResource["Roughness"]->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = TexResource["Roughness"]->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(TexResource["Roughness"], &srvDesc, *hDescriptor);
	mMaterials[MaterialName]->RoughnessSrvHeapIndex = (*SRVIndex)++;

	// next descriptor
	hDescriptor->Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = TexResource["Height"]->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = TexResource["Height"]->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(TexResource["Height"], &srvDesc, *hDescriptor);
	mMaterials[MaterialName]->HeightSrvHeapIndex = (*SRVIndex)++;

	// next descriptor
	hDescriptor->Offset(1, mCbvSrvDescriptorSize);

}

void GameApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void GameApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void GameApp::OnMouseMove(WPARAM btnState, int x, int y)
{

		//if (x <= 400 || x >= 1910 || y <= 400 || y >= 1070)
		//{
		//	//SetCursorPos(1920 / 2, 1080 / 2);
		//	mLastMousePos.x = 1920 / 2;
		//	mLastMousePos.y = 1080 / 2;
		//}
		//else {
		//	float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		//	float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
		//	mCamera.Pitch(dy);
		//	mCamera.RotateY(dx);
		//	mLastMousePos.x = x;
		//	mLastMousePos.y = y;
		//	
		//}
	

	if ((btnState & MK_LBUTTON) != 0)
	{
		if (x >= mScissorRect.left && y >= mScissorRect.top
			&& y <= (mScissorRect.top+ (mScissorRect.bottom- mScissorRect.top))
			&& x <= (mScissorRect .left+ (mScissorRect.right- mScissorRect.left)))
		{
			float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
			mCamera.Pitch(dy);
			mCamera.RotateY(dx);
			mLastMousePos.x = x;
			mLastMousePos.y = y;
		}
	}

}

void GameApp::OnKeyboardInput(const GameTimer& gt)
{

	float DeltaTime = gt.DeltaTime();
	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f * DeltaTime);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * DeltaTime);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * DeltaTime);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * DeltaTime);


	mCamera.UpdateViewMatrix();
}


//not use(球面坐标)
void GameApp::UpdateCamera(const GameTimer& gt)
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
}

void GameApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->SimpleObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->ObjCBIndex>=0)
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

void GameApp::UpdateInstanceCBs(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	
	for (int index=0;index< mRitemLayer[(int)RenderLayer::InstanceSimpleItems].size();++index)
	{
		const auto& instanceData = mRitemLayer[(int)RenderLayer::InstanceSimpleItems][index]->Instances;
		const auto& ReflectioninstanceData = mReflectionWaterLayer[(int)RenderLayer::InstanceSimpleItems][index]->Instances;
		int visibleInstanceCount = 0;
		
		for (int matNum = 0; matNum < mModelLoder->GetModelTextureNames("Blue_Tree_02a").size(); ++matNum)
		{
			visibleInstanceCount = 0;
			auto currInstanceBuffer = mCurrFrameResource->InstanceSimpleObjectCB[matNum].get();
			auto ReflectioncurrInstanceBuffer = mCurrFrameResource->ReflectionInstanceSimpleObjectCB[matNum].get();
			for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
			{
				XMMATRIX world = XMLoadFloat4x4(&instanceData[i].World);
				XMMATRIX ReflectionWorld = XMLoadFloat4x4(&ReflectioninstanceData[i].World);
				
				XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

				XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);

				// 视图空间转换到局部空间
				XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

				// 将摄像机视锥从视图空间转换为对象的局部空间。
				BoundingFrustum localSpaceFrustum;
				mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

				// 在局部空间中执行盒子/视锥相交测试。
				if ((localSpaceFrustum.Contains(mRitemLayer[(int)RenderLayer::InstanceSimpleItems][index]->Bounds) != DirectX::DISJOINT) || (mFrustumCullingEnabled == false))
				{
					InstanceConstants data;
					XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
					XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
					string matname = mModelLoder->GetModelTextureNames("Blue_Tree_02a")[matNum];
					data.MaterialIndex = mMaterials[wstring(matname.begin(),matname.end())]->MatCBIndex;
				
					InstanceConstants Reflectiondata= data;
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

void GameApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.NormalSrvHeapIndex = mat->NormalSrvHeapIndex;
			matConstants.RoughnessSrvHeapIndex = mat->RoughnessSrvHeapIndex;
			matConstants.MetallicSrvHeapIndex = mat->MetallicSrvHeapIndex;
			matConstants.AOSrvHeapIndex = mat->AoSrvHeapIndex;
			matConstants.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matConstants.HeightSrvHeapIndex = mat->HeightSrvHeapIndex;
			matConstants.height_scale = mat->height_scale;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));
			
			currMaterialBuffer->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void GameApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mCamera.GetView4x4f());
	XMMATRIX proj = XMLoadFloat4x4(&mCamera.GetProj4x4f());

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(XMLoadFloat4x4(&mShadowMap->mShadowTransform)));
	
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };


	mMainPassCB.Lights[0].Direction=mShadowMap->mRotatedLightDirections[0];
	mMainPassCB.Lights[0].Strength = { 1.0f, 1.0f, 1.0f };
	mMainPassCB.Lights[0].FalloffStart = 1.0f;
	mMainPassCB.Lights[0].FalloffEnd = 50.0f;
	mMainPassCB.Lights[0].Position = { 20,20,20 };

	mMainPassCB.Lights[1].Direction = mShadowMap->mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 1.0f, 1.0f, 1.0f };
	mMainPassCB.Lights[1].FalloffStart = 1.0f;
	mMainPassCB.Lights[1].FalloffEnd = 50.0f;
	mMainPassCB.Lights[1].Position = { 20,20,20 };

	mMainPassCB.Lights[2].Direction = mShadowMap->mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 1.0f, 1.0f, 1.0f };
	mMainPassCB.Lights[2].FalloffStart = 1.0f;
	mMainPassCB.Lights[2].FalloffEnd = 50.0f;
	mMainPassCB.Lights[2].Position = { 20,20,20 };

	mMainPassCB.ShadowMapIndex = mShadowMapIndex;
	mMainPassCB.WaterDumpWaveIndex = mWaterDumpWaveIndex;
	mMainPassCB.WaterReflectionMapIndex = mWaterReflectionMapIndex;
	mMainPassCB.WaterRefractionMapIndex = mWaterRefractionMapIndex;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);//index 0 main pass
}

void GameApp::UpdateWaves(const GameTimer& gt)
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
	mWaves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for (int i = 0; i < mWaves->VertexCount(); ++i)
	{
		L3DVertice::PosNormalTexc v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mSphereRitem->Geo[0]->VertexBufferGPU = currWavesVB->Resource();
}

void GameApp::BuildRootSignature()
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

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void GameApp::BuildShadersAndInputLayout()
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

	mShaders["GerstnerWave"] = new GerstnerWaveShader(L"Shaders\\DefaultVS.hlsl", L"Shaders\\DefaultPS.hlsl", L"");
	mShaders["Sky"] = new SkyShader(L"Shaders\\SkyVS.hlsl", L"Shaders\\SkyPS.hlsl", L"");
	mShaders["Instance"] = new SimpleGeometryInstanceShader(L"Shaders\\SimpleGeoInstanceVS.hlsl", L"Shaders\\SimpleGeoInstancePS.hlsl", L"");
	mShaders["InstanceSimpleGeoShadowMap"]= new SimpleGeometryShadowMapShader(L"Shaders\\SimpleGeoInstanceShadowVS.hlsl", L"Shaders\\SimpleGeoInstanceShadowPS.hlsl", L"", alphaTestDefines);
	mShaders["StaticComplexModelInstance"] = new ComplexStaticModelInstanceShader(L"Shaders\\SimpleGeoInstanceVS.hlsl", L"Shaders\\ComplexModelInstancePS.hlsl", L"", alphaTestDefines);
	mShaders["SkinAnime"]= new SkinAnimeShader(L"Shaders\\SimpleGeoInstanceVS.hlsl", L"Shaders\\ComplexModelInstancePS.hlsl", L"",SkinAnimeDefines);
	mShaders["WaterRefractionMask"] = new WaterRefractionMaskShader(L"Shaders\\WaterRefractionMaskVS.hlsl", L"Shaders\\WaterRefractionMaskPS.hlsl", L"");
	mShaders["ShockWaveWater"] = new ShockWaveWaterShader(L"Shaders\\ShockWaveWaterVS.hlsl", L"Shaders\\ShockWaveWaterPS.hlsl", L"");
}

void GameApp::BuildSimpleGeometry()
{
	//box mesh
	mGeometries["Box"] = std::move(mSimpleGeoInstance->GetSimpleGeoMesh(SimpleGeometry::Box));
	
	//build sky geometrie
	mSky->BuildSkyMesh();
	mGeometries["Sky"] = std::move(mSky->GetSkyMesh());
}

void GameApp::BuildLandGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(100.0f, 100.0f, 2, 2);

	//
	// Extract the vertex elements we are interested and apply the height function to
	// each vertex.  In addition, color the vertices based on their height so we have
	// sandy looking beaches, grassy low hills, and snow mountain peaks.
	//

	std::vector<L3DVertice::PosNormalTangentTexc> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		vertices[i].Pos = grid.Vertices[i].Position;
		vertices[i].Normal = grid.Vertices[i].Normal;
		vertices[i].Tangent = grid.Vertices[i].TangentU;
		vertices[i].Texc = grid.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(L3DVertice::PosNormalTangentTexc);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = L3DUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = L3DUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(L3DVertice::PosNormalTangentTexc);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

void GameApp::BuildWavesGeometryBuffers()
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

	UINT vbByteSize = mWaves->VertexCount() * sizeof(L3DVertice::PosNormalTexc);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = L3DUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(L3DVertice::PosNormalTexc);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void GameApp::BuildPSOs()
{
	
	// PSO for opaque objects.
#pragma region PSO_for_opaque_objects
	std::vector<D3D12_INPUT_ELEMENT_DESC> Input = mShaders["GerstnerWave"]->GetInputLayout();
	DXGI_FORMAT RTVFormats[8];
	RTVFormats[0] = mBackBufferFormat;//only one rtv
	mPSOs->BuildPSO(md3dDevice.Get(), 
		PSOName::Opaque,
		mRootSignature.Get(),
		{reinterpret_cast<BYTE*>(mShaders["GerstnerWave"]->GetVSShader()->GetBufferPointer()),
			mShaders["GerstnerWave"]->GetVSShader()->GetBufferSize() },
		{reinterpret_cast<BYTE*>(mShaders["GerstnerWave"]->GetPSShader()->GetBufferPointer()),
		mShaders["GerstnerWave"]->GetPSShader()->GetBufferSize()},
		{0},
		{0}, 
		{0}, 
		{0},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		mDepthStencilFormat,
		{UINT(m4xMsaaState ? 4 : 1) ,UINT(0)},
		0,
		{0},
		{});
#pragma endregion 


	// PSO for sky.
#pragma region PSO_for_sky
	Input = mShaders["Sky"]->GetInputLayout();
	D3D12_RASTERIZER_DESC RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	D3D12_DEPTH_STENCIL_DESC  DepthStencilState=CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	mPSOs->BuildPSO(md3dDevice.Get(),
		PSOName::Sky,
		mRootSignature.Get(),
		{ reinterpret_cast<BYTE*>(mShaders["Sky"]->GetVSShader()->GetBufferPointer()),
			mShaders["Sky"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(mShaders["Sky"]->GetPSShader()->GetBufferPointer()),
		mShaders["Sky"]->GetPSShader()->GetBufferSize() },
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
		mDepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for Instance .
#pragma region PSO_for_Instance
	Input = mShaders["Instance"]->GetInputLayout();
	mPSOs->BuildPSO(md3dDevice.Get(),
		PSOName::Instance,
		mRootSignature.Get(),
		{ reinterpret_cast<BYTE*>(mShaders["Instance"]->GetVSShader()->GetBufferPointer()),
			mShaders["Instance"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(mShaders["Instance"]->GetPSShader()->GetBufferPointer()),
		mShaders["Instance"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		mDepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for Instance  shadow.
#pragma region PSO_for_Instance_shadow
	D3D12_RASTERIZER_DESC ShadowRasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	ShadowRasterizerState.DepthBias = 25000;
	ShadowRasterizerState.DepthBiasClamp = 0.0f;
	ShadowRasterizerState.SlopeScaledDepthBias = 1.0f;
	RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	Input = mShaders["InstanceSimpleGeoShadowMap"]->GetInputLayout();
	mPSOs->BuildPSO(md3dDevice.Get(),
		PSOName::InstanceSimpleShadow_Opaque,
		mRootSignature.Get(),
		{ reinterpret_cast<BYTE*>(mShaders["InstanceSimpleGeoShadowMap"]->GetVSShader()->GetBufferPointer()),
			mShaders["InstanceSimpleGeoShadowMap"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(mShaders["InstanceSimpleGeoShadowMap"]->GetPSShader()->GetBufferPointer()),
		mShaders["InstanceSimpleGeoShadowMap"]->GetPSShader()->GetBufferSize() },
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
	RTVFormats[0] = mBackBufferFormat;//only one rtv
	Input = mShaders["StaticComplexModelInstance"]->GetInputLayout();
	mPSOs->BuildPSO(md3dDevice.Get(),
		PSOName::StaticComplexModelInstance,
		mRootSignature.Get(),
		{ reinterpret_cast<BYTE*>(mShaders["StaticComplexModelInstance"]->GetVSShader()->GetBufferPointer()),
			mShaders["StaticComplexModelInstance"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(mShaders["StaticComplexModelInstance"]->GetPSShader()->GetBufferPointer()),
		mShaders["StaticComplexModelInstance"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		mDepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for skin anime .
#pragma region PSO_for_skin_anime
	RTVFormats[0] = mBackBufferFormat;//only one rtv
	Input = mShaders["SkinAnime"]->GetInputLayout();
	mPSOs->BuildPSO(md3dDevice.Get(),
		PSOName::SkinAnime,
		mRootSignature.Get(),
		{ reinterpret_cast<BYTE*>(mShaders["SkinAnime"]->GetVSShader()->GetBufferPointer()),
			mShaders["SkinAnime"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(mShaders["SkinAnime"]->GetPSShader()->GetBufferPointer()),
		mShaders["SkinAnime"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		mDepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for Water Refraction Mask.
#pragma region PSO_for_Water_Refraction_Mask
	Input = mShaders["WaterRefractionMask"]->GetInputLayout();
	D3D12_BLEND_DESC BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALPHA;
	BlendDesc.RenderTarget[0].SrcBlendAlpha= D3D12_BLEND_SRC_ALPHA;
	DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	mPSOs->BuildPSO(md3dDevice.Get(),
		PSOName::WaterRefractionMask,
		mRootSignature.Get(),
		{ reinterpret_cast<BYTE*>(mShaders["WaterRefractionMask"]->GetVSShader()->GetBufferPointer()),
			mShaders["WaterRefractionMask"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(mShaders["WaterRefractionMask"]->GetPSShader()->GetBufferPointer()),
		mShaders["WaterRefractionMask"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		BlendDesc,
		UINT_MAX,
		CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
		DepthStencilState,
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		mDepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion


	// PSO for Shock Wave Water.
#pragma region Shock Wave Water
	Input = mShaders["ShockWaveWater"]->GetInputLayout();
	mPSOs->BuildPSO(md3dDevice.Get(),
		PSOName::ShockWaveWater,
		mRootSignature.Get(),
		{ reinterpret_cast<BYTE*>(mShaders["ShockWaveWater"]->GetVSShader()->GetBufferPointer()),
			mShaders["ShockWaveWater"]->GetVSShader()->GetBufferSize() },
		{ reinterpret_cast<BYTE*>(mShaders["ShockWaveWater"]->GetPSShader()->GetBufferPointer()),
		mShaders["ShockWaveWater"]->GetPSShader()->GetBufferSize() },
		{},
		{},
		{},
		{},
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		UINT_MAX,
		CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		{ Input.data(), (UINT)Input.size() },
		{},
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		1,
		RTVFormats,
		mDepthStencilFormat,
		{ UINT(m4xMsaaState ? 4 : 1) ,UINT(0) },
		0,
		{},
		{});
#pragma endregion
}

void GameApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			2,(UINT)mModelLoder->GetModelMesh("Blue_Tree_02a").size(),1,(UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
	}
}

void GameApp::BuildAllMaterials()
{
	//Index
	int MatCBIndex = 0;
	int DiffuseSrvHeapIndex = 0;
	XMFLOAT4X4  MatTransform = MathHelper::Identity4x4();


#pragma region Simple Geometry Material

	//white_spruce_tree_bark Material
	BuildMaterials(
		L"PBRwhite_spruce_tree_bark",
		MatCBIndex++,
		0.09f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform);

	//PBRharshbricks Material
	BuildMaterials(
		L"PBRharshbricks",
		MatCBIndex++,
		0.05f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform);

	//PBRrocky_shoreline1 Material
	BuildMaterials(
		L"PBRrocky_shoreline1",
		MatCBIndex++,
		0.05f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform);

	//PBRstylized_grass1 Material
	BuildMaterials(
		L"PBRstylized_grass1",
		MatCBIndex++,
		0.09f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform);

	//PBRIndustrial_narrow_brick Material
	BuildMaterials(
		L"PBRIndustrial_narrow_brick",
		MatCBIndex++,
		0.05f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform);

	//PBRBrass Material
	XMStoreFloat4x4(&MatTransform, XMMatrixScaling(0.1f, 0.1f, 0.1f));
	BuildMaterials(
		L"PBRBrass",
		MatCBIndex++,
		0.01f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform);
#pragma endregion


#pragma region Load Model Material
	MatTransform = MathHelper::Identity4x4();
	for (auto e : mModelLoder->GetAllModelTextures())
	{
		BuildMaterials(
			wstring(e.first.begin(), e.first.end()),
			MatCBIndex++,
			0.09f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			XMFLOAT3(0.1f, 0.1f, 0.1f),
			MatTransform,
			true);
	}
#pragma endregion


#pragma region Sky Material
		mSky->BuildSkyMaterials(MatCBIndex++);
		mMaterials[L"Sky"] = std::move(mSky->GetSkyMaterial());
#pragma endregion
}


void GameApp::BuildMaterials(
	wstring name,
	int MatCBIndex,
	float Height_Scale,
	XMFLOAT4 DiffuseAlbedo, 
	XMFLOAT3 FresnelR0,
	XMFLOAT4X4 MatTransform,
	bool isModelTexture)
{
	auto  Mat = std::make_unique<Material>();
	Mat->Name = name;
	Mat->MatCBIndex = MatCBIndex;
	Mat->DiffuseAlbedo = DiffuseAlbedo;
	Mat->FresnelR0 = FresnelR0;
	Mat->MatTransform = MatTransform;
	Mat->height_scale = Height_Scale;
	mMaterials[name] = std::move(Mat);

	if(!isModelTexture)
	mPBRTextureName.push_back(name);
}


void GameApp::BuildRenderItems()
{
	//index
	int ObjCBIndex = 0;
	//Land 
	{
		RenderItem LandRitem = {};
		LandRitem.World = MathHelper::Identity4x4();
		XMStoreFloat4x4(&LandRitem.World,XMMatrixTranslation(0.0f,1.0f,0.0f)* XMMatrixScaling(1.0, 1.0, 1.0));
		XMStoreFloat4x4(&LandRitem.TexTransform, XMMatrixScaling(5.0, 5.0, 5.0));
		LandRitem.ObjCBIndex = ObjCBIndex++;
		LandRitem.Mat = mMaterials[L"PBRBrass"].get();
		LandRitem.Geo.push_back(mGeometries["landGeo"].get());
		LandRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		LandRitem.IndexCount.push_back(LandRitem.Geo[0]->DrawArgs["grid"].IndexCount);
		LandRitem.StartIndexLocation.push_back(LandRitem.Geo[0]->DrawArgs["grid"].StartIndexLocation);
		LandRitem.BaseVertexLocation.push_back(LandRitem.Geo[0]->DrawArgs["grid"].BaseVertexLocation);
		LandRitem.InstanceCount = 1;
		
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
	//Shock Wave Water
	{
		RenderItem ShockWaveWaterRitem = {};
		ShockWaveWaterRitem.World = MathHelper::Identity4x4();
		XMStoreFloat4x4(&ShockWaveWaterRitem.World, XMMatrixScaling(5.0, 5.0, 5.0));
		XMStoreFloat4x4(&ShockWaveWaterRitem.TexTransform, XMMatrixScaling(5.0, 5.0, 5.0));
		ShockWaveWaterRitem.ObjCBIndex = ObjCBIndex++;
		ShockWaveWaterRitem.Mat = mMaterials[L"PBRharshbricks"].get();
		ShockWaveWaterRitem.Geo.push_back(mGeometries["landGeo"].get());
		ShockWaveWaterRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ShockWaveWaterRitem.IndexCount.push_back(ShockWaveWaterRitem.Geo[0]->DrawArgs["grid"].IndexCount);
		ShockWaveWaterRitem.StartIndexLocation.push_back(ShockWaveWaterRitem.Geo[0]->DrawArgs["grid"].StartIndexLocation);
		ShockWaveWaterRitem.BaseVertexLocation.push_back(ShockWaveWaterRitem.Geo[0]->DrawArgs["grid"].BaseVertexLocation);
		ShockWaveWaterRitem.InstanceCount = 1;
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
		skyRitem.Mat = mMaterials[L"Sky"].get();
		skyRitem.Geo.push_back(mGeometries["Sky"].get());
		skyRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		skyRitem.IndexCount.push_back(skyRitem.Geo[0]->DrawArgs["Sphere"].IndexCount);
		skyRitem.StartIndexLocation.push_back(skyRitem.Geo[0]->DrawArgs["Sphere"].StartIndexLocation);
		skyRitem.BaseVertexLocation.push_back(skyRitem.Geo[0]->DrawArgs["Sphere"].BaseVertexLocation);
		skyRitem.InstanceCount = 1;

#pragma region Reflection
		RenderItem reflectionWaterskyRitem = skyRitem;
		reflectionWaterskyRitem.ItemType = RenderItemType::Reflection;
		XMStoreFloat4x4(&reflectionWaterskyRitem.World, XMMatrixScaling(1.0f, -1.0f, 1.0f)*XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
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

void GameApp::BuildModelGeoInstanceItems()
{
	// MODEL TEST CODE
#pragma region MODEL_TEST_CODE

	BoundingBox lbox;
	lbox.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	lbox.Extents = XMFLOAT3(200.0f, 200.0f, 200.0f);

	RenderItem InstanceModelRitem = {};
	for (auto& e : mModelLoder->GetModelMesh("Blue_Tree_02a"))
	{
		InstanceModelRitem.Geo.push_back(&e.mMeshGeo);
		InstanceModelRitem.IndexCount.push_back(e.mMeshGeo.DrawArgs["Model Mesh"].IndexCount);
		InstanceModelRitem.StartIndexLocation.push_back(e.mMeshGeo.DrawArgs["Model Mesh"].StartIndexLocation);
		InstanceModelRitem.BaseVertexLocation.push_back(e.mMeshGeo.DrawArgs["Model Mesh"].BaseVertexLocation);
	}
	
	InstanceModelRitem.World = MathHelper::Identity4x4();
	InstanceModelRitem.TexTransform = MathHelper::Identity4x4();
	InstanceModelRitem.ObjCBIndex = -1;//not important in instance item
	InstanceModelRitem.Mat = mMaterials[L"PBRBrass"].get();
	InstanceModelRitem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	InstanceModelRitem.Bounds = lbox;
	
	// Generate instance data.
	///Hard code
	const int n =5;
	InstanceModelRitem.Instances.resize(n * n);
	InstanceModelRitem.InstanceCount = 0;//init count

	float width = 25.0f;
	float height = 25.0f;

	float x = -0.5f * width;
	float y = -0.5f * height;

	float dx = width / (n - 1);
	float dy = height / (n - 1);

		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < n; ++j)
			{
				int index =i * n + j;
				// Position instanced along a 3D grid.
				XMStoreFloat4x4(&InstanceModelRitem.Instances[index].World, /*XMMatrixRotationX(MathHelper::Pi/2)**/XMMatrixRotationY(MathHelper::RandF()* MathHelper::Pi) * XMLoadFloat4x4(&XMFLOAT4X4(
					0.04f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.04f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.04f, 0.0f,
					x + j * dx, 0.0f, y + i * dy, 1.0f)));

				InstanceModelRitem.Instances[index].MaterialIndex = (UINT)(mMaterials.size() - 3);
				InstanceModelRitem.Instances[index].TexTransform= MathHelper::Identity4x4();
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
					XMStoreFloat4x4(&ReflectionInstanceModelRitem.Instances[index].World, XMMatrixScaling(1.0f, -1.0f, 1.0f) * WaterMAT);
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

void GameApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	//sizeof ObjectConstants 
	UINT objCBByteSize = L3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT skinnedCBByteSize = L3DUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));

	auto objectCB = mCurrFrameResource->SimpleObjectCB->Resource();
	auto skinnedCB = mCurrFrameResource->SkinnedCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress;
		if (ri->ObjCBIndex >= 0)
		{
			objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		}
		else//instance draw call
		{
			objCBAddress = objectCB->GetGPUVirtualAddress();
		}
		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);


		for (int MeshNum=0;MeshNum<ri->Geo.size();++MeshNum)
		{
			if (ri->Geo.size() > 1)//test
			{
				D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress();
				cmdList->SetGraphicsRootConstantBufferView(2, skinnedCBAddress);
				//const D3D12_VERTEX_BUFFER_VIEW* pViews[] = { &ri->Geo[MeshNum]->VertexBufferView(),&mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].mBoneMeshs[MeshNum].mBoneGeo.VertexBufferView() };
				cmdList->IASetVertexBuffers(0, 1, &ri->Geo[MeshNum]->VertexBufferView());
				cmdList->IASetVertexBuffers(1, 1, &mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].mBoneMeshs[MeshNum].mBoneGeo.VertexBufferView());
			}
			else
			{
				cmdList->IASetVertexBuffers(0, 1, &ri->Geo[MeshNum]->VertexBufferView());

			}
			cmdList->IASetIndexBuffer(&ri->Geo[MeshNum]->IndexBufferView());
			cmdList->IASetPrimitiveTopology(ri->PrimitiveType);


			// Set the instance buffer to use for this render-item.  For structured buffers, we can bypass 
			// the heap and set as a root descriptor.
			ID3D12Resource* instanceBuffer=nullptr;
			if (ritems[i]->ItemType == RenderItemType::Normal)
			{
				instanceBuffer = mCurrFrameResource->InstanceSimpleObjectCB[MeshNum]->Resource();
			}
			else if(ritems[i]->ItemType == RenderItemType::Reflection)
			{
				instanceBuffer = mCurrFrameResource->ReflectionInstanceSimpleObjectCB[MeshNum]->Resource();
			}
			else {}
			mCommandList->SetGraphicsRootShaderResourceView(3, instanceBuffer->GetGPUVirtualAddress());


			cmdList->DrawIndexedInstanced(ri->IndexCount[MeshNum], ri->InstanceCount, ri->StartIndexLocation[MeshNum], ri->BaseVertexLocation[MeshNum], 0);
		}
	}
}

float GameApp::GetHillsHeight(float x, float z)const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 GameApp::GetHillsNormal(float x, float z)const
{
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

void GameApp::DrawShockWaveWater(const GameTimer& gt)
{
	DrawWaterReflectionMap(gt);

	DrawWaterRefractionMap(gt);


	mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::ShockWaveWater).Get());
	mCommandList->OMSetRenderTargets(1, &m_msaaRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
		, false, &mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_RESOURCE_BARRIER barriers[1] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(
			mMSAARenderTargetBuffer.Get(),
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET)
	};
	mCommandList->ResourceBarrier(1, barriers);

	//Draw Shock Wave Water
	PIXBeginEvent(mCommandList.Get(), 0, "Draw Water::RenderLayer::ShockWaveWater");
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::ShockWaveWater]);
	PIXEndEvent(mCommandList.Get());
}

void GameApp::DrawWaterReflectionMap(const GameTimer& gt)
{

	
	// Clear the back buffer and depth buffer.
	//test Reflection sence no shadow
	{
		mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
			D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		GameApp::RedrawShadowMap = true;
	}

	if (m4xMsaaState)//MSAA
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mShockWaveWater->ReflectionRTV(),
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &barrier);

		auto RTVDescriptor = mShockWaveWater->ReflectionDescriptor();
		auto DSVDescriptor = mShockWaveWater->ReflectionDepthMapDSVDescriptor();

		mCommandList->OMSetRenderTargets(1, &RTVDescriptor, false, &DSVDescriptor);

		mCommandList->ClearRenderTargetView(RTVDescriptor, Colors::Transparent, 0, nullptr);
		mCommandList->ClearDepthStencilView(DSVDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}
	else
	{
		//not do anythings
	}

	//Draw Render Items (Opaque)
	if (mEngineUI->IsShowLand())
	{
		mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::Opaque).Get());
		PIXBeginEvent(mCommandList.Get(), 0, "Draw Water::RenderLayer::Opaque");
		DrawRenderItems(mCommandList.Get(), mReflectionWaterLayer[(int)RenderLayer::Opaque]);
		PIXEndEvent(mCommandList.Get());
	}
	//Draw Instanse 
	if (mEngineUI->IsShowModel())
	{
		mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::SkinAnime).Get());
		PIXBeginEvent(mCommandList.Get(), 0, "Draw Water::RenderLayer::InstanceSimpleItems");
		DrawRenderItems(mCommandList.Get(), mReflectionWaterLayer[(int)RenderLayer::InstanceSimpleItems]);
		PIXEndEvent(mCommandList.Get());
	}
	//Draw Sky box
	if (mEngineUI->IsShowSky())
	{
		mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::Sky).Get());
		PIXBeginEvent(mCommandList.Get(), 0, "Draw Water::RenderLayer::Sky");
		DrawRenderItems(mCommandList.Get(), mReflectionWaterLayer[(int)RenderLayer::Sky]);
		PIXEndEvent(mCommandList.Get());
	}

	D3D12_RESOURCE_BARRIER barriers[1] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(
			mShockWaveWater->ReflectionRTV(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
	};
	mCommandList->ResourceBarrier(1, barriers);
	mCommandList->ResolveSubresource(mShockWaveWater->SingleReflectionSRV(), 0, mShockWaveWater->ReflectionRTV(), 0, mBackBufferFormat);
}

void GameApp::DrawWaterRefractionMap(const GameTimer& gt)
{
	auto rtvDescriptor = m_msaaRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvDescriptor = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
	mCommandList->OMSetRenderTargets(1, &rtvDescriptor, false, &dsvDescriptor);

	//Draw Shock Wave Water
	mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::WaterRefractionMask).Get());
	PIXBeginEvent(mCommandList.Get(), 0, "Draw Main::RenderLayer::WaterRefractionMask");
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::ShockWaveWater]);
	PIXEndEvent(mCommandList.Get());

	D3D12_RESOURCE_BARRIER barriers[1] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(
			mMSAARenderTargetBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
	};
	mCommandList->ResourceBarrier(1, barriers);

	mCommandList->ResolveSubresource(mShockWaveWater->SingleRefractionSRV(), 0, mMSAARenderTargetBuffer.Get(), 0, mBackBufferFormat);
}

HeightmapTerrain::InitInfo GameApp::HeightmapTerrainInit()
{
	HeightmapTerrain::InitInfo TerrainInfo;
	TerrainInfo.HeightMapFilename = "Textures/Terrain/terrain.raw";
	TerrainInfo.LayerMapFilename[0] = "Terrain/grass";
	TerrainInfo.LayerMapFilename[1] = "Terrain/darkdirt";
	TerrainInfo.LayerMapFilename[2] = "Terrain/stone";
	TerrainInfo.LayerMapFilename[3] = "Terrain/lightdirt";
	TerrainInfo.LayerMapFilename[4] = "Terrain/snow";
	TerrainInfo.BlendMapFilename = "Terrain/blend";
	TerrainInfo.HeightScale = 80.0f;
	TerrainInfo.HeightmapWidth = 2049;
	TerrainInfo.HeightmapHeight = 2049;
	TerrainInfo.CellSpacing = 1.0f;

	return TerrainInfo;
}


//Samplers
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GameApp::GetStaticSamplers()
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

void GameApp::LoadModel()
{
	mModelLoder->LoadModel("BlueTree/Blue_Tree_03a.fbx");
	mModelLoder->LoadModel("BlueTree/Blue_Tree_03b.fbx");
	mModelLoder->LoadModel("BlueTree/Blue_Tree_03c.fbx");
	mModelLoder->LoadModel("BlueTree/Blue_Tree_03d.fbx");
	mModelLoder->LoadModel("BlueTree/Blue_Tree_02a.fbx");
	mModelLoder->LoadModel("TraumaGuard/TraumaGuard.fbx");

	//AnimationPlayback["TraumaGuard@ActiveIdleLoop"].OnInitialize();
	//mModelLoder->LoadAnimation("TraumaGuard/TraumaGuard@ActiveIdleLoop.fbx");
	
}					



void GameApp::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;//渲染目标视图的堆描述
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;//深度视图的堆描述
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));


	// Create descriptor heaps for MSAA render target viewsand depth stencil views.
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
		rtvDescriptorHeapDesc.NumDescriptors = 1;
		rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc,
			IID_PPV_ARGS(m_msaaRTVDescriptorHeap.ReleaseAndGetAddressOf())));
	}
}



void GameApp::UpdateShadowPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mShadowMap->mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mShadowMap->mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mShadowMap->mLightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mShadowMap->mLightNearZ;
	mShadowPassCB.FarZ = mShadowMap->mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void GameApp::UpdateAnimation(const GameTimer& gt)
{
	auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();
	static float time = 0.0f;
	double time_in_sec = gt.TotalTime();
	time += gt.DeltaTime();
	static int j = 0;
	/*if (time > 0.3)
	{*/
		//transforms = AnimationPlayback["TraumaGuard@ActiveIdleLoop"].GetModelTransforms();
		time=0;
	//}
	


	/*if (time >= 3.0f)
	{*/
		mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].UpadateBoneTransform(time_in_sec, transforms);
		//mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].Calculate(time_in_sec);
		//time -= 3.0f;
		//mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].GetBoneMatrices(mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].pAnimeScene->mRootNode,x);
		/*int i = 0;
		for (auto e : mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].mTransforms)
		{
			transforms[i++]=(mModelLoder->mAnimations["TraumaGuard"]["ActiveIdleLoop"].AiToXM(e));
		}*/

		SkinnedConstants skinnedConstants;
		std::copy(
			std::begin(transforms),
			std::end(transforms),
			&skinnedConstants.BoneTransforms[0]);
		
		currSkinnedCB->CopyData(0, skinnedConstants);
	
	//}


}


void GameApp::DrawSceneToShadowMap()
{
	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

	UINT passCBByteSize = L3DUtil::CalcConstantBufferByteSize(sizeof(PassConstants));


	// Set null render target because we are only going to draw to
	// depth buffer.  Setting a null render target will disable color writes.
	// Note the active PSO also must specify a render target count of 0.
	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

	// Clear the back buffer and depth buffer.
	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH , 1.0f, 0, 0, nullptr);

	

	// Bind the pass constant buffer for the shadow map pass.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(1, passCBAddress);

	//Simple Instance  Shadow map
	mCommandList->SetPipelineState(mPSOs.get()->GetPSO(PSOName::InstanceSimpleShadow_Opaque).Get());
	PIXBeginEvent(mCommandList.Get(),0,"Draw Shadow::RenderLayer::InstanceSimpleItems");
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::InstanceSimpleItems]);
	PIXEndEvent(mCommandList.Get());

	PIXBeginEvent(mCommandList.Get(), 0, "Draw Shadow::RenderLayer::Opaque");
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
	PIXEndEvent(mCommandList.Get());

	
}
