#include "ProceduralTerrain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
#include "CompiledShaders/TerrainClipmapShadowDS.h"
#include "CompiledShaders/TerrainClipmapPS.h"
#include "CompiledShaders/TerrainClipmapDeferredPS.h"

using namespace DirectX;

namespace
{
    constexpr uint64_t kFnvOffset = 1469598103934665603ull;
    constexpr uint64_t kFnvPrime = 1099511628211ull;

    const char* const kNoiseTypeLabels[] =
    {
        "Billow", "Gabor", "Perlin", "Phasor", "Ridged", "Simplex",
        "Value", "Voronoi", "Wave", "White", "Worley"
    };

    const char* const kFractalTypeLabels[] = { "Single", "FBM", "Ridged" };
    const char* const kWarpModeLabels[] = { "None", "Fixed", "Recursive" };
    const char* const kCombineOpLabels[] = { "Add", "Multiply", "Subtract", "Min", "Max", "Blend" };
    const char* const kPlacementLabels[] = { "World", "Rotated", "Mirrored" };
    const char* const kVoronoiModeLabels[] = { "F1 Distance", "Distance To Edge" };

    void HashBytes(uint64_t& hash, const void* data, size_t size)
    {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= kFnvPrime;
        }
    }

    void HashInt(uint64_t& hash, int value)
    {
        HashBytes(hash, &value, sizeof(value));
    }

    void HashFloat(uint64_t& hash, float value)
    {
        HashBytes(hash, &value, sizeof(value));
    }

    void DrawTerrainNoiseLayerUI(const char* label, TerrainNoiseLayerSettings& layer)
    {
        if (!ImGui::TreeNode(label))
        {
            return;
        }

        ImGui::Checkbox("Enabled", &layer.Enabled);
        ImGui::Combo("Type", &layer.NoiseType, kNoiseTypeLabels, IM_ARRAYSIZE(kNoiseTypeLabels));
        ImGui::Combo("Fractal", &layer.FractalType, kFractalTypeLabels, IM_ARRAYSIZE(kFractalTypeLabels));
        ImGui::Combo("Operation", &layer.CombineOp, kCombineOpLabels, IM_ARRAYSIZE(kCombineOpLabels));
        ImGui::SliderFloat("Opacity", &layer.Opacity, 0.0f, 1.0f);
        ImGui::SliderFloat("Amplitude", &layer.Amplitude, 0.0f, 2.0f);
        ImGui::DragFloat("Frequency", &layer.Frequency, 0.0001f, 0.00001f, 0.1f, "%.6f");
        ImGui::SliderInt("Octaves", &layer.Octaves, 1, 8);
        ImGui::SliderFloat("Lacunarity", &layer.Lacunarity, 1.01f, 4.0f);
        ImGui::SliderFloat("Gain", &layer.Gain, 0.05f, 0.95f);
        ImGui::Combo("Placement", &layer.PlacementMode, kPlacementLabels, IM_ARRAYSIZE(kPlacementLabels));
        ImGui::DragFloat2("Offset", &layer.Offset.x, 1.0f, -10000.0f, 10000.0f);
        ImGui::DragFloat2("Scale", &layer.Scale.x, 0.01f, 0.01f, 20.0f);
        ImGui::SliderFloat("Rotation", &layer.Rotation, -3.14159f, 3.14159f);
        ImGui::Combo("Warp", &layer.WarpMode, kWarpModeLabels, IM_ARRAYSIZE(kWarpModeLabels));
        ImGui::SliderFloat("Warp Amplitude", &layer.WarpAmplitude, 0.0f, 300.0f);
        ImGui::DragFloat("Warp Frequency", &layer.WarpFrequency, 0.0001f, 0.00001f, 0.1f, "%.6f");
        if (layer.NoiseType == TerrainNoise_Voronoi || layer.NoiseType == TerrainNoise_Worley)
        {
            ImGui::Combo("Voronoi Mode", &layer.VoronoiMode, kVoronoiModeLabels, IM_ARRAYSIZE(kVoronoiModeLabels));
        }

        ImGui::TreePop();
    }

    bool SamePlane(const XMFLOAT4& left, const XMFLOAT4& right)
    {
        return left.x == right.x &&
               left.y == right.y &&
               left.z == right.z &&
               left.w == right.w;
    }

    void SortTiles(vector<ClipmapTile*>& tiles)
    {
        std::sort(tiles.begin(), tiles.end(),
            [](const ClipmapTile* left, const ClipmapTile* right)
            {
                if (left->LODLevel != right->LODLevel)
                {
                    return left->LODLevel < right->LODLevel;
                }
                if (left->GridCoord.y != right->GridCoord.y)
                {
                    return left->GridCoord.y < right->GridCoord.y;
                }
                return left->GridCoord.x < right->GridCoord.x;
            });
    }
}

ProceduralTerrain::ProceduralTerrain(
    ID3D12GraphicsCommandList* CmdList,
    const ProceduralTerrainInitInfo& Info)
    : mInitInfo(Info)
{
    mObjectType = ObjectType::Terrain;

    // Terrain shadow casting is opt-in because it is expensive with cascaded shadow maps.
    SetShadowFlag(false);
    SetShadowRenderFlag(false);

    // Initialize UI parameters from init info so first frame uses correct values
    mHeightScaleUI = Info.HeightScale;
    InitializeNoiseLayerPresets();

    // Create clipmap manager
    mClipmap = make_unique<TerrainClipmap>(
        Info.ClipmapLevels, Info.TileSize, Info.HeightmapSize, Info.CellSizeBase, g_Device);

    // Create GPU tile resources
    mClipmap->CreateTileResources(CmdList);

    // Create noise generator
    mNoiseGen = make_unique<TerrainNoiseGenerator>(g_Device, CmdList, Info);
    mNoiseGen->SetNoiseLayers(mNoiseLayersUI, kTerrainNoiseMaxLayers);

    // Build render pipeline
    BuildRootSignature();
    BuildPipelineState(CmdList);

    // Load material textures
    LoadMaterialTextures(CmdList);

    // Build shared patch geometry
    // Grid is (TileSize+2) x (TileSize+2): interior TileSize x TileSize
    // plus 1 extra row/column on each side for LOD skirt border quads.
    UINT gridSize = Info.TileSize + 2;
    UINT patchVerts = gridSize * gridSize;
    UINT patchQuads = (gridSize - 1) * (gridSize - 1);
    mIndexCount = patchQuads * 4;
    mVertexStride = sizeof(Vertices::Terrain);

    // Vertex buffer: Position(float3), Tex(float2), BoundsY(float2)
    // Interior vertices: tileUV in [0,1]. Border vertices: tileUV outside [0,1].
    vector<Vertices::Terrain> vertices(patchVerts);
    for (UINT i = 0; i < gridSize; i++)
    {
        for (UINT j = 0; j < gridSize; j++)
        {
            UINT idx = i * gridSize + j;
            vertices[idx].Position = XMFLOAT3((float)j - 1.0f, 0.0f, (float)i - 1.0f);
            vertices[idx].Tex = XMFLOAT2(
                ((float)j - 1.0f) / (float)(Info.TileSize - 1),
                ((float)i - 1.0f) / (float)(Info.TileSize - 1));
            vertices[idx].BoundsY = XMFLOAT2(-Info.HeightScale, Info.HeightScale);
        }
    }

    // Index buffer: 4 indices per quad patch
    vector<USHORT> indices(mIndexCount);
    UINT k = 0;
    for (UINT i = 0; i < gridSize - 1; i++)
    {
        for (UINT j = 0; j < gridSize - 1; j++)
        {
            indices[k++] = (USHORT)(i * gridSize + j);
            indices[k++] = (USHORT)(i * gridSize + j + 1);
            indices[k++] = (USHORT)((i + 1) * gridSize + j);
            indices[k++] = (USHORT)((i + 1) * gridSize + j + 1);
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
    ThrowIfFailed(mConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedCB)));

    // Create shadow pass constant buffer (separate ring buffer to prevent main pass overwrite)
    ThrowIfFailed(g_Device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&mShadowConstantBuffer)));
    mShadowCBGPU = mShadowConstantBuffer->GetGPUVirtualAddress();
    ThrowIfFailed(mShadowConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedShadowCB)));
}

ProceduralTerrain::~ProceduralTerrain()
{
    if (mConstantBuffer && mMappedCB)
    {
        mConstantBuffer->Unmap(0, nullptr);
        mMappedCB = nullptr;
    }
    if (mShadowConstantBuffer && mMappedShadowCB)
    {
        mShadowConstantBuffer->Unmap(0, nullptr);
        mMappedShadowCB = nullptr;
    }
}

void ProceduralTerrain::InitializeNoiseLayerPresets()
{
    for (UINT i = 0; i < kTerrainNoiseMaxLayers; ++i)
    {
        mNoiseLayersUI[i] = TerrainNoiseLayerSettings();
        mNoiseLayersUI[i].Enabled = false;
        mNoiseLayersUI[i].Octaves = 4;
        mNoiseLayersUI[i].Lacunarity = 2.0f;
        mNoiseLayersUI[i].Gain = 0.5f;
        mNoiseLayersUI[i].Scale = { 1.0f, 1.0f };
    }

    TerrainNoiseLayerSettings& base0 = mNoiseLayersUI[0];
    base0.Enabled = true;
    base0.NoiseType = TerrainNoise_Ridged;
    base0.FractalType = TerrainFractal_FBM;
    base0.CombineOp = TerrainCombine_Add;
    base0.WarpMode = TerrainWarp_Fixed;
    base0.Opacity = 1.0f;
    base0.Amplitude = 0.85f;
    base0.Frequency = 0.00065f;
    base0.Octaves = 7;
    base0.Lacunarity = 2.05f;
    base0.Gain = 0.50f;
    base0.WarpAmplitude = 35.0f;
    base0.WarpFrequency = 0.00045f;

    TerrainNoiseLayerSettings& base1 = mNoiseLayersUI[1];
    base1.Enabled = true;
    base1.NoiseType = TerrainNoise_Perlin;
    base1.FractalType = TerrainFractal_FBM;
    base1.CombineOp = TerrainCombine_Blend;
    base1.WarpMode = TerrainWarp_None;
    base1.Opacity = 0.35f;
    base1.Amplitude = 0.55f;
    base1.Frequency = 0.00022f;
    base1.Octaves = 5;
    base1.Lacunarity = 2.0f;
    base1.Gain = 0.55f;

    TerrainNoiseLayerSettings& detail0 = mNoiseLayersUI[kTerrainNoiseMaxBaseLayers];
    detail0.Enabled = true;
    detail0.NoiseType = TerrainNoise_Billow;
    detail0.FractalType = TerrainFractal_FBM;
    detail0.CombineOp = TerrainCombine_Add;
    detail0.WarpMode = TerrainWarp_Fixed;
    detail0.Opacity = 1.0f;
    detail0.Amplitude = 0.045f;
    detail0.Frequency = 0.0055f;
    detail0.Octaves = 4;
    detail0.Lacunarity = 2.2f;
    detail0.Gain = 0.48f;
    detail0.WarpAmplitude = 8.0f;
    detail0.WarpFrequency = 0.003f;

    TerrainNoiseLayerSettings& detail1 = mNoiseLayersUI[kTerrainNoiseMaxBaseLayers + 1];
    detail1.Enabled = true;
    detail1.NoiseType = TerrainNoise_Voronoi;
    detail1.FractalType = TerrainFractal_Single;
    detail1.CombineOp = TerrainCombine_Add;
    detail1.WarpMode = TerrainWarp_None;
    detail1.Opacity = 0.65f;
    detail1.Amplitude = 0.025f;
    detail1.Frequency = 0.011f;
    detail1.Octaves = 1;
    detail1.VoronoiMode = 1;
}

uint64_t ProceduralTerrain::ComputeNoiseLayerStateHash() const
{
    uint64_t hash = kFnvOffset;
    HashFloat(hash, mHeightScaleUI);
    HashFloat(hash, mSnowHeightUI);
    HashFloat(hash, mStoneSlopeUI);
    HashFloat(hash, mGrassCoverageUI);
    HashFloat(hash, mGrassPatchinessUI);
    HashFloat(hash, mGrassMaxSlopeUI);
    HashFloat(hash, mGrassMoistureBiasUI);

    for (UINT i = 0; i < kTerrainNoiseMaxLayers; ++i)
    {
        const TerrainNoiseLayerSettings& layer = mNoiseLayersUI[i];
        HashInt(hash, layer.Enabled ? 1 : 0);
        HashInt(hash, layer.NoiseType);
        HashInt(hash, layer.FractalType);
        HashInt(hash, layer.WarpMode);
        HashInt(hash, layer.CombineOp);
        HashInt(hash, layer.PlacementMode);
        HashInt(hash, layer.Octaves);
        HashInt(hash, layer.VoronoiMode);
        HashFloat(hash, layer.Opacity);
        HashFloat(hash, layer.Amplitude);
        HashFloat(hash, layer.Frequency);
        HashFloat(hash, layer.Lacunarity);
        HashFloat(hash, layer.Gain);
        HashFloat(hash, layer.WarpAmplitude);
        HashFloat(hash, layer.WarpFrequency);
        HashFloat(hash, layer.Rotation);
        HashFloat(hash, layer.Offset.x);
        HashFloat(hash, layer.Offset.y);
        HashFloat(hash, layer.Scale.x);
        HashFloat(hash, layer.Scale.y);
    }

    return hash;
}

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

    D3D12_RASTERIZER_DESC terrainShadowRaster = RasterizerShadowTwoSided;
    terrainShadowRaster.DepthBias = 4096;
    terrainShadowRaster.SlopeScaledDepthBias = 1.0f;
    terrainShadowRaster.DepthBiasClamp = 0.0f;

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

    // Shadow map casting PSO (VS+HS+DS, no pixel shader)
    // Renders to shadow buffer (D32_FLOAT, standard depth) with depth bias
    mTerrainCastShadowPSO.SetRootSignature(mTerrainRootSig);
    mTerrainCastShadowPSO.SetVertexShader(g_pTerrainClipmapVS, sizeof(g_pTerrainClipmapVS));
    mTerrainCastShadowPSO.SetHullShader(g_pTerrainClipmapHS, sizeof(g_pTerrainClipmapHS));
    mTerrainCastShadowPSO.SetDomainShader(g_pTerrainClipmapShadowDS, sizeof(g_pTerrainClipmapShadowDS));
    mTerrainCastShadowPSO.SetInputLayout(_countof(layout), layout);
    mTerrainCastShadowPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
    mTerrainCastShadowPSO.SetSampleMask(0xFFFFFFFF);
    mTerrainCastShadowPSO.SetDepthStencilState(DepthStateReadWrite);
    mTerrainCastShadowPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    mTerrainCastShadowPSO.SetRasterizerState(terrainShadowRaster);
    mTerrainCastShadowPSO.SetRenderTargetFormats(0, nullptr, DXGI_FORMAT_D32_FLOAT);
    mTerrainCastShadowPSO.Finalize();

    // Deferred rendering PSO - outputs to GBuffer (6 render targets)
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
            // PBR texture order from CreatePBRTextures: [0]albedo [1]normal [2]ao [3]metallic [4]roughness [5]height
            mLayerSRVIndices[layer][0] = AddToGlobalTextureSRVDescriptor(textures[0]->GetSRV()); // albedo
            mLayerSRVIndices[layer][1] = AddToGlobalTextureSRVDescriptor(textures[1]->GetSRV()); // normal
            mLayerSRVIndices[layer][2] = AddToGlobalTextureSRVDescriptor(textures[4]->GetSRV()); // roughness
            mLayerSRVIndices[layer][3] = AddToGlobalTextureSRVDescriptor(textures[3]->GetSRV()); // metallic
            mLayerSRVIndices[layer][4] = AddToGlobalTextureSRVDescriptor(textures[2]->GetSRV()); // ao
        }
        // Height map for height-based layer blending
        if (textures.size() >= 6)
        {
            mLayerHeightSRVIndices[layer] = AddToGlobalTextureSRVDescriptor(textures[5]->GetSRV());
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

    // Propagate UI params to noise generator before tile generation
    if (mNoiseGen)
    {
        mNoiseGen->SetHeightScale(mHeightScaleUI);
        mNoiseGen->SetNoiseLayers(mNoiseLayersUI, kTerrainNoiseMaxLayers);
        mNoiseGen->SetMaterialMaskParams(
            mSnowHeightUI,
            mStoneSlopeUI,
            mGrassCoverageUI,
            mGrassPatchinessUI,
            mGrassMaxSlopeUI,
            mGrassMoistureBiasUI);
    }
    mInitInfo.HeightScale = mHeightScaleUI;

    // Detect noise parameter changes and force full tile regeneration
    const uint64_t noiseLayerStateHash = ComputeNoiseLayerStateHash();
    if (mHeightScaleUI != mPrevHeightScale || noiseLayerStateHash != mPrevNoiseLayerStateHash)
    {
        mClipmap->ForceRegenerateAll();
        mPrevHeightScale = mHeightScaleUI;
        mPrevNoiseLayerStateHash = noiseLayerStateHash;
    }

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

    // Update base constant buffer (frustum, material indices)
    UpdateConstantBuffer();

    UpdateCulling(Context);

#ifdef DEBUG
    // Register first active tile textures in debug visualization panel
    const auto& debugActiveTiles = mClipmap->GetActiveTiles();
    if (!debugActiveTiles.empty())
    {
        ClipmapTile* dbgTile = debugActiveTiles[0];
        if (dbgTile && dbgTile->HeightMap)
        {
            float hmSize = (float)mInitInfo.HeightmapSize;
            EngineGUI::AddRenderPassVisualizeTexture("Terrain", "HeightMap",
                hmSize, hmSize, dbgTile->HeightSRV);
            EngineGUI::AddRenderPassVisualizeTexture("Terrain", "NormalMap",
                hmSize, hmSize, dbgTile->NormalSRV);
            EngineGUI::AddRenderPassVisualizeTexture("Terrain", "MaterialWeights",
                hmSize, hmSize, dbgTile->WeightSRV);
        }
    }

    if (mShowCullingDebugTextureUI && HasCullingDebugTexture())
    {
        EngineGUI::AddRenderPassVisualizeTexture("Terrain Culling", "Quadtree Culling",
            256.0f,
            256.0f,
            GetCullingDebugSRV());
    }
#endif
}

void ProceduralTerrain::UpdateConstantBuffer()
{
    if (!EngineGlobal::gCurrentScene) return;

    Camera* camera = EngineGlobal::gCurrentScene->GetCamera();
    if (!camera) return;

    XMMATRIX viewProj = camera->GetView() * camera->GetProj();
    XMStoreFloat4x4(&mConstants.ViewProj, viewProj);
    mConstants.EyePosW = camera->GetPosition3f();
    mConstants.MinTessDistance = 1.0f;
    mConstants.MaxTessDistance = mMaxTessDistUI;
    mConstants.MinTessFactor = 2.0f;
    mConstants.MaxTessFactor = mMaxTessFactorUI;
    mConstants.TessScale = mTessScaleUI;
    mConstants.HeightScale = mHeightScaleUI;
    mConstants.TexScale = mTexScaleUI;
    mConstants.RoughnessScale = mRoughnessScaleUI;
    mConstants.StochasticSharpness = mStochasticStrengthUI;

    // Extract frustum planes
    XMFLOAT4 planes[6];
    ExtractFrustumPlanes(planes, viewProj);
    memcpy(mConstants.gWorldFrustumPlanes, planes, sizeof(planes));

    // Material SRV indices (using .Index to match padded HLSL cbuffer layout)
    for (int i = 0; i < 5; i++)
    {
        mConstants.LayerAlbedoIndices[i].Index = mLayerSRVIndices[i][0];
        mConstants.LayerNormalIndices[i].Index = mLayerSRVIndices[i][1];
        mConstants.LayerRoughnessIndices[i].Index = mLayerSRVIndices[i][2];
        mConstants.LayerMetallicIndices[i].Index = mLayerSRVIndices[i][3];
        mConstants.LayerAOIndices[i].Index = mLayerSRVIndices[i][4];
        mConstants.LayerHeightIndices[i].Index = mLayerHeightSRVIndices[i];
    }

    // Previous frame data for motion vector computation (DLSS/FSR)
    XMStoreFloat4x4(&mConstants.PreViewProj, camera->GetPreViewProj());
    mConstants.PreJitterOffset = camera->GetPreJitter();
    mConstants.CurJitterOffset = camera->GetCurJitter();

    // Detail texture parameters (reuse base textures with higher tiling frequency)
    mConstants.DetailScale = mDetailScaleUI;
    mConstants.DetailFadeDistance = mDetailFadeDistanceUI;
    mConstants.UseHeightBlend = mUseHeightBlendUI ? 1 : 0;
    mConstants.UseTriplanar = mUseTriplanarUI ? 1 : 0;
    mConstants.ParallaxHeightScale = mParallaxHeightScaleUI;
    mConstants.UseTerrainParallax = mUseParallaxUI ? 1 : 0;
    for (int i = 0; i < 5; i++)
    {
        mConstants.DetailAlbedoIndices[i].Index = mLayerSRVIndices[i][0];    // reuse base albedo
        mConstants.DetailNormalIndices[i].Index = mLayerSRVIndices[i][1];    // reuse base normal
        mConstants.DetailRoughnessIndices[i].Index = mLayerSRVIndices[i][2]; // reuse base roughness
    }
}

void ProceduralTerrain::UpdateCulling(GraphicsContext* Context)
{
    const auto& activeTiles = mClipmap->GetActiveTiles();
    if (activeTiles.empty())
    {
        mCullingTree.Clear();
        mCullingItems.clear();
        mVisibleTiles.clear();
        mVisibleTileSet.clear();
        mVisibleCullingItems.clear();
        mCullingDebugPixels.clear();
        mCullingDebugTextureValid = false;
        mPrevCullingTileStates.clear();
        mHasPrevCullingFrustum = false;
        return;
    }

    vector<CullingTileState> tileStates;
    tileStates.reserve(activeTiles.size());
    for (const ClipmapTile* tile : activeTiles)
    {
        CullingTileState state;
        state.Tile = tile;
        if (tile)
        {
            state.GridCoord = tile->GridCoord;
            state.LODLevel = tile->LODLevel;
            state.HasHeightMap = tile->HeightMap != nullptr;
        }
        tileStates.push_back(state);
    }

    bool activeTilesChanged = tileStates.size() != mPrevCullingTileStates.size();
    if (!activeTilesChanged)
    {
        for (size_t i = 0; i < tileStates.size(); ++i)
        {
            const CullingTileState& current = tileStates[i];
            const CullingTileState& previous = mPrevCullingTileStates[i];
            if (current.Tile != previous.Tile ||
                current.GridCoord.x != previous.GridCoord.x ||
                current.GridCoord.y != previous.GridCoord.y ||
                current.LODLevel != previous.LODLevel ||
                current.HasHeightMap != previous.HasHeightMap)
            {
                activeTilesChanged = true;
                break;
            }
        }
    }

    bool frustumChanged = !mHasPrevCullingFrustum;
    if (!frustumChanged)
    {
        for (int i = 0; i < 6; ++i)
        {
            if (!SamePlane(mConstants.gWorldFrustumPlanes[i], mPrevCullingFrustumPlanes[i]))
            {
                frustumChanged = true;
                break;
            }
        }
    }

    const bool treeSettingsChanged =
        mCullingMaxDepthUI != mPrevCullingMaxDepthUI ||
        mCullingMaxItemsPerLeafUI != mPrevCullingMaxItemsPerLeafUI ||
        mHeightScaleUI != mPrevCullingHeightScaleUI ||
        mCullingMinHeightUI != mPrevCullingMinHeightUI ||
        mCullingMaxHeightUI != mPrevCullingMaxHeightUI ||
        mCullingHeightPaddingUI != mPrevCullingHeightPaddingUI;
    const bool cullingModeChanged = mEnableQuadtreeCullingUI != mPrevEnableQuadtreeCullingUI;
    const bool debugLODChanged = mDebugLODLevel != mPrevDebugLODLevel;
    const bool debugTextureModeChanged = mShowCullingDebugTextureUI != mPrevShowCullingDebugTextureUI;
    const bool rebuildTree = activeTilesChanged || treeSettingsChanged || mCullingTree.IsEmpty();
    const bool refreshVisibleTiles =
        rebuildTree ||
        frustumChanged ||
        cullingModeChanged ||
        debugLODChanged ||
        (debugTextureModeChanged && mShowCullingDebugTextureUI);

    SceneCullingBounds worldBounds = mCullingWorldBounds;
    bool hasBounds = false;
    vector<SceneCullingItem> newCullingItems;
    if (rebuildTree)
    {
        uint32_t itemId = 0;
        for (ClipmapTile* tile : activeTiles)
        {
            if (!tile || !tile->HeightMap)
            {
                continue;
            }

            const float cellSize = mClipmap->GetCellSize(tile->LODLevel);
            const float tileWorldSize = (float)mInitInfo.TileSize * cellSize;
            const float skirtDepth = mHeightScaleUI * 0.005f;

            const float cullMinHeight = (std::max)(
                -mHeightScaleUI - skirtDepth,
                (std::min)(mCullingMinHeightUI, mCullingMaxHeightUI) - mCullingHeightPaddingUI - skirtDepth);
            const float cullMaxHeight = (std::min)(
                mHeightScaleUI,
                (std::max)(mCullingMinHeightUI, mCullingMaxHeightUI) + mCullingHeightPaddingUI);
            SceneCullingBounds bounds;
            bounds.Min = {
                (float)tile->GridCoord.x * tileWorldSize,
                cullMinHeight,
                (float)tile->GridCoord.y * tileWorldSize
            };
            bounds.Max = {
                bounds.Min.x + tileWorldSize,
                cullMaxHeight,
                bounds.Min.z + tileWorldSize
            };

            SceneCullingItem item;
            item.Id = itemId++;
            item.UserData = tile;
            item.Bounds = bounds;
            item.Mask = 1u << (std::min)(tile->LODLevel, 31);
            newCullingItems.push_back(item);

            if (!hasBounds)
            {
                worldBounds = bounds;
                hasBounds = true;
            }
            else
            {
                worldBounds.Min.x = (std::min)(worldBounds.Min.x, bounds.Min.x);
                worldBounds.Min.y = (std::min)(worldBounds.Min.y, bounds.Min.y);
                worldBounds.Min.z = (std::min)(worldBounds.Min.z, bounds.Min.z);
                worldBounds.Max.x = (std::max)(worldBounds.Max.x, bounds.Max.x);
                worldBounds.Max.y = (std::max)(worldBounds.Max.y, bounds.Max.y);
                worldBounds.Max.z = (std::max)(worldBounds.Max.z, bounds.Max.z);
            }
        }

        if (!hasBounds)
        {
            mCullingTree.Clear();
            mCullingItems.clear();
            mVisibleTiles.clear();
            mVisibleTileSet.clear();
            mVisibleCullingItems.clear();
            mCullingDebugTextureValid = false;
            mPrevCullingTileStates = std::move(tileStates);
            return;
        }

        const float minSide = (std::max)(worldBounds.Max.x - worldBounds.Min.x, worldBounds.Max.z - worldBounds.Min.z);
        const float centerX = 0.5f * (worldBounds.Min.x + worldBounds.Max.x);
        const float centerZ = 0.5f * (worldBounds.Min.z + worldBounds.Max.z);
        const float halfSide = (std::max)(1.0f, minSide * 0.5f);
        worldBounds.Min.x = centerX - halfSide;
        worldBounds.Max.x = centerX + halfSide;
        worldBounds.Min.z = centerZ - halfSide;
        worldBounds.Max.z = centerZ + halfSide;

        mCullingWorldBounds = worldBounds;
        mCullingItems = std::move(newCullingItems);

        mCullingTree.Build(
            mCullingItems,
            worldBounds,
            (uint32_t)(std::max)(1, mCullingMaxDepthUI),
            (uint32_t)(std::max)(1, mCullingMaxItemsPerLeafUI));
    }

    if (refreshVisibleTiles)
    {
        mVisibleTiles.clear();
        mVisibleTileSet.clear();

        if (mEnableQuadtreeCullingUI)
        {
            QueryVisibleTiles(mConstants.gWorldFrustumPlanes, mVisibleTiles);
        }
        else
        {
            for (const SceneCullingItem& item : mCullingItems)
            {
                ClipmapTile* tile = static_cast<ClipmapTile*>(item.UserData);
                if (tile)
                {
                    mVisibleTiles.push_back(tile);
                }
            }
        }

        SortTiles(mVisibleTiles);

        for (ClipmapTile* tile : mVisibleTiles)
        {
            mVisibleTileSet.insert(tile);
        }
    }

#ifdef DEBUG
    const bool refreshDebugTexture =
        mShowCullingDebugTextureUI &&
        (refreshVisibleTiles ||
         rebuildTree ||
         treeSettingsChanged ||
         debugTextureModeChanged ||
         !mCullingDebugTextureValid);
    if (refreshDebugTexture)
    {
        UpdateCullingDebugTexture(Context);
    }
#endif

    mPrevCullingTileStates = std::move(tileStates);
    memcpy(mPrevCullingFrustumPlanes, mConstants.gWorldFrustumPlanes, sizeof(mPrevCullingFrustumPlanes));
    mHasPrevCullingFrustum = true;
    mPrevEnableQuadtreeCullingUI = mEnableQuadtreeCullingUI;
    mPrevShowCullingDebugTextureUI = mShowCullingDebugTextureUI;
    mPrevDebugLODLevel = mDebugLODLevel;
    mPrevCullingMaxDepthUI = mCullingMaxDepthUI;
    mPrevCullingMaxItemsPerLeafUI = mCullingMaxItemsPerLeafUI;
    mPrevCullingHeightScaleUI = mHeightScaleUI;
    mPrevCullingMinHeightUI = mCullingMinHeightUI;
    mPrevCullingMaxHeightUI = mCullingMaxHeightUI;
    mPrevCullingHeightPaddingUI = mCullingHeightPaddingUI;
}

void ProceduralTerrain::QueryVisibleTiles(const XMFLOAT4 FrustumPlanes[6], vector<ClipmapTile*>& VisibleTiles)
{
    VisibleTiles.clear();
    if (mCullingTree.IsEmpty())
    {
        return;
    }

    mCullingTree.QueryFrustum(FrustumPlanes, mVisibleCullingItems);
    for (const SceneCullingItem* item : mVisibleCullingItems)
    {
        ClipmapTile* tile = static_cast<ClipmapTile*>(item->UserData);
        if (!tile)
        {
            continue;
        }

        if (mDebugLODLevel >= 0 && tile->LODLevel != mDebugLODLevel)
        {
            continue;
        }

        if (mClipmap->IsCoveredByFinerLevel(tile))
        {
            continue;
        }

        VisibleTiles.push_back(tile);
    }
}

const vector<ClipmapTile*>& ProceduralTerrain::GetMainRenderTiles() const
{
    return mEnableQuadtreeCullingUI ? mVisibleTiles : mClipmap->GetActiveTiles();
}

void ProceduralTerrain::UpdateCullingDebugTexture(GraphicsContext* Context)
{
    constexpr uint32_t debugSize = 256;
    constexpr uint32_t colorBackground = 0xFF1E1E1Eu;
    constexpr uint32_t colorOutsideFrustum = 0xFF303030u;
    constexpr uint32_t colorInFrustum = 0xFF3030D0u;
    constexpr uint32_t colorCovered = 0xFFD08030u;
    constexpr uint32_t colorFiltered = 0xFF808080u;
    constexpr uint32_t colorIntersecting = 0xFF40D0D0u;
    constexpr uint32_t colorFrustum = 0xFFFF40FFu;
    constexpr uint32_t colorGrid = 0xFFFFFFFFu;

    mCullingDebugPixels.assign(debugSize * debugSize, colorBackground);

    const float minX = mCullingWorldBounds.Min.x;
    const float minZ = mCullingWorldBounds.Min.z;
    const float invWidth = 1.0f / (std::max)(1.0f, mCullingWorldBounds.Max.x - mCullingWorldBounds.Min.x);
    const float invDepth = 1.0f / (std::max)(1.0f, mCullingWorldBounds.Max.z - mCullingWorldBounds.Min.z);

    auto toPixelX = [&](float x)
    {
        float u = (x - minX) * invWidth;
        u = std::clamp(u, 0.0f, 1.0f);
        return (int)(u * (float)(debugSize - 1));
    };
    auto toPixelY = [&](float z)
    {
        float v = (z - minZ) * invDepth;
        v = std::clamp(v, 0.0f, 1.0f);
        return (int)((1.0f - v) * (float)(debugSize - 1));
    };
    auto testBounds = [&](const SceneCullingBounds& bounds)
    {
        bool fullyInside = true;
        const XMFLOAT3 center =
        {
            0.5f * (bounds.Min.x + bounds.Max.x),
            0.5f * (bounds.Min.y + bounds.Max.y),
            0.5f * (bounds.Min.z + bounds.Max.z)
        };
        const XMFLOAT3 extents =
        {
            0.5f * (bounds.Max.x - bounds.Min.x),
            0.5f * (bounds.Max.y - bounds.Min.y),
            0.5f * (bounds.Max.z - bounds.Min.z)
        };

        for (const XMFLOAT4& plane : mConstants.gWorldFrustumPlanes)
        {
            const float radius =
                extents.x * std::abs(plane.x) +
                extents.y * std::abs(plane.y) +
                extents.z * std::abs(plane.z);
            const float distance =
                plane.x * center.x +
                plane.y * center.y +
                plane.z * center.z +
                plane.w;

            if (distance + radius < 0.0f)
            {
                return SceneCullingResult::Culled;
            }
            if (distance - radius < 0.0f)
            {
                fullyInside = false;
            }
        }

        return fullyInside ? SceneCullingResult::Visible : SceneCullingResult::Intersecting;
    };
    auto fillRect = [&](const SceneCullingBounds& bounds, uint32_t color)
    {
        int x0 = toPixelX(bounds.Min.x);
        int x1 = toPixelX(bounds.Max.x);
        int y0 = toPixelY(bounds.Max.z);
        int y1 = toPixelY(bounds.Min.z);
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        x0 = std::clamp(x0, 0, (int)debugSize - 1);
        x1 = std::clamp(x1, 0, (int)debugSize - 1);
        y0 = std::clamp(y0, 0, (int)debugSize - 1);
        y1 = std::clamp(y1, 0, (int)debugSize - 1);
        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                mCullingDebugPixels[y * debugSize + x] = color;
            }
        }
    };
    auto drawRect = [&](const SceneCullingBounds& bounds, uint32_t color)
    {
        int x0 = toPixelX(bounds.Min.x);
        int x1 = toPixelX(bounds.Max.x);
        int y0 = toPixelY(bounds.Max.z);
        int y1 = toPixelY(bounds.Min.z);
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        x0 = std::clamp(x0, 0, (int)debugSize - 1);
        x1 = std::clamp(x1, 0, (int)debugSize - 1);
        y0 = std::clamp(y0, 0, (int)debugSize - 1);
        y1 = std::clamp(y1, 0, (int)debugSize - 1);
        for (int x = x0; x <= x1; ++x)
        {
            mCullingDebugPixels[y0 * debugSize + x] = color;
            mCullingDebugPixels[y1 * debugSize + x] = color;
        }
        for (int y = y0; y <= y1; ++y)
        {
            mCullingDebugPixels[y * debugSize + x0] = color;
            mCullingDebugPixels[y * debugSize + x1] = color;
        }
    };
    auto drawPixel = [&](int x, int y, uint32_t color)
    {
        if (x < 0 || x >= (int)debugSize || y < 0 || y >= (int)debugSize)
        {
            return;
        }
        mCullingDebugPixels[y * debugSize + x] = color;
    };
    auto drawLine = [&](const XMFLOAT2& start, const XMFLOAT2& end, uint32_t color)
    {
        int x0 = toPixelX(start.x);
        int y0 = toPixelY(start.y);
        const int x1 = toPixelX(end.x);
        const int y1 = toPixelY(end.y);

        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;

        for (;;)
        {
            drawPixel(x0, y0, color);
            drawPixel(x0 + 1, y0, color);
            drawPixel(x0, y0 + 1, color);

            if (x0 == x1 && y0 == y1)
            {
                break;
            }

            const int e2 = 2 * error;
            if (e2 >= dy)
            {
                error += dy;
                x0 += sx;
            }
            if (e2 <= dx)
            {
                error += dx;
                y0 += sy;
            }
        }
    };
    auto planeDistanceXZ = [](const XMFLOAT4& plane, const XMFLOAT2& point)
    {
        return plane.x * point.x + plane.z * point.y + plane.w;
    };
    auto clipPolygonToPlane = [&](const vector<XMFLOAT2>& polygon, const XMFLOAT4& plane)
    {
        vector<XMFLOAT2> clipped;
        if (polygon.empty())
        {
            return clipped;
        }

        XMFLOAT2 previous = polygon.back();
        float previousDistance = planeDistanceXZ(plane, previous);
        bool previousInside = previousDistance >= 0.0f;

        for (const XMFLOAT2& current : polygon)
        {
            const float currentDistance = planeDistanceXZ(plane, current);
            const bool currentInside = currentDistance >= 0.0f;

            if (currentInside != previousInside)
            {
                const float denom = previousDistance - currentDistance;
                if (std::abs(denom) > 0.000001f)
                {
                    const float t = previousDistance / denom;
                    clipped.push_back({
                        previous.x + (current.x - previous.x) * t,
                        previous.y + (current.y - previous.y) * t
                    });
                }
            }

            if (currentInside)
            {
                clipped.push_back(current);
            }

            previous = current;
            previousDistance = currentDistance;
            previousInside = currentInside;
        }

        return clipped;
    };

    for (const SceneCullingItem& item : mCullingItems)
    {
        ClipmapTile* tile = static_cast<ClipmapTile*>(item.UserData);
        if (!tile)
        {
            continue;
        }

        const SceneCullingResult frustumResult = testBounds(item.Bounds);
        uint32_t color = frustumResult == SceneCullingResult::Culled ? colorOutsideFrustum : colorInFrustum;
        if (mDebugLODLevel >= 0 && tile->LODLevel != mDebugLODLevel)
        {
            color = colorFiltered;
        }
        else if (frustumResult != SceneCullingResult::Culled && mClipmap->IsCoveredByFinerLevel(tile))
        {
            color = colorCovered;
        }
        fillRect(item.Bounds, color);
    }

    for (const SceneQuadtreeDebugNode& node : mCullingTree.GetDebugNodes())
    {
        const SceneCullingResult nodeResult = testBounds(node.Bounds);
        if (nodeResult == SceneCullingResult::Intersecting)
        {
            drawRect(node.Bounds, colorIntersecting);
        }
        else if (node.Depth <= 2)
        {
            drawRect(node.Bounds, colorGrid);
        }
    }

    vector<XMFLOAT2> frustumPolygon =
    {
        { mCullingWorldBounds.Min.x, mCullingWorldBounds.Min.z },
        { mCullingWorldBounds.Max.x, mCullingWorldBounds.Min.z },
        { mCullingWorldBounds.Max.x, mCullingWorldBounds.Max.z },
        { mCullingWorldBounds.Min.x, mCullingWorldBounds.Max.z }
    };

    for (const XMFLOAT4& plane : mConstants.gWorldFrustumPlanes)
    {
        const float xzNormalLengthSq = plane.x * plane.x + plane.z * plane.z;
        if (xzNormalLengthSq <= 0.000001f)
        {
            if (plane.w < 0.0f)
            {
                frustumPolygon.clear();
                break;
            }
            continue;
        }

        frustumPolygon = clipPolygonToPlane(frustumPolygon, plane);
        if (frustumPolygon.empty())
        {
            break;
        }
    }

    for (size_t i = 0; i < frustumPolygon.size(); ++i)
    {
        drawLine(frustumPolygon[i], frustumPolygon[(i + 1) % frustumPolygon.size()], colorFrustum);
    }

    if (!Context)
    {
        return;
    }

    EnsureCullingDebugTextureResources();

    D3D12_RESOURCE_DESC texDesc = mCullingDebugTextureResource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT64 uploadSize = 0;
    g_Device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, nullptr, nullptr, &uploadSize);

    auto uploadAlloc = Context->ReserveUploadMemory((size_t)uploadSize + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    const size_t alignedUploadOffset = Math::AlignUp(uploadAlloc.Offset, (size_t)D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    const size_t uploadOffsetAdjust = alignedUploadOffset - uploadAlloc.Offset;
    footprint.Offset += alignedUploadOffset;

    uint8_t* mappedData = static_cast<uint8_t*>(uploadAlloc.DataPtr) + uploadOffsetAdjust;
    for (uint32_t row = 0; row < debugSize; ++row)
    {
        memcpy(
            mappedData + row * footprint.Footprint.RowPitch,
            mCullingDebugPixels.data() + row * debugSize,
            debugSize * sizeof(uint32_t));
    }

    CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
        mCullingDebugTextureResource.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST);
    Context->GetCommandList()->ResourceBarrier(1, &toCopyDest);

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = mCullingDebugTextureResource.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = uploadAlloc.Buffer.GetResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = footprint;

    Context->GetCommandList()->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    CD3DX12_RESOURCE_BARRIER toShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(
        mCullingDebugTextureResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Context->GetCommandList()->ResourceBarrier(1, &toShaderResource);
    mCullingDebugTextureValid = true;
}

void ProceduralTerrain::EnsureCullingDebugTextureResources()
{
    constexpr uint32_t debugSize = 256;

    if (!mCullingDebugTextureResource)
    {
        D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R8G8B8A8_UNORM, debugSize, debugSize, 1, 1);
        CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(g_Device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&mCullingDebugTextureResource)));
        mCullingDebugTextureResource->SetName(L"Terrain Quadtree Culling Debug");

        if (mCullingDebugSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        {
            mCullingDebugSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        g_Device->CreateShaderResourceView(mCullingDebugTextureResource.Get(), &srvDesc, mCullingDebugSRV);
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
    vbv.SizeInBytes = (UINT)(mVertexStride * (mInitInfo.TileSize + 2) * (mInitInfo.TileSize + 2));
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
    const auto& activeTiles = GetMainRenderTiles();
    for (int i = (int)activeTiles.size() - 1; i >= 0; --i)
    {
        ClipmapTile* tile = activeTiles[i];
        if (!tile || !tile->HeightMap)
        {
            continue;
        }

        // Debug: filter to a single LOD level
        if (mDebugLODLevel >= 0 && tile->LODLevel != mDebugLODLevel)
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

        // Compute finer LOD level coverage bounds for patch-level clipping.
        // Shrink bounds by one coarse cell to create overlap zone at LOD boundary,
        // preventing gaps between LOD levels.
        if (tile->LODLevel > 0 && mClipmap->IsLevelInitialized(tile->LODLevel - 1))
        {
            UINT finerLevel = tile->LODLevel - 1;
            float finerCellSize = mClipmap->GetCellSize(finerLevel);
            float finerTileWorldSize = (float)mInitInfo.TileSize * finerCellSize;
            float coarseCellSize = mClipmap->GetCellSize(tile->LODLevel);
            XMINT2 finerOrigin = mClipmap->GetLevelOrigin(finerLevel);
            mConstants.HasFinerLevel = 1;
            mConstants.FinerLevelMinX = ((float)finerOrigin.x - 2.0f) * finerTileWorldSize + coarseCellSize;
            mConstants.FinerLevelMaxX = ((float)finerOrigin.x + 3.0f) * finerTileWorldSize - coarseCellSize;
            mConstants.FinerLevelMinZ = ((float)finerOrigin.y - 2.0f) * finerTileWorldSize + coarseCellSize;
            mConstants.FinerLevelMaxZ = ((float)finerOrigin.y + 3.0f) * finerTileWorldSize - coarseCellSize;
        }
        else
        {
            mConstants.HasFinerLevel = 0;
        }

        ComputeSkirtFlags(tile);

        // Upload to ring buffer slot (avoids overwriting in-flight CB data)
        memcpy(mMappedCB + ringIndex * cbAlignedSize, &mConstants, sizeof(ProceduralTerrainConstants));

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

void ProceduralTerrain::ComputeSkirtFlags(const ClipmapTile* tile)
{
    mConstants.SkirtEdgeFlags = 0;
    mConstants.SkirtDepth = mHeightScaleUI * 0.005f;

    // Only coarser tiles (LOD > 0) can have LOD boundary edges
    if (tile->LODLevel == 0)
        return;

    UINT finerLevel = tile->LODLevel - 1;
    if (!mClipmap->IsLevelInitialized(finerLevel))
        return;

    // Compute this tile's world-space bounds
    float cellSize = mClipmap->GetCellSize(tile->LODLevel);
    float tileWorldSize = (float)mInitInfo.TileSize * cellSize;
    float tileMinX = (float)tile->GridCoord.x * tileWorldSize;
    float tileMaxX = tileMinX + tileWorldSize;
    float tileMinZ = (float)tile->GridCoord.y * tileWorldSize;
    float tileMaxZ = tileMinZ + tileWorldSize;

    // Finer level's 5x5 coverage bounds
    float finerCellSize = mClipmap->GetCellSize(finerLevel);
    float finerTileWorldSize = (float)mInitInfo.TileSize * finerCellSize;
    XMINT2 finerOrigin = mClipmap->GetLevelOrigin(finerLevel);
    int halfTiles = 2;
    float finerMinX = ((float)finerOrigin.x - (float)halfTiles) * finerTileWorldSize;
    float finerMaxX = ((float)finerOrigin.x + (float)(halfTiles + 1)) * finerTileWorldSize;
    float finerMinZ = ((float)finerOrigin.y - (float)halfTiles) * finerTileWorldSize;
    float finerMaxZ = ((float)finerOrigin.y + (float)(halfTiles + 1)) * finerTileWorldSize;

    // For each edge, check if the finer level's coverage extends beyond that edge.
    // If the finer level is present on a given side, add a skirt to hide the LOD discontinuity.
    // Left edge: finer level extends to the left of this tile
    if (finerMinX < tileMinX) mConstants.SkirtEdgeFlags |= 1u;
    // Right edge: finer level extends to the right of this tile
    if (finerMaxX > tileMaxX) mConstants.SkirtEdgeFlags |= 2u;
    // Top edge: finer level extends above this tile
    if (finerMinZ < tileMinZ) mConstants.SkirtEdgeFlags |= 4u;
    // Bottom edge: finer level extends below this tile
    if (finerMaxZ > tileMaxZ) mConstants.SkirtEdgeFlags |= 8u;
}

void ProceduralTerrain::ComputeSkirtFlagsFor(const ClipmapTile* tile, ProceduralTerrainConstants& outConstants)
{
    outConstants.SkirtEdgeFlags = 0;
    outConstants.SkirtDepth = mHeightScaleUI * 0.005f;

    if (tile->LODLevel == 0)
        return;

    UINT finerLevel = tile->LODLevel - 1;
    if (!mClipmap->IsLevelInitialized(finerLevel))
        return;

    float cellSize = mClipmap->GetCellSize(tile->LODLevel);
    float tileWorldSize = (float)mInitInfo.TileSize * cellSize;
    float tileMinX = (float)tile->GridCoord.x * tileWorldSize;
    float tileMaxX = tileMinX + tileWorldSize;
    float tileMinZ = (float)tile->GridCoord.y * tileWorldSize;
    float tileMaxZ = tileMinZ + tileWorldSize;

    float finerCellSize = mClipmap->GetCellSize(finerLevel);
    float finerTileWorldSize = (float)mInitInfo.TileSize * finerCellSize;
    XMINT2 finerOrigin = mClipmap->GetLevelOrigin(finerLevel);
    int halfTiles = 2;
    float finerMinX = ((float)finerOrigin.x - (float)halfTiles) * finerTileWorldSize;
    float finerMaxX = ((float)finerOrigin.x + (float)(halfTiles + 1)) * finerTileWorldSize;
    float finerMinZ = ((float)finerOrigin.y - (float)halfTiles) * finerTileWorldSize;
    float finerMaxZ = ((float)finerOrigin.y + (float)(halfTiles + 1)) * finerTileWorldSize;

    if (finerMinX < tileMinX) outConstants.SkirtEdgeFlags |= 1u;
    if (finerMaxX > tileMaxX) outConstants.SkirtEdgeFlags |= 2u;
    if (finerMinZ < tileMinZ) outConstants.SkirtEdgeFlags |= 4u;
    if (finerMaxZ > tileMaxZ) outConstants.SkirtEdgeFlags |= 8u;
}

void ProceduralTerrain::DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO)
{
    // Ignore SpecificShadowPSO because terrain uses its own tessellation PSO.
    Context.SetRootSignature(mTerrainRootSig);
    Context.SetPipelineState(mTerrainCastShadowPSO);

    // Render target, depth target, and viewport are set by caller (ShadowMap::Draw or Scene)

    // DS needs gSRVMap[] for heightmap sampling
    Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        Render::gTextureHeap.GetHeapPointer());
    Context.SetDescriptorTable(3,
        Render::gTextureHeap[MAXCUBESRVSIZE + COMMONSRVSIZE + COMMONUAVSIZE]);

    // Set vertex and index buffers
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    vbv.SizeInBytes = (UINT)(mVertexStride * (mInitInfo.TileSize + 2) * (mInitInfo.TileSize + 2));
    vbv.StrideInBytes = mVertexStride;

    D3D12_INDEX_BUFFER_VIEW ibv = {};
    ibv.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = (UINT)mIndexCount * sizeof(USHORT);
    ibv.Format = DXGI_FORMAT_R16_UINT;

    Context.SetVertexBuffer(0, vbv);
    Context.SetIndexBuffer(ibv);
    Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

    // Prepare shadow constants with light VP.
    // Copy current per-frame constants as base, then override ViewProj and frustum planes.
    mShadowConstants = mConstants;
    XMMATRIX shadowVP = XMLoadFloat4x4(&mCachedShadowVP);
    XMStoreFloat4x4(&mShadowConstants.ViewProj, shadowVP);

    // Replace camera frustum planes with light frustum planes for correct shadow culling
    XMFLOAT4 lightFrustumPlanes[6];
    ExtractFrustumPlanes(lightFrustumPlanes, shadowVP);
    memcpy(mShadowConstants.gWorldFrustumPlanes, lightFrustumPlanes, sizeof(lightFrustumPlanes));

    const UINT cbAlignedSize = Math::AlignUp(sizeof(ProceduralTerrainConstants), 256u);
    const UINT cRingBufferSize = 256;
    UINT ringIndex = 0;

    vector<ClipmapTile*> shadowVisibleTiles;
    if (mEnableQuadtreeCullingUI)
    {
        QueryVisibleTiles(lightFrustumPlanes, shadowVisibleTiles);
        std::sort(shadowVisibleTiles.begin(), shadowVisibleTiles.end(),
            [](const ClipmapTile* left, const ClipmapTile* right)
            {
                if (left->LODLevel != right->LODLevel)
                {
                    return left->LODLevel < right->LODLevel;
                }
                if (left->GridCoord.y != right->GridCoord.y)
                {
                    return left->GridCoord.y < right->GridCoord.y;
                }
                return left->GridCoord.x < right->GridCoord.x;
            });
    }
    const auto& activeTiles = mEnableQuadtreeCullingUI ? shadowVisibleTiles : mClipmap->GetActiveTiles();
    for (int i = (int)activeTiles.size() - 1; i >= 0; --i)
    {
        ClipmapTile* tile = activeTiles[i];
        if (!tile || !tile->HeightMap) continue;
        if (mDebugLODLevel >= 0 && tile->LODLevel != mDebugLODLevel) continue;
        if (mClipmap->IsCoveredByFinerLevel(tile)) continue;

        // Per-tile constants
        mShadowConstants.HeightMapIndex = tile->HeightSRVIndex;
        mShadowConstants.NormalMapIndex = tile->NormalSRVIndex;
        mShadowConstants.MaterialWeightMapIndex = tile->WeightSRVIndex;
        mShadowConstants.ClipmapLevel = tile->LODLevel;
        mShadowConstants.CellSize = mClipmap->GetCellSize(tile->LODLevel);
        mShadowConstants.TileGridOffsetX = tile->GridCoord.x * (int)mInitInfo.TileSize;
        mShadowConstants.TileGridOffsetY = tile->GridCoord.y * (int)mInitInfo.TileSize;

        // Finer LOD level coverage bounds
        if (tile->LODLevel > 0 && mClipmap->IsLevelInitialized(tile->LODLevel - 1))
        {
            UINT finerLevel = tile->LODLevel - 1;
            float finerCellSize = mClipmap->GetCellSize(finerLevel);
            float finerTileWorldSize = (float)mInitInfo.TileSize * finerCellSize;
            float coarseCellSize = mClipmap->GetCellSize(tile->LODLevel);
            XMINT2 finerOrigin = mClipmap->GetLevelOrigin(finerLevel);
            mShadowConstants.HasFinerLevel = 1;
            mShadowConstants.FinerLevelMinX = ((float)finerOrigin.x - 2.0f) * finerTileWorldSize + coarseCellSize;
            mShadowConstants.FinerLevelMaxX = ((float)finerOrigin.x + 3.0f) * finerTileWorldSize - coarseCellSize;
            mShadowConstants.FinerLevelMinZ = ((float)finerOrigin.y - 2.0f) * finerTileWorldSize + coarseCellSize;
            mShadowConstants.FinerLevelMaxZ = ((float)finerOrigin.y + 3.0f) * finerTileWorldSize - coarseCellSize;
        }
        else
        {
            mShadowConstants.HasFinerLevel = 0;
        }

        ComputeSkirtFlagsFor(tile, mShadowConstants);

        // Upload to shadow ring buffer (separate from main pass ring buffer)
        memcpy(mMappedShadowCB + ringIndex * cbAlignedSize, &mShadowConstants, sizeof(ProceduralTerrainConstants));

        D3D12_GPU_VIRTUAL_ADDRESS tileCBAddr = mShadowCBGPU + ringIndex * cbAlignedSize;
        ringIndex = (ringIndex + 1) % cRingBufferSize;

        Context.SetConstantBuffer(1, tileCBAddr);

        // Transition tile resources for reading
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
    vbv.SizeInBytes = (UINT)(mVertexStride * (mInitInfo.TileSize + 2) * (mInitInfo.TileSize + 2));
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

    const auto& activeTiles = GetMainRenderTiles();
    for (int i = (int)activeTiles.size() - 1; i >= 0; --i)
    {
        ClipmapTile* tile = activeTiles[i];
        if (!tile || !tile->HeightMap) continue;

        // Debug: filter to a single LOD level
        if (mDebugLODLevel >= 0 && tile->LODLevel != mDebugLODLevel) continue;

        // Skip tiles entirely covered by a finer LOD level
        if (mClipmap->IsCoveredByFinerLevel(tile)) continue;

        mConstants.HeightMapIndex = tile->HeightSRVIndex;
        mConstants.NormalMapIndex = tile->NormalSRVIndex;
        mConstants.MaterialWeightMapIndex = tile->WeightSRVIndex;
        mConstants.ClipmapLevel = tile->LODLevel;
        mConstants.CellSize = mClipmap->GetCellSize(tile->LODLevel);
        mConstants.TileGridOffsetX = tile->GridCoord.x * (int)mInitInfo.TileSize;
        mConstants.TileGridOffsetY = tile->GridCoord.y * (int)mInitInfo.TileSize;

        // Compute finer LOD level coverage bounds for patch-level clipping.
        // Shrink bounds by one coarse cell to create overlap zone at LOD boundary.
        if (tile->LODLevel > 0 && mClipmap->IsLevelInitialized(tile->LODLevel - 1))
        {
            UINT finerLevel = tile->LODLevel - 1;
            float finerCellSize = mClipmap->GetCellSize(finerLevel);
            float finerTileWorldSize = (float)mInitInfo.TileSize * finerCellSize;
            float coarseCellSize = mClipmap->GetCellSize(tile->LODLevel);
            XMINT2 finerOrigin = mClipmap->GetLevelOrigin(finerLevel);
            mConstants.HasFinerLevel = 1;
            mConstants.FinerLevelMinX = ((float)finerOrigin.x - 2.0f) * finerTileWorldSize + coarseCellSize;
            mConstants.FinerLevelMaxX = ((float)finerOrigin.x + 3.0f) * finerTileWorldSize - coarseCellSize;
            mConstants.FinerLevelMinZ = ((float)finerOrigin.y - 2.0f) * finerTileWorldSize + coarseCellSize;
            mConstants.FinerLevelMaxZ = ((float)finerOrigin.y + 3.0f) * finerTileWorldSize - coarseCellSize;
        }
        else
        {
            mConstants.HasFinerLevel = 0;
        }

        ComputeSkirtFlags(tile);

        memcpy(mMappedCB + ringIndex * cbAlignedSize, &mConstants, sizeof(ProceduralTerrainConstants));

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
        if (ImGui::TreeNode("Generation"))
        {
            ImGui::SliderFloat("Height Scale", &mHeightScaleUI, 10.0f, 3000.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Base Layers"))
        {
            for (UINT layerIndex = 0; layerIndex < kTerrainNoiseMaxBaseLayers; ++layerIndex)
            {
                ImGui::PushID((int)layerIndex);
                char label[32];
                sprintf_s(label, "Base %u", layerIndex);
                DrawTerrainNoiseLayerUI(label, mNoiseLayersUI[layerIndex]);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Detail Layers"))
        {
            for (UINT layerIndex = 0; layerIndex < kTerrainNoiseMaxDetailLayers; ++layerIndex)
            {
                const UINT noiseLayerIndex = kTerrainNoiseMaxBaseLayers + layerIndex;
                ImGui::PushID((int)noiseLayerIndex);
                char label[32];
                sprintf_s(label, "Detail %u", layerIndex);
                DrawTerrainNoiseLayerUI(label, mNoiseLayersUI[noiseLayerIndex]);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Tessellation"))
        {
            ImGui::SliderFloat("Tess Scale", &mTessScaleUI, 0.25f, 4.0f);
            ImGui::SliderFloat("Max Tess Factor", &mMaxTessFactorUI, 2.0f, 32.0f);
            ImGui::SliderFloat("Tess Distance", &mMaxTessDistUI, 50.0f, 2000.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Materials"))
        {
            ImGui::SliderFloat("Tex Scale", &mTexScaleUI, 1.0f, 200.0f);
            ImGui::SliderFloat("Roughness Scale", &mRoughnessScaleUI, 0.1f, 10.0f);
            ImGui::SliderFloat("Stochastic Strength", &mStochasticStrengthUI, 0.0f, 1.0f);
            ImGui::SliderFloat("Snow Height", &mSnowHeightUI, 50.0f, 300.0f);
            ImGui::SliderFloat("Stone Slope", &mStoneSlopeUI, 0.1f, 1.0f);
            ImGui::SliderFloat("Grass Coverage", &mGrassCoverageUI, 0.0f, 2.0f);
            ImGui::SliderFloat("Grass Patchiness", &mGrassPatchinessUI, 0.0f, 1.0f);
            ImGui::SliderFloat("Grass Max Slope", &mGrassMaxSlopeUI, 0.05f, 0.8f);
            ImGui::SliderFloat("Grass Moisture Bias", &mGrassMoistureBiasUI, 0.0f, 1.0f);
            ImGui::SliderFloat("Detail Scale", &mDetailScaleUI, 1.0f, 100.0f);
            ImGui::SliderFloat("Detail Fade Dist", &mDetailFadeDistanceUI, 5.0f, 1000.0f);
            ImGui::Checkbox("Height Blend", &mUseHeightBlendUI);
            ImGui::Checkbox("Parallax Mapping", &mUseParallaxUI);
            ImGui::Checkbox("Triplanar Mapping", &mUseTriplanarUI);
            ImGui::SliderFloat("Parallax Height", &mParallaxHeightScaleUI, 0.0f, 0.1f);
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::Text("Active Tiles: %d  Dirty: %d",
            (int)mClipmap->GetActiveTiles().size(),
            (int)mClipmap->GetDirtyTiles().size());
        ImGui::Text("Culled Visible: %d / %d",
            (int)mVisibleTiles.size(),
            (int)mCullingItems.size());

        if (ImGui::TreeNode("Culling"))
        {
            ImGui::Checkbox("Enable Quadtree Culling", &mEnableQuadtreeCullingUI);
            ImGui::Checkbox("Show Culling Texture", &mShowCullingDebugTextureUI);
            ImGui::SliderInt("Max Depth", &mCullingMaxDepthUI, 1, 10);
            ImGui::SliderInt("Leaf Items", &mCullingMaxItemsPerLeafUI, 1, 16);
            ImGui::SliderFloat("Height Min", &mCullingMinHeightUI, -mHeightScaleUI, mHeightScaleUI);
            ImGui::SliderFloat("Height Max", &mCullingMaxHeightUI, -mHeightScaleUI, mHeightScaleUI);
            ImGui::SliderFloat("Height Padding", &mCullingHeightPaddingUI, 0.0f, 1000.0f);
            ImGui::TreePop();
        }

        // Debug LOD filter: -1 = render all levels, 0-9 = single level
        const char* lodLabels[] = {
            "All Levels", "LOD 0", "LOD 1", "LOD 2", "LOD 3",
            "LOD 4", "LOD 5", "LOD 6", "LOD 7", "LOD 8", "LOD 9"
        };
        int lodSelection = mDebugLODLevel + 1;
        if (ImGui::Combo("Debug LOD", &lodSelection, lodLabels, IM_ARRAYSIZE(lodLabels)))
            mDebugLODLevel = lodSelection - 1;
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
