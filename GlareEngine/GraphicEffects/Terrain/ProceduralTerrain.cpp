#include "ProceduralTerrain.h"
#include "Engine/EngineUtility.h"
#include "Engine/EngineGlobal.h"
#include "Engine/Scene.h"
#include "Engine/Vertex.h"
#include "EngineGUI.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/BufferManager.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/Render.h"

#include "CompiledShaders/TerrainClipmapVS.h"
#include "CompiledShaders/TerrainClipmapHS.h"
#include "CompiledShaders/TerrainClipmapDS.h"
#include "CompiledShaders/TerrainClipmapPS.h"
#include "CompiledShaders/TerrainClipmapDeferredPS.h"

using namespace DirectX;

ProceduralTerrain::ProceduralTerrain(
    ID3D12GraphicsCommandList* CmdList,
    const ProceduralTerrainInitInfo& Info)
    : mInitInfo(Info)
{
    mObjectType = ObjectType::Terrain;

    // Create clipmap manager
    mClipmap = make_unique<TerrainClipmap>(
        Info.ClipmapLevels, Info.TileSize, Info.CellSizeBase, g_Device);

    // Create GPU tile resources
    mClipmap->CreateTileResources(CmdList);

    // Create noise generator
    mNoiseGen = make_unique<TerrainNoiseGenerator>(g_Device, CmdList, Info);

    // Build render pipeline
    BuildRootSignature();
    BuildPipelineState(CmdList);

    // Load material textures
    LoadMaterialTextures(CmdList);

    // Build shared patch geometry
    UINT patchVerts = Info.TileSize * Info.TileSize;
    UINT patchQuads = (Info.TileSize - 1) * (Info.TileSize - 1);
    mIndexCount = patchQuads * 4;
    mVertexStride = sizeof(Vertices::Terrain);

    // Vertex buffer: simple grid with Position(float3), Tex(float2), BoundsY(float2)
    vector<Vertices::Terrain> vertices(patchVerts);
    for (UINT i = 0; i < Info.TileSize; i++)
    {
        for (UINT j = 0; j < Info.TileSize; j++)
        {
            UINT idx = i * Info.TileSize + j;
            vertices[idx].Position = XMFLOAT3((float)j, 0.0f, (float)i);
            vertices[idx].Tex = XMFLOAT2(
                (float)j / (float)(Info.TileSize - 1),
                (float)i / (float)(Info.TileSize - 1));
            vertices[idx].BoundsY = XMFLOAT2(-Info.HeightScale, Info.HeightScale);
        }
    }

    // Index buffer: 4 indices per quad patch
    vector<USHORT> indices(mIndexCount);
    UINT k = 0;
    for (UINT i = 0; i < Info.TileSize - 1; i++)
    {
        for (UINT j = 0; j < Info.TileSize - 1; j++)
        {
            indices[k++] = (USHORT)(i * Info.TileSize + j);
            indices[k++] = (USHORT)(i * Info.TileSize + j + 1);
            indices[k++] = (USHORT)((i + 1) * Info.TileSize + j);
            indices[k++] = (USHORT)((i + 1) * Info.TileSize + j + 1);
        }
    }

    UINT vbSize = (UINT)vertices.size() * mVertexStride;
    UINT ibSize = (UINT)indices.size() * sizeof(USHORT);

    mVertexBuffer = EngineUtility::CreateDefaultBuffer(
        g_Device, CmdList, vertices.data(), vbSize, mVBUploader);
    mIndexBuffer = EngineUtility::CreateDefaultBuffer(
        g_Device, CmdList, indices.data(), ibSize, mIBUploader);

    // Create constant buffer (ring buffer of 256 entries to avoid per-tile overwrite)
    const UINT cRingBufferSize = 256;
    UINT cbSize = Math::AlignUp(sizeof(ProceduralTerrainConstants), 256u) * cRingBufferSize;
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
    ThrowIfFailed(g_Device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&mConstantBuffer)));
    mCBGPU = mConstantBuffer->GetGPUVirtualAddress();
}

ProceduralTerrain::~ProceduralTerrain() = default;

void ProceduralTerrain::BuildRootSignature()
{
    // 4 root params + 8 static samplers (matching CommonResource.hlsli s10-s17)
    mTerrainRootSig.Reset(4, 8);

    // [0] Main pass CB (b0) - view/proj/lights
    mTerrainRootSig[0].InitAsConstantBuffer(0);
    // [1] Terrain CB (b1)
    mTerrainRootSig[1].InitAsConstantBuffer(1);
    // [2] Material data structured buffer
    mTerrainRootSig[2].InitAsBufferSRV(1, 1); // space1
    // [3] Texture SRV table
    mTerrainRootSig[3].InitAsDescriptorRange(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 62, MAX2DSRVSIZE);

    // Static samplers matching CommonResource.hlsli registers
    SamplerDesc wrapDesc; wrapDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc anisoDesc; // default is anisotropic wrap, MaxAnisotropy=16
    SamplerDesc shadowDesc; shadowDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    shadowDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_BORDER);
    SamplerDesc clampDesc; clampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    clampDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    SamplerDesc volumeDesc; volumeDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    SamplerDesc pointClampDesc; pointClampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    pointClampDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    SamplerDesc pointBorderDesc; pointBorderDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    pointBorderDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_BORDER);
    SamplerDesc linearBorderDesc; linearBorderDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    linearBorderDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_BORDER);

    mTerrainRootSig.InitStaticSampler(10, wrapDesc);
    mTerrainRootSig.InitStaticSampler(11, anisoDesc);
    mTerrainRootSig.InitStaticSampler(12, shadowDesc);
    mTerrainRootSig.InitStaticSampler(13, clampDesc);
    mTerrainRootSig.InitStaticSampler(14, volumeDesc);
    mTerrainRootSig.InitStaticSampler(15, pointClampDesc);
    mTerrainRootSig.InitStaticSampler(16, pointBorderDesc);
    mTerrainRootSig.InitStaticSampler(17, linearBorderDesc);

    mTerrainRootSig.Finalize(L"ProceduralTerrain", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

void ProceduralTerrain::BuildPipelineState(ID3D12GraphicsCommandList*)
{
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BOUNDY",   0, DXGI_FORMAT_R32G32_FLOAT,    0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_RASTERIZER_DESC wireframeRaster = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    wireframeRaster.CullMode = D3D12_CULL_MODE_NONE;
    wireframeRaster.FillMode = D3D12_FILL_MODE_WIREFRAME;

    // Forward PSO (solid)
    mTerrainPSO.SetRootSignature(mTerrainRootSig);
    mTerrainPSO.SetVertexShader(g_pTerrainClipmapVS, sizeof(g_pTerrainClipmapVS));
    mTerrainPSO.SetHullShader(g_pTerrainClipmapHS, sizeof(g_pTerrainClipmapHS));
    mTerrainPSO.SetDomainShader(g_pTerrainClipmapDS, sizeof(g_pTerrainClipmapDS));
    mTerrainPSO.SetPixelShader(g_pTerrainClipmapPS, sizeof(g_pTerrainClipmapPS));
    mTerrainPSO.SetInputLayout(_countof(layout), layout);
    mTerrainPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
    mTerrainPSO.SetSampleMask(0xFFFFFFFF);
    mTerrainPSO.SetRenderTargetFormats(1, &DefaultHDRColorFormat,
        g_SceneDepthBuffer.GetFormat());
    mTerrainPSO.SetDepthStencilState(DepthStateReadWriteReversed);
    mTerrainPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    mTerrainPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    mTerrainPSO.Finalize();

    // Forward PSO (wireframe)
    mTerrainPSO_Wireframe = mTerrainPSO;
    mTerrainPSO_Wireframe.SetRasterizerState(wireframeRaster);
    mTerrainPSO_Wireframe.Finalize();

    // Depth-only PSO for Z-prepass (VS+HS+DS, no pixel shader)
    mTerrainShadowPSO.SetRootSignature(mTerrainRootSig);
    mTerrainShadowPSO.SetVertexShader(g_pTerrainClipmapVS, sizeof(g_pTerrainClipmapVS));
    mTerrainShadowPSO.SetHullShader(g_pTerrainClipmapHS, sizeof(g_pTerrainClipmapHS));
    mTerrainShadowPSO.SetDomainShader(g_pTerrainClipmapDS, sizeof(g_pTerrainClipmapDS));
    mTerrainShadowPSO.SetInputLayout(_countof(layout), layout);
    mTerrainShadowPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
    mTerrainShadowPSO.SetSampleMask(0xFFFFFFFF);
    mTerrainShadowPSO.SetDepthStencilState(DepthStateReadWriteReversed);
    mTerrainShadowPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    mTerrainShadowPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    mTerrainShadowPSO.SetRenderTargetFormats(0, nullptr, g_SceneDepthBuffer.GetFormat());
    mTerrainShadowPSO.Finalize();

    // Depth-only PSO (wireframe)
    mTerrainShadowPSO_Wireframe = mTerrainShadowPSO;
    mTerrainShadowPSO_Wireframe.SetRasterizerState(wireframeRaster);
    mTerrainShadowPSO_Wireframe.Finalize();

    // Deferred rendering PSO — outputs to GBuffer (6 render targets)
    if (Render::gRasterRenderPipelineType == RasterRenderPipelineType::TBDR ||
        Render::gRasterRenderPipelineType == RasterRenderPipelineType::CBDR)
    {
        mTerrainDeferredPSO.SetRootSignature(mTerrainRootSig);
        mTerrainDeferredPSO.SetVertexShader(g_pTerrainClipmapVS, sizeof(g_pTerrainClipmapVS));
        mTerrainDeferredPSO.SetHullShader(g_pTerrainClipmapHS, sizeof(g_pTerrainClipmapHS));
        mTerrainDeferredPSO.SetDomainShader(g_pTerrainClipmapDS, sizeof(g_pTerrainClipmapDS));
        mTerrainDeferredPSO.SetPixelShader(g_pTerrainClipmapDeferredPS, sizeof(g_pTerrainClipmapDeferredPS));
        mTerrainDeferredPSO.SetInputLayout(_countof(layout), layout);
        mTerrainDeferredPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
        mTerrainDeferredPSO.SetSampleMask(0xFFFFFFFF);
        mTerrainDeferredPSO.SetRenderTargetFormats(
            Render::GBUFFER_Count, Render::GBufferFormat,
            g_SceneDepthBuffer.GetFormat());
        mTerrainDeferredPSO.SetDepthStencilState(DepthStateReadWriteReversed);
        mTerrainDeferredPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
        mTerrainDeferredPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
        mTerrainDeferredPSO.Finalize();

        // Deferred PSO (wireframe)
        mTerrainDeferredPSO_Wireframe = mTerrainDeferredPSO;
        mTerrainDeferredPSO_Wireframe.SetRasterizerState(wireframeRaster);
        mTerrainDeferredPSO_Wireframe.Finalize();
    }
}

void ProceduralTerrain::LoadMaterialTextures(ID3D12GraphicsCommandList* CmdList)
{
    auto texMgr = TextureManager::GetInstance(CmdList);
    auto matMgr = MaterialManager::GetMaterialInstance();

    for (int layer = 0; layer < 5; layer++)
    {
        string filename = mInitInfo.LayerAssetPath + mInitInfo.LayerMapNames[layer];

        vector<Texture*> textures;
        texMgr->CreatePBRTextures(filename, textures);

        // Register material
        wstring wName = StringToWString(mInitInfo.LayerMapNames[layer]);
        matMgr->BuildMaterials(wName, textures, 0.04f,
            XMFLOAT4(1, 1, 1, 1), XMFLOAT3(0.1f, 0.1f, 0.1f),
            MathHelper::Identity4x4());

        // Store SRV indices via global descriptor table
        if (textures.size() >= 5)
        {
            // PBR texture order from CreatePBRTextures: [0]albedo [1]normal [2]ao [3]metallic [4]roughness
            mLayerSRVIndices[layer][0] = AddToGlobalTextureSRVDescriptor(textures[0]->GetSRV()); // albedo
            mLayerSRVIndices[layer][1] = AddToGlobalTextureSRVDescriptor(textures[1]->GetSRV()); // normal
            mLayerSRVIndices[layer][2] = AddToGlobalTextureSRVDescriptor(textures[4]->GetSRV()); // roughness
            mLayerSRVIndices[layer][3] = AddToGlobalTextureSRVDescriptor(textures[3]->GetSRV()); // metallic
            mLayerSRVIndices[layer][4] = AddToGlobalTextureSRVDescriptor(textures[2]->GetSRV()); // ao
        }
    }
}

void ProceduralTerrain::Update(float dt, GraphicsContext* Context)
{
    if (!EngineGlobal::gCurrentScene) return;

    Camera* camera = EngineGlobal::gCurrentScene->GetCamera();
    if (!camera) return;

    XMFLOAT3 camPos = camera->GetPosition3f();

    // Update clipmap based on camera position
    mClipmap->Update(camPos);

    // Generate dirty tiles via compute
    const auto& dirtyTiles = mClipmap->GetDirtyTiles();
    if (!dirtyTiles.empty() && Context)
    {
        ComputeContext& computeCtx = Context->GetComputeContext();
        mNoiseGen->GenerateTiles(computeCtx, dirtyTiles, camPos, mInitInfo.CellSizeBase);

        // Mark tiles as clean after generation
        for (auto* tile : dirtyTiles)
        {
            mClipmap->MarkTileClean(tile);
        }
    }

#ifdef DEBUG
    // Register first active tile textures in debug visualization panel
    const auto& debugActiveTiles = mClipmap->GetActiveTiles();
    if (!debugActiveTiles.empty())
    {
        ClipmapTile* dbgTile = debugActiveTiles[0];
        if (dbgTile && dbgTile->HeightMap)
        {
            float tileSz = (float)mInitInfo.TileSize;
            EngineGUI::AddRenderPassVisualizeTexture("Terrain", "HeightMap",
                tileSz, tileSz, dbgTile->HeightSRV);
            EngineGUI::AddRenderPassVisualizeTexture("Terrain", "NormalMap",
                tileSz, tileSz, dbgTile->NormalSRV);
            EngineGUI::AddRenderPassVisualizeTexture("Terrain", "MaterialWeights",
                tileSz, tileSz, dbgTile->WeightSRV);
        }
    }
#endif

    // Update base constant buffer (frustum, material indices)
    UpdateConstantBuffer();
}

void ProceduralTerrain::UpdateConstantBuffer()
{
    if (!EngineGlobal::gCurrentScene) return;

    Camera* camera = EngineGlobal::gCurrentScene->GetCamera();
    if (!camera) return;

    XMMATRIX viewProj = camera->GetView() * camera->GetProj();
    XMStoreFloat4x4(&mConstants.ViewProj, viewProj);
    mConstants.EyePosW = camera->GetPosition3f();
    mConstants.MinTessDistance = 10.0f;
    mConstants.MaxTessDistance = 500.0f;
    mConstants.MinTessFactor = 2.0f;
    mConstants.MaxTessFactor = 6.0f;
    mConstants.HeightScale = mInitInfo.HeightScale;
    mConstants.TexScale = 50.0f;
    mConstants.StochasticSharpness = 0.95f;

    // Extract frustum planes
    XMFLOAT4 planes[6];
    ExtractFrustumPlanes(planes, viewProj);
    memcpy(mConstants.gWorldFrustumPlanes, planes, sizeof(planes));

    // Material SRV indices
    for (int i = 0; i < 5; i++)
    {
        mConstants.LayerAlbedoIndices[i] = mLayerSRVIndices[i][0];
        mConstants.LayerNormalIndices[i] = mLayerSRVIndices[i][1];
        mConstants.LayerRoughnessIndices[i] = mLayerSRVIndices[i][2];
        mConstants.LayerMetallicIndices[i] = mLayerSRVIndices[i][3];
        mConstants.LayerAOIndices[i] = mLayerSRVIndices[i][4];
    }
}

void ProceduralTerrain::Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
{
    bool isWireframe = Render::gCommonProperty.IsWireframe;

    GraphicsPSO* pso = &mTerrainPSO;
    if (!SpecificPSO &&
        (Render::gRasterRenderPipelineType == RasterRenderPipelineType::TBDR ||
         Render::gRasterRenderPipelineType == RasterRenderPipelineType::CBDR))
    {
        pso = isWireframe ? &mTerrainDeferredPSO_Wireframe : &mTerrainDeferredPSO;
    }
    else if (isWireframe)
    {
        pso = &mTerrainPSO_Wireframe;
    }

    Context.SetRootSignature(mTerrainRootSig);
    Context.SetPipelineState(SpecificPSO ? *SpecificPSO : *pso);

    // Ensure viewport matches scene buffer dimensions
    Context.SetViewportAndScissor(0, 0, g_SceneDepthBuffer.GetWidth(), g_SceneDepthBuffer.GetHeight());

    // Bind main pass CB (b0) for lights/ambient/shadow
    if (mMainCBData && mMainCBSize > 0)
        Context.SetDynamicConstantBufferView(0, mMainCBSize, mMainCBData);

    // Bind global texture SRV heap for gSRVMap[] access in shaders
    Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        Render::gTextureHeap.GetHeapPointer());
    Context.SetDescriptorTable(3,
        Render::gTextureHeap[MAXCUBESRVSIZE + COMMONSRVSIZE + COMMONUAVSIZE]);

    // Set vertex and index buffers
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    vbv.SizeInBytes = (UINT)(mVertexStride * mInitInfo.TileSize * mInitInfo.TileSize);
    vbv.StrideInBytes = mVertexStride;

    D3D12_INDEX_BUFFER_VIEW ibv = {};
    ibv.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = (UINT)mIndexCount * sizeof(USHORT);
    ibv.Format = DXGI_FORMAT_R16_UINT;

    Context.SetVertexBuffer(0, vbv);
    Context.SetIndexBuffer(ibv);

    // Set primitive topology for tessellation
    Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

    // Ring buffer offset for per-tile CB uploads
    const UINT cbAlignedSize = Math::AlignUp(sizeof(ProceduralTerrainConstants), 256u);
    const UINT cRingBufferSize = 256;
    UINT ringIndex = 0;

    // Draw tiles coarse-to-fine so fine LOD overwrites coarse in overlap regions
    const auto& activeTiles = mClipmap->GetActiveTiles();
    for (int i = (int)activeTiles.size() - 1; i >= 0; --i)
    {
        ClipmapTile* tile = activeTiles[i];
        if (!tile || !tile->HeightMap)
        {
            continue;
        }

        // Skip tiles entirely covered by a finer LOD level
        if (mClipmap->IsCoveredByFinerLevel(tile))
        {
            continue;
        }

        // Update per-tile constants
        mConstants.HeightMapIndex = tile->HeightSRVIndex;
        mConstants.NormalMapIndex = tile->NormalSRVIndex;
        mConstants.MaterialWeightMapIndex = tile->WeightSRVIndex;
        mConstants.ClipmapLevel = tile->LODLevel;
        mConstants.CellSize = mClipmap->GetCellSize(tile->LODLevel);
        mConstants.TileGridOffsetX = tile->GridCoord.x * (int)mInitInfo.TileSize;
        mConstants.TileGridOffsetY = tile->GridCoord.y * (int)mInitInfo.TileSize;

        // Upload to ring buffer slot (avoids overwriting in-flight CB data)
        UINT8* pData = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        D3D12_RANGE writeRange = { ringIndex * cbAlignedSize, (ringIndex + 1) * cbAlignedSize };
        mConstantBuffer->Map(0, &readRange, (void**)&pData);
        memcpy(pData + ringIndex * cbAlignedSize, &mConstants, sizeof(ProceduralTerrainConstants));
        mConstantBuffer->Unmap(0, &writeRange);

        D3D12_GPU_VIRTUAL_ADDRESS tileCBAddr = mCBGPU + ringIndex * cbAlignedSize;
        ringIndex = (ringIndex + 1) % cRingBufferSize;

        Context.SetConstantBuffer(1, tileCBAddr);

        // Transition tile resources for reading.
        // After compute generation, tiles are in UAV state and need transition.
        if (tile->NeedsSRVTransition)
        {
            CD3DX12_RESOURCE_BARRIER barriers[3] = {
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->HeightMap.Get(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->NormalMap.Get(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->MaterialWeightMap.Get(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
            };
            Context.GetCommandList()->ResourceBarrier(3, barriers);
            tile->NeedsSRVTransition = false;
        }

        // Draw
        Context.DrawIndexed(mIndexCount);
    }
}

void ProceduralTerrain::DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO)
{
    // TODO: Phase 4 - shadow rendering for procedural terrain
}

void ProceduralTerrain::DrawDepthOnly(GraphicsContext& Context)
{
    Context.SetRootSignature(mTerrainRootSig);
    Context.SetPipelineState(Render::gCommonProperty.IsWireframe ? mTerrainShadowPSO_Wireframe : mTerrainShadowPSO);

    // Bind depth-only output target
    Context.SetDepthStencilTarget(g_SceneDepthBuffer.GetDSV());
    Context.SetViewportAndScissor(0, 0, g_SceneDepthBuffer.GetWidth(), g_SceneDepthBuffer.GetHeight());

    // DS needs gSRVMap[] for heightmap sampling
    Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        Render::gTextureHeap.GetHeapPointer());
    Context.SetDescriptorTable(3,
        Render::gTextureHeap[MAXCUBESRVSIZE + COMMONSRVSIZE + COMMONUAVSIZE]);

    // Set vertex and index buffers
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    vbv.SizeInBytes = (UINT)(mVertexStride * mInitInfo.TileSize * mInitInfo.TileSize);
    vbv.StrideInBytes = mVertexStride;

    D3D12_INDEX_BUFFER_VIEW ibv = {};
    ibv.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = (UINT)mIndexCount * sizeof(USHORT);
    ibv.Format = DXGI_FORMAT_R16_UINT;

    Context.SetVertexBuffer(0, vbv);
    Context.SetIndexBuffer(ibv);
    Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

    const UINT cbAlignedSize = Math::AlignUp(sizeof(ProceduralTerrainConstants), 256u);
    const UINT cRingBufferSize = 256;
    UINT ringIndex = 0;

    const auto& activeTiles = mClipmap->GetActiveTiles();
    for (int i = (int)activeTiles.size() - 1; i >= 0; --i)
    {
        ClipmapTile* tile = activeTiles[i];
        if (!tile || !tile->HeightMap) continue;

        // Skip tiles entirely covered by a finer LOD level
        if (mClipmap->IsCoveredByFinerLevel(tile)) continue;

        mConstants.HeightMapIndex = tile->HeightSRVIndex;
        mConstants.NormalMapIndex = tile->NormalSRVIndex;
        mConstants.MaterialWeightMapIndex = tile->WeightSRVIndex;
        mConstants.ClipmapLevel = tile->LODLevel;
        mConstants.CellSize = mClipmap->GetCellSize(tile->LODLevel);
        mConstants.TileGridOffsetX = tile->GridCoord.x * (int)mInitInfo.TileSize;
        mConstants.TileGridOffsetY = tile->GridCoord.y * (int)mInitInfo.TileSize;

        UINT8* pData = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        D3D12_RANGE writeRange = { ringIndex * cbAlignedSize, (ringIndex + 1) * cbAlignedSize };
        mConstantBuffer->Map(0, &readRange, (void**)&pData);
        memcpy(pData + ringIndex * cbAlignedSize, &mConstants, sizeof(ProceduralTerrainConstants));
        mConstantBuffer->Unmap(0, &writeRange);

        D3D12_GPU_VIRTUAL_ADDRESS tileCBAddr = mCBGPU + ringIndex * cbAlignedSize;
        ringIndex = (ringIndex + 1) % cRingBufferSize;

        Context.SetConstantBuffer(1, tileCBAddr);

        if (tile->NeedsSRVTransition)
        {
            CD3DX12_RESOURCE_BARRIER barriers[3] = {
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->HeightMap.Get(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->NormalMap.Get(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->MaterialWeightMap.Get(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
            };
            Context.GetCommandList()->ResourceBarrier(3, barriers);
            tile->NeedsSRVTransition = false;
        }

        Context.DrawIndexed(mIndexCount);
    }
}

void ProceduralTerrain::DrawUI()
{
    if (ImGui::CollapsingHeader("Procedural Terrain", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Noise Scale", &mNoiseScaleUI, 0.001f, 0.1f);
        ImGui::SliderFloat("Height Scale", &mHeightScaleUI, 10.0f, 500.0f);
        ImGui::SliderFloat("Warp Strength", &mWarpStrengthUI, 0.0f, 100.0f);
        ImGui::SliderFloat("Snow Height", &mSnowHeightUI, 50.0f, 300.0f);
        ImGui::SliderFloat("Stone Slope", &mStoneSlopeUI, 0.1f, 1.0f);

        // Propagate UI params to noise generator
        if (mNoiseGen)
        {
            mNoiseGen->SetNoiseScale(mNoiseScaleUI);
            mNoiseGen->SetHeightScale(mHeightScaleUI);
        }

        ImGui::Separator();
        ImGui::Text("Active Tiles: %d", (int)mClipmap->GetActiveTiles().size());
        ImGui::Text("Dirty Tiles: %d", (int)mClipmap->GetDirtyTiles().size());

        // Show debug info for first active tile
        const auto& activeTiles = mClipmap->GetActiveTiles();
        if (!activeTiles.empty())
        {
            ClipmapTile* tile = activeTiles[0];
            ImGui::Separator();
            ImGui::Text("Debug Tile (first active):");
            ImGui::Text("  Grid: (%d, %d)  LOD: %d",
                tile->GridCoord.x, tile->GridCoord.y, tile->LODLevel);
            ImGui::Text("  CellSize: %.2f  Dirty: %s",
                mClipmap->GetCellSize(tile->LODLevel),
                tile->IsDirty ? "yes" : "no");
            ImGui::Text("  HeightSRV: %d  NormalSRV: %d  WeightSRV: %d",
                tile->HeightSRVIndex, tile->NormalSRVIndex, tile->WeightSRVIndex);
        }
    }
}

void ProceduralTerrain::InitMaterial()
{
    // Materials are loaded in constructor via LoadMaterialTextures
}

float ProceduralTerrain::GetHeight(float x, float z) const
{
    // TODO: Implement CPU-side noise evaluation for game logic queries
    return 0.0f;
}
