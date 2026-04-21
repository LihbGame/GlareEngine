# GlareEngine DXR 单三角形光追实现方案 — 程序设计文档

> 基于 DXRPathTracer 参考实现，在 GlareEngine 中实现硬件光线追踪渲染单个三角形

---

## 1. 概述

### 1.1 目标

在 GlareEngine 现有框架中，利用 DirectX Raytracing (DXR) 渲染一个纯色三角形，作为引擎光追管线的最小可用原型（MVP）。

### 1.2 当前状态

| 组件 | 状态 | 说明 |
|------|------|------|
| DXR 设备检测 | **已完成** | `GraphicsCore::IsDirectXRaytracingSupported()` 检测 Tier 1.1 |
| 渲染管线枚举 | **已完成** | `Render::RenderPipeline::RayTracing` 已定义 |
| GUI 管线切换 | **已完成** | EngineGUI 支持选择 RayTracing 管线 |
| 加速结构定义 | **已完成** | `AccelerationStructureBuffers` 结构体已定义 |
| 描述符堆 | **已完成** | `gRayTracingBufferHeap` 已预留 |
| BLAS/TLAS 构建 | **未实现** | `Scene::BuildRTAccelerationStructure()` 为空 |
| 光追管线状态 | **未实现** | 无 PSO、Root Signature、SBT |
| 光追 Shader | **未实现** | 无 RayGen/ClosestHit/Miss HLSL 文件 |
| DispatchRays | **未实现** | 无光追命令调度 |

### 1.3 参考实现

DXRPathTracer 项目提供完整 DXR Path Tracing 实现，本方案从中提取最小必要逻辑。

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                    GameDemo (应用层)                      │
│   选择 RenderPipeline::RayTracing → 触发光追渲染路径      │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│                   Scene (场景管理层)                       │
│   Update() → 调用 DXRRenderer::Update()                  │
│   RenderScene() → 调用 DXRRenderer::Render()             │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│               DXRRenderer (光追渲染器) ← 新增             │
│                                                          │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ TriangleMesh│  │ Acceleration │  │  RT Pipeline   │  │
│  │  (顶点管理) │  │  Structure   │  │   State Obj    │  │
│  └─────────────┘  │  (BLAS/TLAS) │  │ (PSO+RootSig)  │  │
│                   └──────────────┘  └────────────────┘  │
│  ┌─────────────┐  ┌──────────────┐                      │
│  │ Output UAV  │  │ Shader Table │                      │
│  │ (渲染目标)  │  │    (SBT)     │                      │
│  └─────────────┘  └──────────────┘                      │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│              DXR Shaders (HLSL) ← 新增                   │
│   RayGenShader → TraceRay → ClosestHitShader / Miss     │
└─────────────────────────────────────────────────────────┘
```

---

## 3. 新增文件清单

| 文件路径 | 类型 | 说明 |
|----------|------|------|
| `GraphicEffects/DXR/DXRRenderer.h` | C++ Header | 光追渲染器类定义 |
| `GraphicEffects/DXR/DXRRenderer.cpp` | C++ Source | 光追渲染器实现 |
| `Shaders/DXR/RayTracing.hlsl` | HLSL | 光追着色器（RayGen + ClosestHit + Miss） |
| `EngineCore/Graphics/RaytracingAccelerationStructureBuffer.h` | C++ Header | **新增 Buffer 类**（AS 专用资源封装） |

---

## 4. 数据结构设计

### 4.1 常量缓冲区结构

```cpp
// DXRRenderer.h 中定义，与 HLSL 端一一对应

struct DXRConstants
{
    Math::Matrix4 InvViewProj;      // 逆投影矩阵，用于光线生成
    Math::Vector3 CameraPos;        // 相机世界坐标
    float         Padding0;
    uint32_t      Width;            // 渲染目标宽度
    uint32_t      Height;           // 渲染目标高度
    float         Padding1[2];
};
```

### 4.2 顶点数据

单三角形仅需 3 个顶点：

```cpp
struct SimpleVertex
{
    Math::Vector3 Position;
};
```

顶点数据（NDC 空间，居中显示）：

```
v0 = (-0.5, -0.5, 0.0)
v1 = ( 0.5, -0.5, 0.0)
v2 = ( 0.0,  0.5, 0.0)
```

---

## 5. 框架 Buffer 类使用规范

**所有 GPU 资源禁止直接持有 `ComPtr<ID3D12Resource>`，必须复用引擎现有 Buffer 封装。** 各场景对应类如下：

| 资源类型 | 使用类 | 头文件 |
|----------|--------|--------|
| 顶点/索引 Buffer (Default Heap) | `GlareEngine::ByteAddressBuffer` | `EngineCore/Graphics/GPUBuffer.h` |
| 上传堆 Buffer (CPU→GPU) | `GlareEngine::UploadBuffer` | `EngineCore/Graphics/UploadBuffer.h` |
| UAV/SRV 纹理 (渲染目标) | `GlareEngine::ColorBuffer` | `EngineCore/Graphics/ColorBuffer.h` |
| BLAS/TLAS Result & Scratch | `GlareEngine::RaytracingAccelerationStructureBuffer` | `EngineCore/Graphics/RaytracingAccelerationStructureBuffer.h` **(新增)** |

**统一访问约定**：框架 Buffer 均继承 `GPUResource`，通过 `.GetResource()` 获取底层 `ID3D12Resource*`，通过 `.GetGPUVirtualAddress()` 获取 GPU 地址，禁止直接操作 `m_pResource`。

---

## 6. DXRRenderer 类设计

### 6.1 新增 Buffer 类定义

```cpp
// EngineCore/Graphics/RaytracingAccelerationStructureBuffer.h
#pragma once
#include "GPUResource.h"

namespace GlareEngine
{
    class RaytracingAccelerationStructureBuffer : public GPUResource
    {
    public:
        void Create(const std::wstring& name, uint64_t size,
                    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
    };
}
```

> **为什么新增此类？** 加速结构是一种特殊资源：不需要 SRV/UAV/CBV/RTV/DSV 等任何描述符视图，初始状态为 `RAYTRACING_ACCELERATION_STRUCTURE` 或 `COMMON`+`ALLOW_UNORDERED_ACCESS`。现有 `ByteAddressBuffer` 会额外创建 Raw SRV/UAV 描述符并注册到全局光追描述符表，造成描述符浪费。新增极简子类可避免这一问题。

### 6.2 类声明

```cpp
// DXRRenderer.h
#pragma once

#include "EngineCore/Graphics/GPUBuffer.h"
#include "EngineCore/Graphics/UploadBuffer.h"
#include "EngineCore/Graphics/ColorBuffer.h"
#include "EngineCore/Graphics/RaytracingAccelerationStructureBuffer.h"
#include "EngineCore/Graphics/CommandContext.h"
#include "D3D12RaytracingHelper.h"
#include "EngineCore/Math/Math.h"

using Microsoft::WRL::ComPtr;

class DXRRenderer
{
public:
    DXRRenderer() = default;
    ~DXRRenderer();

    // 禁止拷贝
    DXRRenderer(const DXRRenderer&) = delete;
    DXRRenderer& operator=(const DXRRenderer&) = delete;

    /// 初始化光追资源（在引擎 Startup 阶段调用）
    void Initialize();

    /// 每帧更新（目前为空，预留扩展）
    void Update(float DeltaTime);

    /// 执行光追渲染
    void Render(ID3D12GraphicsCommandList4* CmdList);

    /// 释放资源
    void Cleanup();

private:
    // ---------- 初始化子步骤 ----------

    /// 创建单三角形的顶点/索引缓冲区
    void CreateTriangleGeometry();

    /// 构建 BLAS 和 TLAS
    void BuildAccelerationStructure(ID3D12GraphicsCommandList4* CmdList);

    /// 创建光追根签名
    void CreateRootSignature();

    /// 创建光追管线状态对象 (State Object)
    void CreatePipelineStateObject();

    /// 创建 Shader Binding Table
    void CreateShaderBindingTable();

    /// 创建输出 UAV 纹理
    void CreateOutputBuffer();

    /// 创建常量缓冲区并映射
    void CreateConstantBuffer();

    // ---------- 资源成员（全部使用引擎 Buffer 封装） ----------

    // 三角形几何 —— 使用 ByteAddressBuffer（继承 GPUBuffer，支持 Vertex/Index Buffer View）
    GlareEngine::ByteAddressBuffer mVertexBuffer;
    GlareEngine::ByteAddressBuffer mIndexBuffer;
    // 注：通过 ByteAddressBuffer::Create(initialData) 内部自动走 UploadBuffer 上传，无需额外持有 Upload Buffer

    // 加速结构 —— 使用新增 RaytracingAccelerationStructureBuffer + UploadBuffer
    struct RaytracingASBuffers
    {
        GlareEngine::RaytracingAccelerationStructureBuffer scratch;
        GlareEngine::RaytracingAccelerationStructureBuffer accelerationStructure;
        GlareEngine::UploadBuffer                            instanceDesc;   // TLAS only
        UINT64                                               ResultDataMaxSizeInBytes = 0;
    };
    RaytracingASBuffers mBLAS;
    RaytracingASBuffers mTLAS;

    // 光追管线
    ComPtr<ID3D12RootSignature>    mRootSignature;
    ComPtr<ID3D12StateObject>      mPipelineState;
    ComPtr<ID3D12StateObjectProperties> mPipelineProperties;

    // Shader Binding Table —— 使用 UploadBuffer（仅需上传堆，每帧/初始化时 Map 写入）
    GlareEngine::UploadBuffer mRayGenTable;
    GlareEngine::UploadBuffer mMissTable;
    GlareEngine::UploadBuffer mHitGroupTable;

    // 输出缓冲区 —— 使用 ColorBuffer（继承 PixelBuffer，自带 UAV/SRV/RTV 视图管理）
    GlareEngine::ColorBuffer mOutputUAV;
    D3D12_GPU_DESCRIPTOR_HANDLE mOutputUAVDescriptor = {};

    // 常量缓冲区 —— 使用 UploadBuffer（支持持续 Map，每帧直接 memcpy 更新）
    GlareEngine::UploadBuffer mConstantBuffer;
    DXRConstants*             mMappedCBV = nullptr;

    // 状态标记
    bool mInitialized = false;
    bool mAccelStructureBuilt = false;
};
```

### 6.3 核心方法流程图

```
Initialize()
  │
  ├── CreateTriangleGeometry()      // ByteAddressBuffer::Create(initialData)
  ├── CreateOutputBuffer()          // ColorBuffer::Create()
  ├── CreateConstantBuffer()        // UploadBuffer::Create() + Map()
  ├── CreateRootSignature()         // 4 个 Root Parameter
  ├── CreatePipelineStateObject()   // 编译 HLSL + 创建 State Object
  └── [延迟到首帧] BuildAccelerationStructure()
        ├── BuildBLAS()             // RaytracingAccelerationStructureBuffer::Create()
        └── BuildTLAS()             // RaytracingAccelerationStructureBuffer::Create()

Render(CmdList)
  │
  ├── 更新 ConstantBuffer (memcpy 到 mMappedCBV)
  ├── Resource Transition: mOutputUAV → D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  ├── SetComputeRootSignature(mRootSignature)
  ├── 绑定 Root Parameters:
  │     [0] TLAS (SRV, space200)
  │     [1] OutputUAV (Descriptor Table)
  │     [2] ConstantBuffer (Root CBV)
  ├── SetPipelineState1(mPipelineState)
  ├── DispatchRays(Width, Height, 1)
  └── Resource Transition: mOutputUAV → D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
```

---

## 7. 详细实现设计

### 7.1 CreateTriangleGeometry()

```
目标：创建包含 3 个顶点、3 个索引的 GPU 缓冲区

步骤：
1. 定义顶点数组：{(-0.5,-0.5,0), (0.5,-0.5,0), (0,0.5,0)}
2. 定义索引数组：{0, 1, 2}
3. 调用 mVertexBuffer.Create(L"DXR Vertex Buffer", 3, sizeof(SimpleVertex), vertices);
   → 内部在 Default Heap 创建资源，并通过 CommandContext::InitializeBuffer 完成上传
4. 调用 mIndexBuffer.Create(L"DXR Index Buffer", 3, sizeof(uint32_t), indices);
   → 同上，无需显式管理 Upload Buffer

关键说明：
  - ByteAddressBuffer 继承 GPUBuffer，提供 VertexBufferView() / IndexBufferView()
  - 构建 AS 时直接取 mVertexBuffer.GetGPUVirtualAddress()
  - 格式：Position = DXGI_FORMAT_R32G32B32_FLOAT，索引 = DXGI_FORMAT_R32_UINT
```

### 7.2 BuildAccelerationStructure()

```
目标：从三角形构建 BLAS，再用单个实例构建 TLAS

--- BLAS 构建 ---

1. 填充 D3D12_RAYTRACING_GEOMETRY_DESC：
   Type                = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES
   Flags               = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE
   Triangles.VertexBuffer.StartAddress = mVertexBuffer.GetGPUVirtualAddress()
   Triangles.VertexBuffer.StrideInBytes = sizeof(SimpleVertex) = 12
   Triangles.VertexCount            = 3
   Triangles.VertexFormat           = DXGI_FORMAT_R32G32B32_FLOAT
   Triangles.IndexBuffer            = mIndexBuffer.GetGPUVirtualAddress()
   Triangles.IndexCount             = 3
   Triangles.IndexFormat            = DXGI_FORMAT_R32_UINT

2. 查询预构建信息：
   D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {
     Type       = BOTTOM_LEVEL,
     Flags      = PREFER_FAST_TRACE,
     Layout     = ARRAY,
     NumDescs   = 1,
     pGeometryDescs = &geometryDesc
   }
   Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo)

3. 创建 Scratch Buffer：
   mBLAS.scratch.Create(L"BLAS Scratch", prebuildInfo.ScratchDataSizeInBytes,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   → RaytracingAccelerationStructureBuffer::Create 内部设置 ALLOW_UNORDERED_ACCESS

4. 创建 BLAS Result Buffer：
   mBLAS.accelerationStructure.Create(L"BLAS", prebuildInfo.ResultDataMaxSizeInBytes,
                                      D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

5. 发出构建命令：
   BuildDesc.Inputs  = inputs
   BuildDesc.DestAccelerationStructureData = mBLAS.accelerationStructure.GetGPUVirtualAddress()
   BuildDesc.ScratchAccelerationStructureData = mBLAS.scratch.GetGPUVirtualAddress()
   CmdList->BuildRaytracingAccelerationStructure(&BuildDesc, 0, nullptr)

6. UAV Barrier 等待 BLAS 构建完成
   （若使用 CommandContext 封装，可调用 InsertUAVBarrier(mBLAS.accelerationStructure)）

--- TLAS 构建 ---

1. 填充 D3D12_RAYTRACING_INSTANCE_DESC：
   Transform[0][0] = Transform[1][1] = Transform[2][2] = 1.0  (单位矩阵)
   InstanceMask    = 0xFF
   InstanceID      = 0
   InstanceContributionToHitGroupIndex = 0
   AccelerationStructure = mBLAS.accelerationStructure.GetGPUVirtualAddress()
   Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE

2. 创建 InstanceDesc Buffer（UploadBuffer）：
   mTLAS.instanceDesc.Create(L"TLAS InstanceDesc", sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
   void* mapped = mTLAS.instanceDesc.Map();
   memcpy(mapped, &instanceDesc, sizeof(instanceDesc));
   mTLAS.instanceDesc.Unmap();

3. 查询 TLAS 预构建信息（同 BLAS 步骤，Type 改为 TOP_LEVEL）

4. 创建 TLAS Scratch 和 Result Buffer（同 BLAS，使用 RaytracingAccelerationStructureBuffer）

5. 发出构建命令：
   TLASInputs.Type          = TOP_LEVEL
   TLASInputs.NumDescs      = 1
   TLASInputs.InstanceDescs = mTLAS.instanceDesc.GetGPUVirtualAddress()
   CmdList->BuildRaytracingAccelerationStructure(&TLASBuildDesc, 0, nullptr)

6. UAV Barrier 等待 TLAS 构建完成
```

### 7.3 CreateRootSignature()

```
设计 3 个 Root Parameter（最小化方案）：

Root Parameter Layout:
┌────────────┬──────────────────────────┬─────────────┐
│ Index      │ Type                     │ Shader Reg  │
├────────────┼──────────────────────────┼─────────────┤
│ [0] SRV    │ Root SRV (64-bit)        │ t0, space200│ ← TLAS 加速结构
│ [1] UAV    │ Descriptor Table (1 UAV) │ u0, space0  │ ← 输出渲染目标
│ [2] CBV    │ Root CBV (64-bit)        │ b0, space0  │ ← 常量缓冲区
└────────────┴──────────────────────────┴─────────────┘

Flags: D3D12_ROOT_SIGNATURE_FLAG_NONE
（DXR 要求所有 Shader 可见性为 ALL，且无 Local Root Signature）

代码要点：
  - param[0]: D3D12_ROOT_PARAMETER_TYPE_SRV, ShaderVisibility = ALL
  - param[1]: D3D12_DESCRIPTOR_RANGE_TYPE_UAV, NumDescriptors = 1
  - param[2]: D3D12_ROOT_PARAMETER_TYPE_CBV, ShaderVisibility = ALL
  - 无 Static Sampler（单三角形不需要纹理采样）
  - 使用 Version 1.1 (D3D12_ROOT_SIGNATURE_DESC1)
```

### 7.4 CreatePipelineStateObject()

```
目标：创建 D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE

State Object 子对象列表：

1. DXIL Library
   - 编译 RayTracing.hlsl 为 DXIL Library（shader model 6.6）
   - 导出入口点：
     - L"RayGenShader"
     - L"ClosestHitShader"
     - L"MissShader"

2. Hit Group
   - Type              = D3D12_HIT_GROUP_TYPE_TRIANGLES
   - HitGroupExport    = L"HitGroup"
   - ClosestHitShaderImport = L"ClosestHitShader"

3. Shader Config
   - MaxPayloadSizeInBytes    = 16   (float3 color + float pad = 16 bytes)
   - MaxAttributeSizeInBytes  = 8    (float2 barycentrics)

4. Global Root Signature
   - 指向 mRootSignature

5. Pipeline Config
   - MaxTraceRecursionDepth = 1（单三角形无反射/折射，1 次即可）

关键 API 调用顺序：
  CreateStateObject → QueryInterface(ID3D12StateObjectProperties)
  → 获取各 Shader Identifier
```

### 7.5 CreateShaderBindingTable()

```
Shader Binding Table (SBT) 内存布局：

RayGen Table（1 个 Record）:
┌─────────────────────────────────────────────┐
│ ShaderIdentifier(RayGenShader) | LocalArgs  │  → mRayGenTable (UploadBuffer)
└─────────────────────────────────────────────┘

Miss Table（1 个 Record）:
┌─────────────────────────────────────────────┐
│ ShaderIdentifier(MissShader)  | LocalArgs   │  → mMissTable (UploadBuffer)
└─────────────────────────────────────────────┘

Hit Group Table（1 个 Record）:
┌─────────────────────────────────────────────┐
│ ShaderIdentifier(HitGroup)    | LocalArgs   │  → mHitGroupTable (UploadBuffer)
└─────────────────────────────────────────────┘

每个 Record 大小 = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES (32) + LocalRootArgs(0)
                  = 32 bytes（对齐到 D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT = 64）

实现步骤：
1. mPipelineProperties->GetShaderIdentifier(L"RayGenShader") → 获取 32 字节 ID
2. 同理获取 MissShader、HitGroup 的 ID
3. mRayGenTable.Create(L"RayGen SBT", alignedSize);
4. void* mapped = mRayGenTable.Map();
   memcpy(mapped, shaderId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
   mRayGenTable.Unmap();
5. Miss / HitGroup Table 同理，均使用 UploadBuffer
```

### 7.6 CreateOutputBuffer()

```
目标：创建屏幕分辨率的 UAV 纹理，作为光追输出

参数：
  - Width/Height  = 当前 SwapChain 尺寸（从 GraphicsCore 获取）
  - Format        = DXGI_FORMAT_R8G8B8A8_UNORM
  - Heap Type     = Default
  - Flags         = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
  - Initial State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE

使用 ColorBuffer::Create()：
  mOutputUAV.Create(L"DXR Output UAV", Width, Height, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

同时创建 UAV 描述符：
  - ColorBuffer 内部已创建 UAV (m_UAVHandle[0])
  - 在 gRayTracingBufferHeap 中分配一个 GPU-visible slot
  - 调用 g_Device->CopyDescriptorsSimple 将 mOutputUAV.GetUAV() 拷贝到 GPU heap
  - 保存得到的 mOutputUAVDescriptor
```

### 7.7 CreateConstantBuffer()

```
目标：创建常量缓冲区并获取持久映射指针

实现：
  mConstantBuffer.Create(L"DXR Constant Buffer", sizeof(DXRConstants));
  mMappedCBV = static_cast<DXRConstants*>(mConstantBuffer.Map());
  // UploadBuffer::Map() 返回 CPU 可写指针，后续每帧直接 memcpy 即可
  // 不需要创建 CBV 描述符，Render 阶段直接传 Root CBV (GPUVirtualAddress)
```

### 7.8 Render()

```cpp
void DXRRenderer::Render(ID3D12GraphicsCommandList4* CmdList)
{
    if (!mInitialized) return;

    // 1. 更新常量缓冲区（直接写入已映射的 UploadBuffer 内存）
    DXRConstants cb = {};
    cb.CameraPos = g_Camera.GetPosition();
    cb.InvViewProj = Math::Matrix4::Invert(g_Camera.GetViewProjectionMatrix());
    cb.Width  = g_RenderWidth;
    cb.Height = g_RenderHeight;
    memcpy(mMappedCBV, &cb, sizeof(DXRConstants));

    // 2. Transition Output UAV → 写入状态
    // 方式 A（推荐）：若使用 CommandContext 封装
    //   context.TransitionResource(mOutputUAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    //   context.FlushResourceBarriers();
    // 方式 B（直接使用 CmdList）：
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mOutputUAV.GetResource(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    CmdList->ResourceBarrier(1, &barrier);

    // 3. 设置根签名和管线状态
    CmdList->SetComputeRootSignature(mRootSignature.Get());
    CmdList->SetPipelineState1(mPipelineState.Get());

    // 4. 绑定 Root Parameters
    CmdList->SetComputeRootShaderResourceView(0, mTLAS.accelerationStructure.GetGPUVirtualAddress());
    CmdList->SetComputeRootDescriptorTable(1, mOutputUAVDescriptor);
    CmdList->SetComputeRootConstantBufferView(2, mConstantBuffer.GetGPUVirtualAddress());

    // 5. Dispatch Rays
    D3D12_DISPATCH_RAYS_DESC dispatch = {};
    dispatch.RayGenerationShaderRecord.StartAddress  = mRayGenTable.GetGPUVirtualAddress();
    dispatch.RayGenerationShaderRecord.SizeInBytes    = 64;  // 单条 Record
    dispatch.MissShaderTable.StartAddress             = mMissTable.GetGPUVirtualAddress();
    dispatch.MissShaderTable.SizeInBytes              = 64;
    dispatch.MissShaderTable.StrideInBytes            = 64;
    dispatch.HitGroupTable.StartAddress               = mHitGroupTable.GetGPUVirtualAddress();
    dispatch.HitGroupTable.SizeInBytes                = 64;
    dispatch.HitGroupTable.StrideInBytes              = 64;
    dispatch.Width  = cb.Width;
    dispatch.Height = cb.Height;
    dispatch.Depth  = 1;

    CmdList->DispatchRays(&dispatch);

    // 6. Transition Output UAV → 可读状态（供后续 Present 使用）
    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        mOutputUAV.GetResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    CmdList->ResourceBarrier(1, &barrier2);
}
```

---

## 8. Shader 设计 (RayTracing.hlsl)

```hlsl
//=============================================================================
// GlareEngine DXR - 单三角形光线追踪 Shader
// Shader Model 6.6
//=============================================================================

// --- 常量缓冲区 ---
cbuffer DXRConstants : register(b0)
{
    row_major float4x4 InvViewProj;
    float3 CameraPos;
    uint   Width;
    uint   Height;
};

// --- 加速结构和输出 ---
RaytracingAccelerationStructure SceneAS : register(t0, space200);
RWTexture2D<float4> OutputTexture : register(u0);

// --- Payload ---
struct RayPayload
{
    float3 Color;
};

//-----------------------------------------------------------------------------
// Ray Generation Shader
//-----------------------------------------------------------------------------
[shader("raygeneration")]
void RayGenShader()
{
    // 计算像素坐标和 NDC
    uint2 pixelCoord = DispatchRaysIndex().xy;
    uint2 dims       = DispatchRaysDimensions().xy;

    float2 ndc = (float2(pixelCoord) + 0.5) / float2(dims) * 2.0 - 1.0;
    ndc.y = -ndc.y; // 翻转 Y 轴

    // 通过逆投影矩阵生成光线
    float4 origin = mul(float4(ndc, 0.0, 1.0), InvViewProj);
    float4 target = mul(float4(ndc, 1.0, 1.0), InvViewProj);
    origin.xyz /= origin.w;
    target.xyz /= target.w;

    float3 rayDir = normalize(target.xyz - origin.xyz);

    // 设置光线参数
    RayDesc ray;
    ray.Origin    = origin.xyz;
    ray.Direction = rayDir;
    ray.TMin      = 0.001;
    ray.TMax      = 10000.0;

    // 追踪光线
    RayPayload payload;
    payload.Color = float3(0.0, 0.0, 0.0);

    TraceRay(SceneAS,           // 加速结构
             RAY_FLAG_NONE,     // 追踪标志
             0xFF,              // 实例掩码
             0,                 // Ray Contribution To Hit Group Index
             1,                 // Multiplier For Geometry Contribution
             0,                 // Miss Shader Index
             ray,               // 光线描述
             payload);          // 载荷

    // 写入输出
    OutputTexture[pixelCoord] = float4(payload.Color, 1.0);
}

//-----------------------------------------------------------------------------
// Closest Hit Shader — 三角形命中时调用
//-----------------------------------------------------------------------------
[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // 用重心坐标插值三角形顶点颜色
    // attr.barycentrics.x = weight for vertex 1
    // attr.barycentrics.y = weight for vertex 2
    // weight for vertex 0 = 1 - x - y
    float3 v0Color = float3(1.0, 0.0, 0.0);  // 红色
    float3 v1Color = float3(0.0, 1.0, 0.0);  // 绿色
    float3 v2Color = float3(0.0, 0.0, 1.0);  // 蓝色

    float baryW = 1.0 - attr.barycentrics.x - attr.barycentrics.y;
    payload.Color = v0Color * baryW
                  + v1Color * attr.barycentrics.x
                  + v2Color * attr.barycentrics.y;
}

//-----------------------------------------------------------------------------
// Miss Shader — 光线未命中任何几何体时调用
//-----------------------------------------------------------------------------
[shader("miss")]
void MissShader(inout RayPayload payload)
{
    // 背景颜色：深灰色渐变
    float t = 0.5 + 0.5 * WorldRayDirection().y;
    payload.Color = float3(0.1, 0.1, 0.15) * (1.0 - t) + float3(0.3, 0.3, 0.4) * t;
}
```

**Shader 关键设计点：**
- RayGen 通过逆投影矩阵从像素坐标生成世界空间光线
- ClosestHit 使用重心坐标插值 RGB 三色，生成经典彩色三角形
- Miss 返回渐变背景色，与三角形形成对比
- MaxTraceRecursionDepth = 1（无递归反射）

---

## 9. 引擎集成方案

### 9.1 修改 Scene 类

在 `Scene.h` 中添加：

```cpp
// Scene.h
#include "GraphicEffects/DXR/DXRRenderer.h"

class Scene
{
    // ... 现有成员 ...

    // 新增
    DXRRenderer mDXRRenderer;
};
```

在 `Scene.cpp` 中的关键位置添加调用：

```cpp
// Scene::Startup() 中
void Scene::Startup()
{
    // ... 现有初始化 ...

    if (Render::GetRenderPipeline() == Render::RayTracing)
    {
        mDXRRenderer.Initialize();
    }
}

// Scene::Update() 中
void Scene::Update(float DeltaTime)
{
    // ... 现有更新 ...

    mDXRRenderer.Update(DeltaTime);
}

// Scene::RenderScene() 中 — 在管线分支处
void Scene::RenderScene(RasterRenderPipelineType Type)
{
    if (m_pGUI->GetRenderPipelineIndex() == Render::RayTracing)
    {
        // 光追渲染路径
        GraphicsContext& context = GraphicsContext::Begin(L"DXR Render");
        ID3D12GraphicsCommandList4* cmdList4 = nullptr;
        context.GetCommandList()->QueryInterface(IID_PPV_ARGS(&cmdList4));

        mDXRRenderer.Render(cmdList4);

        cmdList4->Release();
        context.Finish();

        // 将光追结果 Copy 到 BackBuffer
        PresentRayTracingResult();
    }
    else
    {
        // 现有光栅化路径 ...
    }
}
```

### 9.2 结果呈现

光追渲染到 UAV 纹理后，需要将结果绘制到 SwapChain BackBuffer：

```
方案：全屏 Quad Copy

1. 在光追 Render() 结束后，Output UAV 转为 PIXEL_SHADER_RESOURCE
2. 绘制全屏三角形/四边形
3. Pixel Shader 中采样 Output UAV 纹理（通过 SRV）
4. 输出到 BackBuffer
```

### 9.3 CMake 修改

在 `GlareEngine/CMakeLists.txt` 中添加源文件：

```cmake
# DXR Source Files
GraphicEffects/DXR/DXRRenderer.cpp
EngineCore/Graphics/RaytracingAccelerationStructureBuffer.cpp
```

在 Shader 编译部分添加：

```cmake
# Ray Tracing Shader (编译为 DXIL Library)
add_custom_command(
    OUTPUT ${SHADER_OUTPUT_DIR}/RayTracing.cso
    COMMAND dxc -T lib_6_6 -E "" -Zi -Fo ${SHADER_OUTPUT_DIR}/RayTracing.cso
            ${CMAKE_SOURCE_DIR}/Shaders/DXR/RayTracing.hlsl
    DEPENDS ${CMAKE_SOURCE_DIR}/Shaders/DXR/RayTracing.hlsl
)
```

---

## 10. 资源生命周期

```
Startup()
  │
  ├── DXRRenderer::Initialize()
  │     ├── CreateTriangleGeometry()    → ByteAddressBuffer (一次性)
  │     ├── CreateOutputBuffer()        → ColorBuffer (随窗口 Resize 重建)
  │     ├── CreateConstantBuffer()      → UploadBuffer (持久 Map)
  │     ├── CreateRootSignature()       → Root Signature
  │     └── CreatePipelineStateObject() → State Object
  │
  ├── [首帧] BuildAccelerationStructure()
  │     ├── BuildBLAS()                 → RaytracingAccelerationStructureBuffer
  │     └── BuildTLAS()                 → RaytracingAccelerationStructureBuffer
  │
  └── CreateShaderBindingTable()        → UploadBuffer

每帧 Update()
  └── DXRRenderer::Update()  → nop (预留)

每帧 Render()
  └── DXRRenderer::Render()
        ├── memcpy CBV (写入 UploadBuffer 映射内存)
        ├── Transition UAV (ColorBuffer)
        ├── DispatchRays
        └── Transition UAV (ColorBuffer)

OnResize()
  └── 重建 mOutputUAV (ColorBuffer::Create 新分辨率)

Cleanup()
  └── DXRRenderer::Cleanup()
        └── 各 Buffer 对象调用 Destroy() 释放
        └── UploadBuffer Unmap() 后自动释放
```

**关键生命周期说明**：
- `ByteAddressBuffer`、`ColorBuffer`、`UploadBuffer`、`RaytracingAccelerationStructureBuffer` 均在析构时自动调用 `Destroy()`，释放底层 `ID3D12Resource`。
- `UploadBuffer` 在 `Map()` 后应保持映射状态（用于每帧更新），在 `Cleanup()` 中调用 `Unmap()` 后再销毁。
- 所有框架 Buffer 内部使用 `ComPtr<ID3D12Resource>`，无需手动 `Release()`。

---

## 11. 调试与验证

### 11.1 验证步骤

| 步骤 | 验证项 |
|------|--------|
| 1 | DXR 设备检测通过（`IsDirectXRaytracingSupported()` 返回 true） |
| 2 | BLAS/TLAS 构建无 D3D12 错误，Debug Layer 无警告 |
| 3 | State Object 创建成功 |
| 4 | SBT 中 Shader Identifier 正确获取（非全零） |
| 5 | DispatchRays 执行后 Output UAV 非空 |
| 6 | 屏幕显示彩色三角形（RGB 三色插值）+ 渐变背景 |

### 11.2 PIX 调试要点

- 使用 `PIX_GPU_BEGIN`/`END` 标记光追渲染区域
- 在 PIX 中检查 `DispatchRays` 调用的输入/输出
- 验证加速结构内容（PIX 支持可视化 BVH）
- 检查 SBT 布局是否正确

### 11.3 常见问题排查

| 问题 | 原因 | 解决 |
|------|------|------|
| 黑屏无输出 | TLAS 地址未绑定 | 检查 Root Param [0] 绑定 |
| D3D12 设备移除 | AS 构建参数错误 | 验证顶点缓冲区地址/大小 |
| 三角形位置不对 | 顶点在 NDC 而非世界空间 | 将三角形顶点定义在世界空间原点附近 |
| Debug Layer 报错 | SBT 大小未对齐 | 确保 Record 大小对齐到 64 字节 |

---

## 12. 后续扩展方向

本方案完成后，可逐步扩展为完整光追系统：

```
单三角形 (MVP)
  │
  ├── 加载任意模型 (替换硬编码顶点为 Model 数据)
  ├── PBR 材质系统 (纹理采样 + BRDF 计算)
  ├── 光照系统 (阴影光线 + 环境光)
  ├── 递归反射/折射 (增大 MaxTraceRecursionDepth)
  ├── 多次弹跳 Path Tracing (渐进式累积)
  └── 混合渲染 (光栅化 + 光追反射/阴影)
```

---

## 13. 修改文件汇总

| 文件 | 操作 | 修改内容 |
|------|------|----------|
| `GraphicEffects/DXR/DXRRenderer.h` | **新增** | 光追渲染器类 |
| `GraphicEffects/DXR/DXRRenderer.cpp` | **新增** | 光追渲染器实现 |
| `Shaders/DXR/RayTracing.hlsl` | **新增** | 光追着色器 |
| `EngineCore/Graphics/RaytracingAccelerationStructureBuffer.h` | **新增** | **AS 专用 Buffer 封装** |
| `EngineCore/Graphics/RaytracingAccelerationStructureBuffer.cpp` | **新增** | **AS Buffer 实现** |
| `EngineCore/Engine/Scene.h` | **修改** | 添加 DXRRenderer 成员 |
| `EngineCore/Engine/Scene.cpp` | **修改** | 在 RayTracing 管线分支调用 DXRRenderer |
| `CMakeLists.txt` | **修改** | 添加新源文件和 Shader 编译规则 |
