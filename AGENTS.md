# Repository Guidelines

## Project Structure & Module Organization

GlareEngine is a Windows x64 DirectX 12 rendering engine written in C++20. Core engine code lives in `GlareEngine/EngineCore/`; rendering features live in `GlareEngine/GraphicEffects/`; HLSL shaders live in `GlareEngine/Shaders/`, usually mirroring the rendering module layout. `GameDemo/` is the main executable, `EngineDemo_Old/` is legacy sample code, and shared runtime assets are under `Resource/`. Native dependencies and SDK files are kept in `packages/` and `GlareEngine/Dependences/`.

## Build, Test, and Development Commands

- `cmake -B build -G "Visual Studio 17 2022" -A x64`: configure the Visual Studio 2022 x64 build.
- `cmake --build build --config Debug`: build the engine and demos with debug symbols.
- `cmake --build build --config Release`: build optimized binaries.
- `Build_Windows.bat`: project-provided Windows build helper.

Build outputs are generated under `build/` and runtime binaries under `bin/Debug/` or `bin/Release/`. CMake compiles shaders with `dxc` and emits generated headers in `GlareEngine/Shaders/CompiledShaders/`.

## Coding Style & Naming Conventions

Use C++20 and the existing engine abstractions before adding new interfaces. Keep comments in English. `_HAS_STD_BYTE=0` is defined globally, so avoid `std::byte`; use `uint8_t` or `BYTE`. HLSL file suffixes define shader type: `*VS.hlsl`, `*PS.hlsl`, `*CS.hlsl`, `*HS.hlsl`, `*DS.hlsl`, and `*GS.hlsl`. Be careful with matrix conventions: verify CPU-side transposition before changing HLSL `mul` operand order.

## Testing Guidelines

There is no dedicated unit test suite in this repository. Treat successful Debug and Release builds as the minimum validation. For rendering changes, verify the affected path visually and, when possible, inspect the pass in RenderDoc. Confirm shader headers are regenerated when shader sources or includes change.

## Commit & Pull Request Guidelines

Recent commits use short, imperative summaries such as `Fix terrain shadow rendering and add shadow intensity control`. Keep commit messages focused on one change. Pull requests should describe the rendering path or subsystem touched, list validation steps, and include screenshots or captures for visual changes. Link related issues when available.

## Agent-Specific Instructions

Do not skip build/link verification after code changes. Check changed files for mojibake before submitting. Prefer existing module patterns in `GlareEngine/GraphicEffects/` and shader resource conventions in `GlareEngine/Shaders/`.
