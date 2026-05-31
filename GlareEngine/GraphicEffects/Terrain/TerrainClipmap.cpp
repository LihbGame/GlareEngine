#include "TerrainClipmap.h"
#include "Engine/EngineUtility.h"
#include "Engine/Vertex.h"

using namespace DirectX;

TerrainClipmap::TerrainClipmap(
    UINT ClipmapLevels,
    UINT TileSize,
    UINT HeightmapSize,
    float CellSizeBase,
    ID3D12Device* Device)
    : mClipmapLevels(ClipmapLevels)
    , mTileSize(TileSize)
    , mHeightmapSize(HeightmapSize)
    , mCellSizeBase(CellSizeBase)
    , mDevice(Device)
{
    // Create descriptor heap for per-tile resources
    // Each tile needs front/back SRV+UAV descriptors for atomic terrain updates.
    // Total tiles: up to 25 per level (5x5) * levels
    UINT maxTiles = 25 * mClipmapLevels;
    mDescriptorHeapSize = maxTiles * 12;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = mDescriptorHeapSize;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mTileDescriptorHeap)));

    // Initialize clipmap levels
    mLevels.resize(mClipmapLevels);
    for (UINT level = 0; level < mClipmapLevels; level++)
    {
        ClipmapLevel& lvl = mLevels[level];
        // 5x5 grid of tiles per level
        const int tilesPerSide = 5;
        lvl.Tiles.resize(tilesPerSide * tilesPerSide);
        for (int i = 0; i < tilesPerSide * tilesPerSide; i++)
        {
            lvl.Tiles[i] = make_unique<ClipmapTile>();
            lvl.Tiles[i]->LODLevel = level;
        }
    }
}

TerrainClipmap::~TerrainClipmap() = default;

CD3DX12_CPU_DESCRIPTOR_HANDLE TerrainClipmap::AllocateTileDescriptor()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        mTileDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(mNextDescriptorIndex,
        mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    mNextDescriptorIndex++;
    return handle;
}

void TerrainClipmap::CreateTileGPUResources(ClipmapTile* Tile, ID3D12GraphicsCommandList*)
{
    UINT texSize = mHeightmapSize;
    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);

    auto createTexture = [&](DXGI_FORMAT format, ComPtr<ID3D12Resource>& resource)
    {
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, texSize, texSize, 1, 1);
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr, IID_PPV_ARGS(&resource)));
    };

    auto createViews = [&](ComPtr<ID3D12Resource>& heightMap,
                           ComPtr<ID3D12Resource>& normalMap,
                           ComPtr<ID3D12Resource>& weightMap,
                           CD3DX12_CPU_DESCRIPTOR_HANDLE& heightSRV,
                           CD3DX12_CPU_DESCRIPTOR_HANDLE& normalSRV,
                           CD3DX12_CPU_DESCRIPTOR_HANDLE& weightSRV,
                           CD3DX12_CPU_DESCRIPTOR_HANDLE& heightUAV,
                           CD3DX12_CPU_DESCRIPTOR_HANDLE& normalUAV,
                           CD3DX12_CPU_DESCRIPTOR_HANDLE& weightUAV,
                           int& heightSRVIndex,
                           int& normalSRVIndex,
                           int& weightSRVIndex)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        heightSRV = AllocateTileDescriptor();
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        mDevice->CreateShaderResourceView(heightMap.Get(), &srvDesc, heightSRV);

        normalSRV = AllocateTileDescriptor();
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        mDevice->CreateShaderResourceView(normalMap.Get(), &srvDesc, normalSRV);

        weightSRV = AllocateTileDescriptor();
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        mDevice->CreateShaderResourceView(weightMap.Get(), &srvDesc, weightSRV);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        heightUAV = AllocateTileDescriptor();
        uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
        mDevice->CreateUnorderedAccessView(heightMap.Get(), nullptr, &uavDesc, heightUAV);

        normalUAV = AllocateTileDescriptor();
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        mDevice->CreateUnorderedAccessView(normalMap.Get(), nullptr, &uavDesc, normalUAV);

        weightUAV = AllocateTileDescriptor();
        uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        mDevice->CreateUnorderedAccessView(weightMap.Get(), nullptr, &uavDesc, weightUAV);

        heightSRVIndex = AddToGlobalTextureSRVDescriptor(heightSRV);
        normalSRVIndex = AddToGlobalTextureSRVDescriptor(normalSRV);
        weightSRVIndex = AddToGlobalTextureSRVDescriptor(weightSRV);
    };

    createTexture(DXGI_FORMAT_R32_FLOAT, Tile->HeightMap);
    createTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, Tile->NormalMap);
    createTexture(DXGI_FORMAT_R8G8B8A8_UNORM, Tile->MaterialWeightMap);
    createTexture(DXGI_FORMAT_R32_FLOAT, Tile->PendingHeightMap);
    createTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, Tile->PendingNormalMap);
    createTexture(DXGI_FORMAT_R8G8B8A8_UNORM, Tile->PendingMaterialWeightMap);

    createViews(
        Tile->HeightMap,
        Tile->NormalMap,
        Tile->MaterialWeightMap,
        Tile->HeightSRV,
        Tile->NormalSRV,
        Tile->WeightSRV,
        Tile->HeightUAV,
        Tile->NormalUAV,
        Tile->WeightUAV,
        Tile->HeightSRVIndex,
        Tile->NormalSRVIndex,
        Tile->WeightSRVIndex);

    createViews(
        Tile->PendingHeightMap,
        Tile->PendingNormalMap,
        Tile->PendingMaterialWeightMap,
        Tile->PendingHeightSRV,
        Tile->PendingNormalSRV,
        Tile->PendingWeightSRV,
        Tile->PendingHeightUAV,
        Tile->PendingNormalUAV,
        Tile->PendingWeightUAV,
        Tile->PendingHeightSRVIndex,
        Tile->PendingNormalSRVIndex,
        Tile->PendingWeightSRVIndex);

    Tile->IsDirty = true;
}

void TerrainClipmap::CreateTileResources(ID3D12GraphicsCommandList* CmdList)
{
    for (auto& level : mLevels)
    {
        for (auto& tile : level.Tiles)
        {
            if (!tile->HeightMap)
            {
                CreateTileGPUResources(tile.get(), CmdList);
            }
        }
    }
}

XMINT2 TerrainClipmap::WorldToGrid(const XMFLOAT3& WorldPos, UINT Level) const
{
    float cellSize = mCellSizeBase * (1u << Level) * mTileSize;
    return XMINT2(
        (int)floorf(WorldPos.x / cellSize),
        (int)floorf(WorldPos.z / cellSize)
    );
}

XMINT2 TerrainClipmap::SnapToGrid(const XMINT2& GridPos) const
{
    return XMINT2(
        GridPos.x >= 0 ? GridPos.x : GridPos.x - 4,
        GridPos.y >= 0 ? GridPos.y : GridPos.y - 4
    );
}

void TerrainClipmap::Update(const XMFLOAT3& CameraPos)
{
    mDirtyTiles.clear();
    mActiveTiles.clear();

    for (UINT level = 0; level < mClipmapLevels; level++)
    {
        XMINT2 newOrigin = WorldToGrid(CameraPos, level);
        ClipmapLevel& lvl = mLevels[level];

        if (newOrigin.x != lvl.CurrentOrigin.x || newOrigin.y != lvl.CurrentOrigin.y || !lvl.Initialized)
        {
            lvl.CurrentOrigin = newOrigin;
            lvl.Initialized = true;
            UpdateActiveTiles(level, newOrigin);
        }

        // Collect active and dirty tiles
        for (auto& tile : lvl.Tiles)
        {
            if (tile->IsActive)
            {
                mActiveTiles.push_back(tile.get());
                if (tile->IsDirty)
                {
                    mDirtyTiles.push_back(tile.get());
                }
            }
        }
    }
}

void TerrainClipmap::UpdateActiveTiles(UINT Level, const XMINT2& NewOrigin)
{
    ClipmapLevel& lvl = mLevels[Level];
    const int tilesPerSide = 5;
    int halfTiles = tilesPerSide / 2;

    for (int dy = -halfTiles; dy <= halfTiles; dy++)
    {
        for (int dx = -halfTiles; dx <= halfTiles; dx++)
        {
            int idx = (dy + halfTiles) * tilesPerSide + (dx + halfTiles);
            auto& tile = lvl.Tiles[idx];

            XMINT2 newCoord = { NewOrigin.x + dx, NewOrigin.y + dy };

            if (tile->GridCoord.x != newCoord.x || tile->GridCoord.y != newCoord.y)
            {
                tile->GridCoord = newCoord;
                tile->IsDirty = true;
                tile->HasPendingContent = false;
            }
            tile->IsActive = true;
        }
    }
}

ClipmapTile* TerrainClipmap::GetTile(UINT Level, XMINT2 GridCoord)
{
    if (Level >= mClipmapLevels) return nullptr;
    for (auto& tile : mLevels[Level].Tiles)
    {
        if (tile->GridCoord.x == GridCoord.x && tile->GridCoord.y == GridCoord.y)
            return tile.get();
    }
    return nullptr;
}

void TerrainClipmap::MarkTileGenerated(ClipmapTile* Tile)
{
    if (Tile)
    {
        Tile->IsDirty = false;
        Tile->PendingGridCoord = Tile->GridCoord;
        Tile->HasPendingContent = true;
    }
}

void TerrainClipmap::CommitTile(ClipmapTile* Tile)
{
    if (!Tile || !Tile->HasPendingContent)
    {
        return;
    }

    std::swap(Tile->HeightMap, Tile->PendingHeightMap);
    std::swap(Tile->NormalMap, Tile->PendingNormalMap);
    std::swap(Tile->MaterialWeightMap, Tile->PendingMaterialWeightMap);

    std::swap(Tile->HeightSRV, Tile->PendingHeightSRV);
    std::swap(Tile->NormalSRV, Tile->PendingNormalSRV);
    std::swap(Tile->WeightSRV, Tile->PendingWeightSRV);

    std::swap(Tile->HeightUAV, Tile->PendingHeightUAV);
    std::swap(Tile->NormalUAV, Tile->PendingNormalUAV);
    std::swap(Tile->WeightUAV, Tile->PendingWeightUAV);

    std::swap(Tile->HeightSRVIndex, Tile->PendingHeightSRVIndex);
    std::swap(Tile->NormalSRVIndex, Tile->PendingNormalSRVIndex);
    std::swap(Tile->WeightSRVIndex, Tile->PendingWeightSRVIndex);
    std::swap(Tile->NeedsSRVTransition, Tile->PendingNeedsSRVTransition);

    Tile->RenderGridCoord = Tile->PendingGridCoord;
    Tile->HasGeneratedContent = true;
    Tile->HasPendingContent = false;
}

void TerrainClipmap::ForceRegenerateAll()
{
    mDirtyTiles.clear();
    for (ClipmapTile* tile : mActiveTiles)
    {
        if (tile && tile->HeightMap)
        {
            tile->IsDirty = true;
            mDirtyTiles.push_back(tile);
        }
    }
}

bool TerrainClipmap::IsCoveredByFinerLevel(const ClipmapTile* Tile) const
{
    if (!Tile || Tile->LODLevel == 0) return false;

    UINT finerLevel = Tile->LODLevel - 1;
    if (finerLevel >= mClipmapLevels) return false;

    const ClipmapLevel& finer = mLevels[finerLevel];
    if (!finer.Initialized) return false;

    // This tile's world-space bounds
    float cellSize = mCellSizeBase * (1u << Tile->LODLevel);
    float tileWorldSize = (float)mTileSize * cellSize;
    float tileMinX = (float)Tile->GridCoord.x * tileWorldSize;
    float tileMaxX = tileMinX + tileWorldSize;
    float tileMinZ = (float)Tile->GridCoord.y * tileWorldSize;
    float tileMaxZ = tileMinZ + tileWorldSize;

    // Finer level's total world-space coverage
    float finerCellSize = mCellSizeBase * (1u << finerLevel);
    float finerTileWorldSize = (float)mTileSize * finerCellSize;
    int halfTiles = 2; // 5/2 = 2 (tiles go from origin-2 to origin+2)
    float finerMinX = ((float)finer.CurrentOrigin.x - (float)halfTiles) * finerTileWorldSize;
    float finerMaxX = ((float)finer.CurrentOrigin.x + (float)(halfTiles + 1)) * finerTileWorldSize;
    float finerMinZ = ((float)finer.CurrentOrigin.y - (float)halfTiles) * finerTileWorldSize;
    float finerMaxZ = ((float)finer.CurrentOrigin.y + (float)(halfTiles + 1)) * finerTileWorldSize;

    // Tile is covered if entirely within finer level's bounds
    return tileMinX >= finerMinX && tileMaxX <= finerMaxX &&
           tileMinZ >= finerMinZ && tileMaxZ <= finerMaxZ;
}

XMINT2 TerrainClipmap::GetLevelOrigin(UINT Level) const
{
    if (Level >= mClipmapLevels) return { 0, 0 };
    return mLevels[Level].CurrentOrigin;
}

bool TerrainClipmap::IsLevelInitialized(UINT Level) const
{
    if (Level >= mClipmapLevels) return false;
    return mLevels[Level].Initialized;
}

void TerrainClipmap::ActivateTilesForLevel(UINT Level, const XMINT2& NewOrigin)
{
    if (Level >= mClipmapLevels) return;
    mLevels[Level].CurrentOrigin = NewOrigin;
    UpdateActiveTiles(Level, NewOrigin);
}
