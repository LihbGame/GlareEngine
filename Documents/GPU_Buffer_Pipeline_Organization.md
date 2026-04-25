# GPU Buffer Pipeline Organization

This document describes how GPU buffers, descriptor heaps, root signatures, and shader resources are organized in GlareEngine's rendering pipeline.

---

## 1. EngineConfig Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `COMMONSRVSIZE` | 20 | Common SRV slots (GBuffer, lighting data) |
| `COMMONUAVSIZE` | 10 | Common UAV slots (scene color, masks) |
| `MAXPBRSRVSIZE` | 10 | PBR material SRV slots |
| `MAXCUBESRVSIZE` | 32 | Cube texture SRV slots |
| `MAX2DSRVSIZE` | 2048 | 2D texture SRV slots |
| `NUMFRAME` | 3 | Triple buffering frame count |

Defined in `EngineConfig.h` (C++) and `CommonResource.hlsli` (HLSL) — must stay in sync.

---

## 2. Global Root Signature

The engine uses a single global root signature with 13 parameters and 8 static samplers.

**File:** `EngineCore/Graphics/Render.cpp` `BuildRootSignature()`

### 2.1 Root Parameters

| Slot | Enum | Type | Register | Space | Count | Description |
|------|------|------|----------|-------|-------|-------------|
| 0 | `eMainConstantBuffer` | CBV (b0) | b0 | 0 | 1 | Main pass constants |
| 1 | `eMaterialConstants` | CBV (b2) | b2 | 0 | 1 | Per-material constants |
| 2 | `eCommonConstantBuffer` | CBV (b1) | b1 | 0 | 1 | Shadow/terrain/object CB |
| 3 | `eCommonSRVs` | SRV table | t0 | 0 | 20 | GBuffer, SSAO, lighting |
| 4 | `eCommonUAVs` | UAV table | u0 | 0 | 10 | Scene color UAV, masks |
| 5 | `eMaterialSRVs` | SRV table | t20 | 0 | 10 | Reserved PBR material SRVs |
| 6 | `eCubeTextures` | SRV table | t30 | 0 | 32 | Cube map textures |
| 7 | `ePBRTextures` | SRV table | t62 | 0 | 2048 | 2D PBR textures |
| 8 | `eMaterialSamplers` | Sampler table | s0 | 0 | 10 | Dynamic material samplers |
| 9 | `eMaterialConstantData` | Buffer SRV | t1 | 1 | 1 | Material structured buffer |
| 10 | `eInstanceConstantData` | Buffer SRV | t0 | 1 | 10 | Instance data buffer |
| 11 | `eSkinMatrices` | Buffer SRV | t2 | 1 | 1 | Skin animation matrices |

### 2.2 Static Samplers

| Register | Name | Filter | Address Mode |
|----------|------|--------|-------------|
| s10 | `gSamplerLinearWrap` | Linear | Wrap |
| s11 | `gSamplerAnisoWrap` | Anisotropic (16x) | Wrap |
| s12 | `gSamplerShadow` | Comparison Linear | Border |
| s13 | `gSamplerLinearClamp` | Linear | Clamp |
| s14 | `gSamplerVolumeWrap` | Point | Wrap |
| s15 | `gSamplerPointClamp` | Point | Clamp |
| s16 | `gSamplerPointBorder` | Point | Border |
| s17 | `gSamplerLinearBorder` | Linear | Border |

---

## 3. Descriptor Heap Layout

The main descriptor heap `gTextureHeap` (8192 slots, `CBV_SRV_UAV` type) is divided into fixed regions:

```
Offset  0        20       30         62                    2110        8192
        |--------|--------|----------|----------------------|-----------|
        CommonSRVs CommonUAVs CubeSRVs   2D Texture SRVs      (free)
        (20)       (10)       (32)       (2048)
```

### 3.1 Region Details

| Region | Heap Offset | Size | Root Param | Shader Register | Content |
|--------|------------|------|------------|----------------|---------|
| CommonSRVs | 0 | 20 | `eCommonSRVs` | t0..t19 | GBuffer SRVs, SSAO, lighting grids (populated per-frame) |
| CommonUAVs | 20 | 10 | `eCommonUAVs` | u0..u9 | Scene color UAV, FSR masks |
| CubeSRVs | 30 | 32 | `eCubeTextures` | t30..t61 | Sky cube, IBL cube, env maps |
| 2D Texture SRVs | 62 | 2048 | `ePBRTextures` | t62..t2109 | All PBR material textures |

### 3.2 CommonSRVs Per-Frame Population

`CommonSRVHeapOffset` starts at `GBUFFER_TEXTURE_REGISTER_COUNT` each frame and increments:

| Offset | Count | Content |
|--------|-------|---------|
| 0..5 | 6 | GBuffer[Emissive, Normal, MSR, BaseColor, WorldTangent] + Depth |
| 6 | 1 | SSAO result |
| 7..12 | 6 | LightGrid, LightBuffer, LightShadowArray, ClusterLightGrid, GlobalLightIndexList, AreaLightData |

### 3.3 Descriptor Copy Flow

```
TextureManager loads texture → Texture::GetSRV() creates CPU descriptor
  → AddToGlobalTextureSRVDescriptor() pushes to g_TextureSRV vector
  → CopyTextureHeap() copies entire vector to gTextureHeap[62+]
  → Shader accesses via gSRVMap[index] at register t62+
```

---

## 4. Constant Buffers

### 4.1 MainConstants (b0)

**File:** `GraphicEffects/Misc/ConstantBuffer.h`, `Shaders/Misc/CommonResource.hlsli`

Per-frame data uploaded every frame via `SetDynamicConstantBufferView`:

| Field | Type | Description |
|-------|------|-------------|
| `gView/InvView/Proj/InvProj/ViewProj/InvViewProj` | float4x4 | Camera matrices |
| `gShadowTransform` | float4x4 | Shadow light space matrix |
| `gPreViewProjMatrix` | float4x4 | Previous frame VP (for TAA/motion blur) |
| `gEyePosW` | float3 | Camera world position |
| `gRenderTargetSize / gInvRenderTargetSize` | float2 | Render target dimensions |
| `gNearZ / gFarZ` | float | Clip planes |
| `gTotalTime / gDeltaTime` | float | Time |
| `gAmbientLight` | float4 | Ambient color |
| `gLights[16]` | DirectionalLight[16] | Directional lights |
| `gInvTileDimension / gTileCount` | float4 / uint4 | Tiled lighting params |
| `gShadowMapIndex / gSkyCubeIndex` | int | SRV indices into gSRVMap[] |
| `gBakingDiffuseCubeIndex / gBakingPreFilteredEnvIndex / gBakingIntegrationBRDFIndex` | int | IBL texture indices |
| `gDirectionalLightsCount` | int | Active light count |
| `gAreaLightLTC1/2SRVIndex` | int | Area light LTC texture indices |
| `gMipLodBias` | float | Mipmap LOD bias |
| `gCurJitterOffset / gPreJitterOffset` | float2 | Temporal AA jitter |

### 4.2 CommonConstantBuffer (b1)

Used for pass-specific data:
- **Shadow pass**: `ShadowConstantBuffer` (shadow ViewProj)
- **Terrain pass**: `TerrainConstants` / `ProceduralTerrainConstants`
- **Object pass**: Per-object world matrix

### 4.3 MaterialConstants (b2)

Per-material constants for instance models. Each material has a `MatCBIndex` used to index into the material constant buffer.

### 4.4 Structured Buffers (space1)

| Register | Struct | Description |
|----------|--------|-------------|
| t0, space1 | `InstanceData` | World/TexTransform + MaterialIndex per instance |
| t1, space1 | `MaterialData` | Albedo, FresnelR0, HeightScale, MatTransform, texture indices |
| t2, space1 | Skin matrices | Bone transform matrices for animated models |

Uploaded via `SetDynamicSRV` each frame from `MaterialManager::GetMaterialsConstantBuffer()`.

---

## 5. Terrain Root Signature

Terrain uses a separate root signature for its tessellation pipeline.

**File:** `GraphicEffects/Terrain/ProceduralTerrain.cpp`

| Slot | Type | Register | Description |
|------|------|----------|-------------|
| 0 | CBV | b0 | MainPass CB (cached via CacheMainCBData) |
| 1 | CBV | b1 | ProceduralTerrainConstants (per-tile, ring buffer) |
| 2 | Buffer SRV | t1, space1 | Material data structured buffer |
| 3 | SRV table | t62, count=MAX2DSRVSIZE | 2D texture array (shares gTextureHeap) |

Static samplers: s10..s17 (same as global root signature).

### 5.1 ProceduralTerrainConstants (b1)

Per-tile constant buffer uploaded via ring buffer (256 slots):

| Field | Type | Description |
|-------|------|-------------|
| `ViewProj` | float4x4 | Camera view-projection |
| `EyePosW` | float3 | Camera position |
| `MinTessDistance / MaxTessDistance` | float | Tessellation LOD range |
| `MinTessFactor / MaxTessFactor` | float | Tessellation factor range |
| `ClipmapLevel` | int | Current LOD level |
| `CellSize` | float | World-space cell size |
| `HeightScale` | float | Height multiplier |
| `TexScale` | float | Texture tiling scale |
| `gWorldFrustumPlanes[6]` | float4 | Frustum culling planes |
| `HeightMapIndex / NormalMapIndex / MaterialWeightMapIndex` | int | Per-tile SRV indices |
| `TileGridOffsetX / TileGridOffsetY` | int | Tile position in clipmap grid |
| `LayerAlbedoIndices[5]` | int[5] | Material layer texture indices |
| `LayerNormalIndices[5]` | int[5] | Normal map texture indices |
| `LayerRoughnessIndices[5]` | int[5] | Roughness map texture indices |
| `LayerMetallicIndices[5]` | int[5] | Metallic map texture indices |
| `LayerAOIndices[5]` | int[5] | AO map texture indices |

### 5.2 ProceduralTerrainNoiseCB

Compute shader constant buffer for tile generation:

| Field | Type | Description |
|-------|------|-------------|
| `CameraPosition` | float3 | Camera position |
| `CellSize` | float | Grid cell size |
| `TileOffset` | int2 | Tile grid position |
| `TileSize` | int | Vertices per tile edge |
| `HeightScale / NoiseScale` | float | Height and noise parameters |
| `Seed / Octaves` | uint | Noise generation params |
| `Lacunarity / Persistence` | float | FBM octave params |
| `WarpStrength / WarpScale` | float | Domain warping params |
| `SnowHeight / SnowTransition` | float | Snow biome params |
| `StoneSlope / StoneTransition` | float | Stone biome params |
| `LODLevel` | int | Clipmap LOD level |

---

## 6. G-Buffer Layout

**File:** `EngineCore/Graphics/Render.h`

| Slot | Name | Format | Channels |
|------|------|--------|----------|
| 0 | `GBUFFER_Emissive` | R11G11B10_FLOAT | Emissive RGB |
| 1 | `GBUFFER_Normal` | R16G16B16A16_UNORM | World Normal RGB |
| 2 | `GBUFFER_MSR` | R8G8B8A8_UNORM | Metallic R, Specular G, Roughness B, ShadingModel A |
| 3 | `GBUFFER_BaseColor` | R16G16B16A16_UNORM | Base Color RGB, Material AO A |
| 4 | `GBUFFER_WorldTangent` | R8G8B8A8_UNORM | World Tangent RGB, Aniso Intensity A |
| 5 | `GBUFFER_MotionVector` | R16G16_FLOAT | Motion Vector RG |

**Depth Buffer:** `g_SceneDepthBuffer` — `DXGI_FORMAT_R32_TYPELESS` (DSV: D32_FLOAT)

---

## 7. Texture Registration Pipeline

Textures go through these stages from loading to shader access:

### 7.1 Loading Phase
```
1. TextureManager::CreatePBRTextures(path)
   → For each PBR type (basecolor, normal, metallic, roughness, ao, height):
     - TextureManager::GetModelTexture(name, srgb)
       → CreateDDSTextureFromFile12(device, cmdList, file, resource, uploadHeap)
       → Store in mTextures[name]

2. MaterialManager::BuildMaterials(name, textures)
   → For each texture:
     - AllocateDescriptor(CBV_SRV_UAV) — allocates from staging heap
     - CreateShaderResourceView(resource, &srvDesc, descriptor)
     - AddToGlobalTextureSRVDescriptor(descriptor) → returns index N
     - Material->mDescriptorsIndex.push_back(N)

3. AddToGlobalCubeSRVDescriptor() for cube maps
```

### 7.2 Copy Phase
```
4. Render::CopyTextureHeap()
   → Copy cube descriptors to gTextureHeap[30..61]
   → Copy 2D descriptors to gTextureHeap[62..62+N]
```

### 7.3 Render Phase
```
5. Scene render sets descriptor table:
   Context.SetDescriptorTable(ePBRTextures, gTextureHeap[62])

6. Shader accesses texture by index:
   gSRVMap[material.mDiffuseMapIndex]  // index N → register t(62+N)
```

### 7.4 glTF Model Texture Binding

glTF models pack 6 textures per material into contiguous SRV slots:
```cpp
// glTFModelLoader.cpp
SRVDescriptorTable = 62 + g_TextureSRV.size() - 6;
mesh.srvTable = SRVDescriptorTable;  // stored per-mesh
// In shader: texture array starts at mesh.srvTable offset
```

### 7.5 Instance Model Texture Binding

Instance models use `MaterialConstant.mDiffuseMapIndex` etc. to index into `gSRVMap[]`:
```cpp
// MaterialManager stores indices from AddToGlobalTextureSRVDescriptor
// Shader reads: gSRVMap[gMaterialData[matIndex].mDiffuseMapIndex]
```

---

## 8. Dynamic Descriptor System

**File:** `EngineCore/Graphics/DynamicDescriptorHeap.h`

| Property | Value |
|----------|-------|
| `MaxNumDescriptors` | 4096 |
| `MaxNumDescriptorTables` | 16 |
| Descriptors per heap | 1024 |

Dynamic descriptor heaps cache descriptor table updates and batch-copy them to GPU-visible heaps at draw time. This avoids per-draw descriptor heap switches.

---

## 9. Rendering Pipeline Resource Flow

### 9.1 Forward Rendering (FR)
```
SetRootSignature(gRootSignature)
SetDescriptorHeap(gTextureHeap)
  ├── b0: MainConstants (SetDynamicConstantBufferView)
  ├── t30: CubeTextures (SetDescriptorTable, heap[30])
  ├── t62: PBRTextures (SetDescriptorTable, heap[62])
  ├── t1/space1: MaterialData (SetDynamicSRV)
  ├── [Terrain Pass] (switches to terrain root sig, then restores)
  │     ├── b0: MainConstants (cached)
  │     ├── b1: ProceduralTerrainConstants (ring buffer)
  │     └── t62: Texture SRV table
  └── [Main Pass] (restores gRootSignature)
        ├── Shadow pass
        └── Draw objects
```

### 9.2 Tiled/Clustered Forward (TBFR/CBFR)
```
SetRootSignature(gRootSignature)
  ├── Depth PrePass → LinearizeZ → SSAO
  ├── Light culling (Tiled: FillLightGrid / Clustered: BuildCluster+Mask+Cull)
  ├── Copy lighting SRVs to CommonSRVs region
  ├── SetDescriptorHeap + SetDescriptorTable(eCommonSRVs, heap[0])
  ├── Shadow maps
  ├── [Terrain Pass]
  └── MeshRenderPass (opaque + transparent)
```

### 9.3 Deferred (TBDR/CBDR)
```
SetRootSignature(gRootSignature)
  ├── Clear GBuffer
  ├── Depth PrePass + Sky
  ├── LinearizeZ → SSAO
  ├── Light culling
  ├── [Terrain Pass]
  ├── MeshRenderPass (GBuffer fill)
  └── DeferredLighting (compute shader)
        ├── Read GBuffer SRVs (heap[0..5])
        ├── Read Cube/PBR textures
        └── Write SceneColor UAV (heap[20])
```

---

## 10. Summary Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Root Signature (13 params)                      │
├──────────┬──────────┬──────────┬───────────┬───────────┬───────────┤
│  b0      │  b1      │  b2      │  t0-19    │  u0-9     │  t20-29   │
│  MainCB  │  CommonCB│  MatCB   │  Common   │  Common   │  MatSRV   │
│          │          │          │  SRVs     │  UAVs     │  (reserve)│
├──────────┴──────────┴──────────┴───────────┴───────────┴───────────┤
│  t30-61          │  t62-2109         │  s0-9      │  space1       │
│  CubeTextures    │  gSRVMap[2048]    │  DynSampler│  t0:Instance  │
│  (32 slots)      │  (2D textures)    │            │  t1:Material  │
│                  │                   │            │  t2:Skin      │
├──────────────────┴───────────────────┴────────────┴───────────────┤
│                    Static Samplers: s10-s17                         │
└────────────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────┴──────────┐
                    │   gTextureHeap     │
                    │   (8192 slots)     │
                    │                    │
                    │  [0..19]  Common   │
                    │  [20..29] UAVs     │
                    │  [30..61] Cubes    │
                    │  [62..2109] 2D Tex │
                    └────────────────────┘
```
