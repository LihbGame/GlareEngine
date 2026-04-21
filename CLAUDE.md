# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GlareEngine is a DirectX 12 rendering engine written in C++20, targeting Windows x64 with Visual Studio 2022. It supports multiple rendering paths (tiled forward, tiled deferred, clustered deferred) with PBR, DXR raytracing, DLSS, and FSR.

## Build System

- **CMake** (minimum 3.15) with Visual Studio 2022 generator
- Build from the `build/` directory which contains the VS solution
- CMake configure: `cmake -B build -G "Visual Studio 17 2022" -A x64`
- Build: `cmake --build build --config Release` (or `Debug`)
- Output goes to `bin/Release/` or `bin/Debug/`
- **Startup project**: `GameDemo` (set via `VS_STARTUP_PROJECT`)
- Shaders compile as part of the VS build using dxc (Shader Model 6.6, `-HV 2021` flag)

### Build Targets
- **GlareEngine** — static library (core engine)
- **GameDemo** — executable, links against GlareEngine
- **EngineDemo_Old** — legacy demo, links against GlareEngine

### Shader Compilation Convention
所有的shader都是在cmake里配置的dxc 编译的，把编译的.h文件结果保存到GlareEngine/Shaders/CompiledShaders/，然后再需要的CPP文件中include进来
Shaders are HLSL files under `GlareEngine/Shaders/`. The shader type is determined by filename suffix:
- `*VS.hlsl` → Vertex, `*PS.hlsl` → Pixel, `*CS.hlsl` → Compute
- `*GS.hlsl` → Geometry, `*HS.hlsl` → Hull, `*DS.hlsl` → Domain
- Compiled headers output to `GlareEngine/Shaders/CompiledShaders/`

### Shader Model Troubleshooting
If the scene renders dark, change the shader model from 6.6 to 5.1 in `GlareEngine/CMakeLists.txt` (`VS_SHADER_MODEL` property). This typically indicates a GPU driver compatibility issue.

## Architecture

```
GlareEngine/                          # Engine library root
├── EngineCore/                       # Core systems
│   ├── Engine/                       # App framework, camera, input, logging, threading, profiling
│   ├── Graphics/                     # DX12 abstraction: command lists, descriptor heaps, buffers,
│   │                                 #   root signatures, pipeline state, texture management,
│   │                                 #   DLSS/FSR integration, GPU timing
│   ├── Math/                         # Vectors, matrices, quaternions, frustum, bounding volumes
│   └── Model/                        # glTF loading, mesh building, model rendering
├── EngineEditor/ImgUIEditor/         # ImGui-based editor UI
├── GraphicEffects/                   # Rendering features as separate modules:
│   ├── DXR/ GI/                      # Raytracing and global illumination
│   ├── Shadow/                       # PCSS/PCF shadow mapping
│   ├── PostProcessing/               # SSAO, bloom, TAA, FXAA, motion blur, color grading
│   ├── Sky/ Terrain/ Particles/      # Environment and effects
│   └── InstanceModel/                # Instanced PBR rendering
└── Shaders/                          # HLSL shaders, organized to mirror GraphicEffects/
    ├── Lighting/                     # Tiled/clustered forward and deferred lighting shaders
    ├── PBRInstanceModel/             # PBR instanced model shaders
    └── Present/                      # Final presentation/upsampling
GameDemo/                             # Demo application
EngineDemo_Old/                       # Legacy demo
Resource/                             # Shared assets (models, textures, UI)
packages/                             # NuGet-style native dependencies
```

### Key Abstractions (EngineCore/Graphics/)
- **GraphicsCore** — DX12 device initialization and factory
- **CommandContext / CommandListManager** — command list recording and pooling
- **DescriptorHeap / DynamicDescriptorHeap** — CPU/GPU descriptor management
- **RootSignature / PipelineState** — PSO and root signature construction
- **BufferManager / ColorBuffer / DepthBuffer** — GPU resource management
- **LinearAllocator / UploadBuffer** — temporary and upload heap allocations
- **TextureManager** — texture loading and conversion (WIC)
- **Display** — swap chain and presentation
- **DLSS / FSR** — super-resolution integration

### Rendering Flow
The engine supports multiple rendering paths selected at runtime. `GraphicEffects/` modules implement render passes that plug into the main render loop. Each effect module typically has a corresponding shader directory under `Shaders/`.

## Dependencies

- **DirectXTK12 / DirectXMesh / DirectXTex** — DX12 utilities (in `packages/` and `Dependences/`)
- **assimp** — 3D model import (static linked from `Dependences/lib/`)
- **dxc** — DirectX shader compiler (in `packages/dxc_2023_08_14/`)
- **DLSS SDK** — NVIDIA super-resolution (`Dependences/include/DLSS/`, `Dependences/lib/nvsdk_ngx_d.lib`)
- **AMD FidelityFX** — FSR (`Dependences/lib/amd_fidelityfx_dx12.lib`)
- **ozz** — Animation library
- **stb_image** — Image loading
- **ImGui** — Editor UI
- **WinPixEventRuntime** — GPU profiling
- **filewatch** — Runtime shader hot-reload
- **zlib** — Compression

## Key Definitions

- `_HAS_STD_BYTE=0` — defined globally; use `std::byte` alternatives
- `_DEBUG` / `DEBUG` — defined in Debug configuration
