# Procedural Infinite Terrain System - Technical Design Document

> **Version:** 2.0 (Updated post-implementation)  
> **Date:** 2026-04-20  
> **Status:** Phase 1-3 implemented, Phase 4 pending

## Context

The current GlareEngine terrain (`GraphicEffects/Terrain/`) uses a single heightmap with DX tessellation, identical to the legacy EngineDemo_Old approach. It's limited to a fixed-size heightmap (e.g. 2049x2049), has incomplete shaders (DS commented out, PS returns white), and suffers from texture tiling artifacts. The new system builds an infinite procedural terrain that generates terrain on the GPU, uses geometry clipmap LOD for scalability, reduces tiling via stochastic sampling, and distributes 5 material layers via procedural height/slope rules.

---

## Architecture Overview

```
ProceduralTerrain (RenderObject)
  +-- TerrainClipmap          -- manages LOD levels and chunk lifecycle
  +-- TerrainNoiseGenerator   -- GPU compute dispatch for height + normal + material weights
  +-- Tessellation Pipeline   -- VS/HS/DS/PS for terrain rendering (inside ProceduralTerrain)
```

### File List

#### New Files (Created)

| File | Purpose |
|------|---------|
| `GraphicEffects/Terrain/ProceduralTerrain.h` | Main terrain class header |
| `GraphicEffects/Terrain/ProceduralTerrain.cpp` | Main terrain class implementation |
| `GraphicEffects/Terrain/TerrainClipmap.h` | Clipmap level/chunk manager header |
| `GraphicEffects/Terrain/TerrainClipmap.cpp` | Clipmap level/chunk manager implementation |
| `GraphicEffects/Terrain/TerrainNoiseGenerator.h` | GPU noise generation interface header |
| `GraphicEffects/Terrain/TerrainNoiseGenerator.cpp` | GPU noise generation implementation |
| `Shaders/Terrain/TerrainCommon.hlsli` | Shared terrain structures, CBuffers, stochastic utilities |
| `Shaders/Terrain/TerrainNoiseCS.hlsl` | Compute shader: heightmap + normalmap + material weights |
| `Shaders/Terrain/TerrainClipmapVS.hlsl` | Vertex shader for clipmap patches |
| `Shaders/Terrain/TerrainClipmapHS.hlsl` | Hull shader with clipmap-aware tessellation + frustum culling |
| `Shaders/Terrain/TerrainClipmapDS.hlsl` | Domain shader with heightmap displacement |
| `Shaders/Terrain/TerrainClipmapPS.hlsl` | Pixel shader with stochastic sampling + PBR material blending |

#### Modified Files

| File | Change |
|------|--------|
| `GraphicEffects/Misc/ConstantBuffer.h` | Added `ProceduralTerrainNoiseCB`, `ProceduralTerrainConstants`, `ProceduralTerrainInitInfo` |

#### Unchanged Files (Planned but not needed)

| File | Reason |
|------|--------|
| `Shaders/Misc/CommonResource.hlsli` | Terrain shader structs placed in `TerrainCommon.hlsli` instead (cleaner separation) |
| `CMakeLists.txt` | Glob patterns auto-discover new shaders by suffix convention (`*CS.hlsl`, `*VS.hlsl`, etc.) |
| `EngineCore/Engine/Vertex.h` | Reused existing `Vertices::Terrain` struct (compatible layout: float3+float2+float2) |

---

## Component Design

### 1. TerrainClipmap — LOD Management

**Concept:** Geometry clipmap creates concentric square rings of fixed-size terrain tiles around the camera. Each ring (level) covers 2x the area of the inner ring with the same vertex count, effectively halving resolution.

**Parameters:**
- `CLIPMAP_LEVELS = 10` — number of LOD levels
- `CLIPMAP_TILE_SIZE = 64` — vertices per tile side
- `CLIPMAP_TILES_PER_SIDE = 5` — tiles per ring side (5x5 = 25 tiles per level)
- `CLIPMAP_CELL_SIZE_BASE = 1.0f` — meters per cell at finest level

**Chunk Lifecycle:**
```
Camera moves -> TerrainClipmap::Update()
  -> Compute new camera grid position per LOD level
  -> If origin changed, shift tiles and mark dirty
  -> Dirty tiles queued for GPU generation
  -> TerrainNoiseGenerator::GenerateTiles() dispatches compute
  -> Mark tiles clean after generation
  -> ProceduralTerrain::Draw() renders active tiles
```

**Per-tile GPU resources:**

| Resource | Format | Size | Purpose |
|----------|--------|------|---------|
| HeightMap | R16_FLOAT | 64x64 | Displacement in DS |
| NormalMap | R16G16_FLOAT | 64x64 | Packed XZ normal components |
| MaterialWeightMap | R8G8B8A8_UNORM | 64x64 | 4 stored weights, 5th = 1-sum |

Each tile has 6 descriptors (3 SRV + 3 UAV) allocated from a private descriptor heap, plus global SRV indices for shader access.

**Clipmap Update Strategy:**
- Track camera position in grid space per level
- 5x5 grid centered on camera, shifted when camera crosses tile boundary
- All GPU resources pre-allocated at startup (no runtime allocation)
- State transitions via raw `ResourceBarrier` (tile textures are raw `ComPtr<ID3D12Resource>`, not `GPUResource`)

**Crack Fixup:** Not yet implemented. Planned for Phase 4 using trim regions at LOD boundaries.

### 2. TerrainNoiseGenerator — GPU Procedural Generation

**Compute Shader: `TerrainNoiseCS.hlsl`**  
Thread group: `[numthreads(64, 64, 1)]` — one group per tile (64x64 threads = 4096 texels).

**Input CBuffer (`ProceduralTerrainNoiseCB`, register b0):**
```cpp
struct ProceduralTerrainNoiseCB
{
    XMFLOAT3    CameraPosition;     // +12
    float       CellSize;           // +16  (computed from LOD level: baseCellSize * 2^LOD)
    XMINT2      TileOffset;         // +24  (gridCoord * TileSize)
    int         TileSize;           // +28  (64)
    float       HeightScale;        // +32
    float       NoiseScale;         // +36
    UINT        Seed;               // +40
    UINT        Octaves;            // +44
    float       Lacunarity;         // +48
    float       Persistence;        // +52
    float       WarpStrength;       // +56
    float       WarpScale;          // +60
    float       SnowHeight;         // +64
    float       SnowTransition;     // +68
    float       StoneSlope;         // +72
    float       StoneTransition;    // +76
    int         LODLevel;           // +80
    float       Pad[3];             // +84-92  (alignment to 16-byte)
};
```

**Output UAVs:**
- `RWTexture2D<float> HeightMap` (u0)
- `RWTexture2D<float2> NormalMap` (u1)
- `RWTexture2D<float4> MaterialWeights` (u2)

**Noise Pipeline (per thread/texel):**
1. Compute world position: `(TileOffset + DTid.xy) * CellSize`
2. Domain warp: `pos += WarpStrength * FBM(WarpScale * pos)`
3. Base FBM height using `GradientNoise` (6-8 octaves with rotation between octaves)
4. Ridge noise: `ridge = (1.0 - abs(GradientNoise(...)))^2` for mountains
5. Blend base and ridge via another noise layer (`ridgeMask`)
6. Compute normal via finite differences (4 neighbor samples, cross product of tangents)
7. Compute material weights from height + slope + noise (see Material Rules below)
8. Write to UAVs

**Noise functions reused from `CommonResource.hlsli`:** `GradientNoise`, `GradientNoised` (with derivatives), `FBM`, `random12/22/32`.

### 3. Tessellation Pipeline

#### Vertex Shader: `TerrainClipmapVS.hlsl`
- Pass-through: forwards `Position`, `Tex` (as TileUV), `BoundsY`
- Reads `gClipmapLevel` and `gTerrainTileOffsetX/Y` from render CB → passes to HS/DS
- Input layout matches `Vertices::Terrain`: `{float3 Position, float2 Tex, float2 BoundsY}`

#### Hull Shader: `TerrainClipmapHS.hlsl`
- **World position computation:** From tile UV + `gTerrainTileOffsetX/Y` + `gTerrainCellSize`
  ```
  worldXZ = tileUV * TILE_SIZE * cellSize + tileOffset * cellSize
  ```
- **Frustum culling:** Per-patch AABB test using `BoundsY` (min/max height) and world XZ bounds
- **Tessellation factor:** Distance-based, LOD-scaled:
  ```hlsl
  tess = lerp(maxTess, minTess, saturate((dist - minDist) / (maxDist - minDist)))
  tess /= (1 << clipmapLevel)
  ```
- Partitioning: `fractional_even`, max tess factor 64

#### Domain Shader: `TerrainClipmapDS.hlsl`
- Bilinear interpolation of tile UV from quad patch
- World position from interpolated UV + tile offset (same formula as HS)
- **Displacement:** Sample `HeightMap` SRV at interpolated tile UV
- **Normal reconstruction:** Unpack `NormalMap` XZ, compute Y from normalization
- **Tangent frame:** Cross product of normal with up-vector
- **Material weights:** Sample `MaterialWeightMap` at tile UV
- Outputs: clip position, world position, normal, tangent, tile UV, world XZ, material weights

#### Pixel Shader: `TerrainClipmapPS.hlsl`
- **Stochastic sampling** (per material layer, per texture type):
  1. Compute rotation from world position hash (`random12`)
  2. Sample texture twice: original UV + rotated UV
  3. Blend using noise mask with `gStochasticSharpness` control via smoothstep
- **Material blending:** Weighted sum across 5 layers (skip if weight < 0.01):
  - Albedo, Normal, Roughness, Metallic, AO — each with stochastic sampling
- **PBR Lighting:** Inline Cook-Torrance BRDF (GGX D, Smith G, Schlick F)
  - Directional light loop using `gLights[]` array
  - Lambertian diffuse + specular
- **Fog:** Distance-based linear fog blended toward ambient color
- **Performance:** ~50 texture samples worst case (5 layers × 5 maps × 2 stochastic); early-out for low-weight layers reduces this significantly

### 4. ProceduralTerrain — Main Class

```cpp
class ProceduralTerrain : public RenderObject
{
    // Subsystems
    unique_ptr<TerrainClipmap>          mClipmap;
    unique_ptr<TerrainNoiseGenerator>   mNoiseGen;

    // GPU pipeline
    RootSignature   mTerrainRootSig;
    GraphicsPSO     mTerrainPSO;
    GraphicsPSO     mTerrainShadowPSO;     // TODO: Phase 4

    // Constant buffer (ring buffer, 64 slots)
    ProceduralTerrainConstants mConstants;
    ComPtr<ID3D12Resource>     mConstantBuffer;  // upload heap, 64x aligned CB size
    D3D12_GPU_VIRTUAL_ADDRESS  mCBGPU;

    // Material layer SRVs: [layer][albedo, normal, roughness, metallic, AO]
    int mLayerSRVIndices[5][5];

    // Shared geometry (same VB/IB for all tiles)
    ComPtr<ID3D12Resource> mVertexBuffer;
    ComPtr<ID3D12Resource> mIndexBuffer;
    UINT mIndexCount;
};
```

**Key design decisions:**
- **Ring buffer CB (64 slots):** Prevents GPU read-write race when uploading per-tile constants. Each tile draw uses a unique CB slot.
- **Raw ResourceBarrier:** Tile textures are raw `ComPtr<ID3D12Resource>`, not wrapped in `GPUResource`. State transitions use `GetCommandList()->ResourceBarrier()` directly.
- **Engine PSO pattern:** Uses `PSO.SetRootSignature()` + `PSO.Finalize()` instead of raw `CreateGraphicsPipelineState`.

### 5. Constant Buffer Structures

#### Render CB (`ProceduralTerrainConstants`, register b1):
```cpp
__declspec(align(256)) struct ProceduralTerrainConstants
{
    XMFLOAT4X4  ViewProj;                // 64 bytes
    XMFLOAT3    EyePosW;                 // 12
    float       MinTessDistance;         // 4   →  register 4
    float       MaxTessDistance;         // 4
    float       MinTessFactor;           // 4
    float       MaxTessFactor;           // 4   →  register 5
    int         ClipmapLevel;            // 4
    float       CellSize;                // 4
    float       HeightScale;             // 4
    float       TexScale;                // 4   →  register 6
    float       StochasticSharpness;     // 4   →  register 7.x
    // 3 padding floats to next register boundary → register 7.yzw unused
    XMFLOAT4    gWorldFrustumPlanes[6];  // 96 bytes → registers 8-13
    int         HeightMapIndex;          // 4
    int         NormalMapIndex;          // 4
    int         MaterialWeightMapIndex;  // 4
    int         TileGridOffsetX;         // 4   →  register 14 (separate ints for HLSL alignment)
    int         TileGridOffsetY;         // 4
    int         LayerAlbedoIndices[5];   // 20
    int         LayerNormalIndices[5];   // 20
    int         LayerRoughnessIndices[5];// 20
    int         LayerMetallicIndices[5]; // 20
    int         LayerAOIndices[5];       // 20
};
```

> **Note:** `TileGridOffset` is split into separate X/Y ints to avoid HLSL packing issues (`int2` at non-16-byte-aligned offsets gets pushed to the next register boundary by HLSL, causing C++/HLSL layout mismatch).

#### Init Info:
```cpp
struct ProceduralTerrainInitInfo
{
    UINT    ClipmapLevels    = 10;
    UINT    TileSize         = 64;
    float   CellSizeBase     = 1.0f;
    float   HeightScale      = 200.0f;
    float   NoiseScale       = 0.005f;
    UINT    Seed             = 42;
    string  LayerMapNames[5] = { "grass","darkdirt","stone","lightdirt","snow" };
    string  LayerAssetPath;
};
```

---

## Data Flow

```
Frame Update:
  1. ProceduralTerrain::Update(dt, GraphicsContext*)
  2.   mClipmap->Update(cameraPos)
       - Compute camera grid position per LOD level
       - Shift tiles if origin changed, mark dirty
       - Collect active + dirty tile lists
  3.   mNoiseGen->GenerateTiles(computeCtx, dirtyTiles, cameraPos, baseCellSize)
       - For each dirty tile:
         a. Compute cellSize = baseCellSize * 2^LOD
         b. Fill noise CB, map/unmap upload
         c. ResourceBarrier: PixelSR → UAV (3 barriers)
         d. Set UAVs, Dispatch2D(64, 64, 64, 64) = 1 thread group
       - FlushResourceBarriers
  4.   Mark dirty tiles clean
  5.   UpdateConstantBuffer() — frustum planes + material indices

Frame Draw:
  1. ProceduralTerrain::Draw(GraphicsContext&)
  2.   Set root signature + PSO
  3.   Set shared VB/IB, set primitive topology (4 control point patch)
  4.   For each active tile:
       a. Update per-tile CB fields (SRV indices, tile offset, LOD)
       b. Write to ring buffer CB slot N
       c. SetConstantBuffer(1, cbGPU + N * alignedSize)
       d. ResourceBarrier: UAV → PixelSR (3 barriers)
       e. DrawIndexed(mIndexCount)
  5.   PS: stochastic sample → material blend → PBR lighting → fog → output
```

---

## Stochastic Sampling Detail

The anti-tiling technique blends 2 samples of the same texture with different rotations:

```hlsl
// Shared stochastic rotation per pixel
float2x2 StochasticRotation(float2 worldPos)
{
    float angle = random12(worldPos * 0.137) * PI2;
    float s, c; sincos(angle, s, c);
    return float2x2(c, -s, s, c);
}

// Sample with rotation
float3 SampleStochastic(int srvIndex, float2 baseUV, float2 worldPos)
{
    float2x2 rot = StochasticRotation(worldPos);
    float2 uv1 = baseUV;
    float2 uv2 = mul(rot, baseUV - 0.5) + 0.5;
    float3 s1 = gSRVMap[srvIndex].Sample(gSamplerAnisoWrap, uv1).rgb;
    float3 s2 = gSRVMap[srvIndex].Sample(gSamplerAnisoWrap, uv2).rgb;
    float blend = random12(baseUV * 7.41 + worldPos * 0.03);
    blend = smoothstep(0.5 - sharpness*0.5, 0.5 + sharpness*0.5, blend);
    return lerp(s1, s2, blend);
}
```

**Performance cost:** 2x texture samples per map per active layer.  
**Optimizations applied:**
- Skip layers with weight < 0.01
- Share rotation across all map types within same layer
- `gStochasticSharpness` (default 0.95) controls blend softness

---

## Procedural Material Rules Detail

Executed in `TerrainNoiseCS.hlsl` per texel. Produces 4 stored weights; 5th derived as `1 - sum`.

**Layer order:** 0=lightdirt, 1=darkdirt, 2=grass, 3=stone, 4=snow

```hlsl
// Priority-based weight computation
float noise = GradientNoise(worldPos.xz * 0.05);

float snow  = smoothstep(SnowHeight - SnowTr, SnowHeight + SnowTr, height);
float stone = smoothstep(StoneSlope - StoneTr, StoneSlope + StoneTr, slope);
float grass = smoothstep(0.2, 0.6, noise + 0.5 - slope * 2.0);
float dirt  = smoothstep(-0.1, 0.3, noise - 0.2) * (1.0 - grass);

w[4] = snow;
w[3] = (1-snow) * stone;
w[2] = (1-snow)(1-stone) * grass;
w[1] = (1-snow)(1-stone)(1-grass) * dirt;
w[0] = max(0, 1 - w[1] - w[2] - w[3] - w[4]);

// Normalize to sum=1.0
```

**Where slope = 1.0 - normal.y** (0=flat, 1=vertical)

---

## Root Signatures

### TerrainNoiseGenerator (Compute)
```
[0] CBV(b0)     — TerrainNoiseCB
[1] UAV range   — 3 UAVs (height, normal, weights)
```

### ProceduralTerrain (Graphics)
```
[0] CBV(b0)     — Main pass constants (view/proj/lights)
[1] CBV(b1)     — ProceduralTerrainConstants (per-tile)
[2] SRV(t1,s1)  — Material data structured buffer
[3] SRV range   — gSRVMap[62..1085] (texture array)
```

---

## Integration Points

### Existing Code Reused
| Component | File | Usage |
|-----------|------|-------|
| `RenderObject` base | `GraphicEffects/Misc/RenderObject.h` | `ProceduralTerrain` inherits with `ObjectType::Terrain` |
| `Vertices::Terrain` | `Engine/Vertex.h` | Shared vertex format: `{float3, float2, float2}` |
| Noise functions | `Shaders/Misc/CommonResource.hlsli` | `GradientNoise`, `FBM`, `random12/22/32` |
| `TextureManager` | `Graphics/TextureManager.h` | `CreatePBRTextures()` for 5 material layers |
| `MaterialManager` | `Misc/ConstantBuffer.h` | `BuildMaterials()` for PBR material registration |
| Global SRV array | `gSRVMap[]` + `AddToGlobalTextureSRVDescriptor()` | All texture access |
| `GraphicsPSO` / `RootSignature` | `Graphics/PipelineState.h` | `Finalize()` pattern for PSO creation |
| `ComputeContext` | `Graphics/CommandContext.h` | `Dispatch2D()`, `SetDynamicDescriptor()`, `SetPipelineState()` |
| `GraphicsContext` | `Graphics/CommandContext.h` | `SetVertexBuffer()`, `SetIndexBuffer()`, `DrawIndexed()` |
| `EngineUtility` | `Engine/EngineUtility.h` | `LoadShaderFile()`, `CreateDefaultBuffer()` |
| AABB frustum culling | `CommonResource.hlsli` | `AABBOutsideFrustumTest()` |

### Scene Integration
- `ProceduralTerrain` inherits `RenderObject` with `ObjectType::Terrain`
- Add to scene via `Scene::AddObjectToScene()` pattern
- Render loop calls: `Update()` → compute generation → `Draw()` via scene's render object iteration

---

## Implementation Status

### Phase 1: Foundation — DONE
- [x] `ProceduralTerrain` class inheriting `RenderObject`
- [x] `TerrainClipmap` with 5x5 grid per level, dirty tracking
- [x] `TerrainNoiseGenerator` with compute pipeline
- [x] FBM noise + domain warping + ridge noise in `TerrainNoiseCS.hlsl`
- [x] Per-tile GPU resources (heightmap, normal, material weights)

### Phase 2: Rendering Pipeline — DONE
- [x] Shared VB/IB geometry for all tiles
- [x] `TerrainClipmapVS/HS/DS.hlsl` (tessellation + heightmap displacement)
- [x] `TerrainClipmapPS.hlsl` (full PBR + fog)
- [x] Camera-following clipmap updates

### Phase 3: Materials + Anti-Tiling — DONE
- [x] Material weight generation in `TerrainNoiseCS.hlsl`
- [x] Stochastic sampling in pixel shader (rotation-based)
- [x] Procedural material blending (5 layers with early-out)
- [x] 5 PBR material textures loaded and wired into shader

### Phase 4: Polish — PENDING
- [ ] Shadow rendering support (`DrawShadow` + `mTerrainShadowPSO`)
- [ ] Crack fixup between clipmap LOD levels (trim regions)
- [ ] CPU-side `GetHeight()` for game logic (collision, camera follow)
- [ ] ImGui debug UI wiring (noise params already declared)
- [ ] Performance optimization (async compute for generation, fence sync)
- [ ] LOD level visualization debug overlay

---

## Verification Plan

| Phase | Test | Expected Result |
|-------|------|-----------------|
| 1 | Dispatch compute for single tile, read back heightmap | Valid noise data in [0, HeightScale], not all zeros |
| 2 | Move camera, observe clipmap tile updates | Tiles follow camera, no holes or gaps |
| 2 | Toggle wireframe | Tessellation decreases with distance, increases near camera |
| 3 | Side-by-side with/without stochastic sampling | Tiling artifacts visibly reduced |
| 3 | Check material distribution | Snow on peaks, stone on cliffs, grass on flats, smooth transitions |
| 4 | Shadow mapping enabled | Terrain casts correct shadows, no artifacts at LOD boundaries |
| 4 | High-speed camera movement (>100 m/s) | No frame drops below 60fps, tiles generate without stalling |
| 4 | GPU profiler | Noise generation < 2ms, total terrain render < 4ms at 2km view distance |
