# AGENTS.md

This file provides guidance to Codex when working in this repository.

## Project Overview

GlareEngine is a DirectX 12 rendering engine written in C++20 for Windows x64 with Visual Studio 2022. It supports tiled forward, tiled deferred, and clustered deferred rendering paths with PBR, DXR, DLSS, and FSR.

## Build System

- CMake minimum version: 3.15.
- Configure: `cmake -B build -G "Visual Studio 17 2022" -A x64`.
- Build: `cmake --build build --config Release` or `cmake --build build --config Debug`.
- Output directories: `bin/Release/` and `bin/Debug/`.
- Startup project: `GameDemo`.
- Shaders are compiled by CMake with dxc and emitted as headers under `GlareEngine/Shaders/CompiledShaders/`.

## Build Targets

- `GlareEngine`: core static library.
- `GameDemo`: main executable.
- `EngineDemo_Old`: legacy demo executable.

## Shader Conventions

- HLSL files live under `GlareEngine/Shaders/`.
- Shader type is determined by filename suffix:
  - `*VS.hlsl`: vertex shader.
  - `*PS.hlsl`: pixel shader.
  - `*CS.hlsl`: compute shader.
  - `*GS.hlsl`: geometry shader.
  - `*HS.hlsl`: hull shader.
  - `*DS.hlsl`: domain shader.
- Compiled shader headers are included by the C++ files that need them.
- If the scene renders dark, try shader model 5.1 instead of 6.6 in `GlareEngine/CMakeLists.txt`.

## Architecture

- `GlareEngine/EngineCore/`: app framework, graphics abstraction, math, and model systems.
- `GlareEngine/GraphicEffects/`: rendering modules such as Shadow, Terrain, GI, DXR, Sky, PostProcessing, and InstanceModel.
- `GlareEngine/Shaders/`: HLSL code organized to mirror rendering features.
- `GameDemo/`: demo application.
- `EngineDemo_Old/`: legacy demo.
- `Resource/`: shared assets.
- `packages/` and `Dependences/`: native dependencies and SDKs.

## Important Notes

- `_HAS_STD_BYTE=0` is defined globally; avoid `std::byte`.
- Code comments should be in English.
- C++ and HLSL matrix storage and multiplication order differ across paths in this codebase. Always verify CPU-side transposition before changing HLSL `mul` operand order.
