#include "DXRRenderer.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/Display.h"
#include "Graphics/Render.h"
#include "Graphics/BufferManager.h"
#include "Engine/EngineGlobal.h"
#include "Engine/Camera.h"
#include "Engine/EngineLog.h"
#include "Graphics/DescriptorHeap.h"
#include "Engine/Tools/ShaderCompiler.h"
#include "Engine/Scene.h"

using namespace GlareEngine;
using namespace GlareEngine::Math;

DXRRenderer::~DXRRenderer()
{
    Cleanup();
}

void DXRRenderer::Cleanup()
{
    if (mMappedCBV)
    {
        mConstantBuffer.Unmap();
        mMappedCBV = nullptr;
    }

    mVertexBuffer.Destroy();
    mIndexBuffer.Destroy();
    mBLAS.scratch.Destroy();
    mBLAS.accelerationStructure.Destroy();
    mTLAS.scratch.Destroy();
    mTLAS.accelerationStructure.Destroy();
    mTLAS.instanceDesc.Destroy();
    mConstantBuffer.Destroy();
    mRayGenTable.Destroy();
    mMissTable.Destroy();
    mHitGroupTable.Destroy();

    mRootSignature.Reset();
    mPipelineState.Reset();
    mPipelineProperties.Reset();

    mInitialized = false;
    mAccelStructureBuilt = false;
}

void DXRRenderer::Initialize()
{
    if (mInitialized)
        return;

    CreateTriangleGeometry();
    CreateOutputBuffer();
    CreateConstantBuffer();
    CreateRootSignature();
    CreatePipelineStateObject();
    CreateShaderBindingTable();

    mInitialized = true;
}

void DXRRenderer::Update(float DeltaTime)
{
    (void)DeltaTime;
}

void DXRRenderer::CreateTriangleGeometry()
{
    SimpleVertex vertices[3] =
    {
        { Math::Vector3(-0.5f, -0.5f, 0.0f) },
        { Math::Vector3( 0.5f, -0.5f, 0.0f) },
        { Math::Vector3( 0.0f,  0.5f, 0.0f) }
    };

    uint32_t indices[3] = { 0, 1, 2 };

    mVertexBuffer.Create(L"DXR Vertex Buffer", 3, sizeof(SimpleVertex), vertices);
    mIndexBuffer.Create(L"DXR Index Buffer", 3, sizeof(uint32_t), indices);
}

void DXRRenderer::BuildAccelerationStructure(ID3D12GraphicsCommandList4* CmdList)
{
    if (mAccelStructureBuilt)
        return;

    // ---- Build BLAS ----
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geometryDesc.Triangles.VertexBuffer.StartAddress = mVertexBuffer.GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(SimpleVertex);
    geometryDesc.Triangles.VertexCount = 3;
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.IndexBuffer = mIndexBuffer.GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexCount = 3;
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
    blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    blasInputs.NumDescs = 1;
    blasInputs.pGeometryDescs = &geometryDesc;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo = {};
    ComPtr<ID3D12Device5> device5;
    ThrowIfFailed(g_Device->QueryInterface(IID_PPV_ARGS(&device5)));
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuildInfo);

    mBLAS.scratch.Create(L"BLAS Scratch", blasPrebuildInfo.ScratchDataSizeInBytes,
                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    mBLAS.accelerationStructure.Create(L"BLAS", blasPrebuildInfo.ResultDataMaxSizeInBytes,
                                        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc = {};
    blasBuildDesc.Inputs = blasInputs;
    blasBuildDesc.DestAccelerationStructureData = mBLAS.accelerationStructure.GetGPUVirtualAddress();
    blasBuildDesc.ScratchAccelerationStructureData = mBLAS.scratch.GetGPUVirtualAddress();

    CmdList->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = mBLAS.accelerationStructure.GetResource();
    CmdList->ResourceBarrier(1, &barrier);

    // ---- Build TLAS ----
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.Transform[0][0] = 1.0f;
    instanceDesc.Transform[1][1] = 1.0f;
    instanceDesc.Transform[2][2] = 1.0f;
    instanceDesc.InstanceMask = 0xFF;
    instanceDesc.InstanceID = 0;
    instanceDesc.InstanceContributionToHitGroupIndex = 0;
    instanceDesc.AccelerationStructure = mBLAS.accelerationStructure.GetGPUVirtualAddress();
    instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

    mTLAS.instanceDesc.Create(L"TLAS InstanceDesc", sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    void* mapped = mTLAS.instanceDesc.Map();
    memcpy(mapped, &instanceDesc, sizeof(instanceDesc));
    mTLAS.instanceDesc.Unmap();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    tlasInputs.NumDescs = 1;
    tlasInputs.InstanceDescs = mTLAS.instanceDesc.GetGPUVirtualAddress();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

    mTLAS.scratch.Create(L"TLAS Scratch", tlasPrebuildInfo.ScratchDataSizeInBytes,
                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    mTLAS.accelerationStructure.Create(L"TLAS", tlasPrebuildInfo.ResultDataMaxSizeInBytes,
                                        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc = {};
    tlasBuildDesc.Inputs = tlasInputs;
    tlasBuildDesc.DestAccelerationStructureData = mTLAS.accelerationStructure.GetGPUVirtualAddress();
    tlasBuildDesc.ScratchAccelerationStructureData = mTLAS.scratch.GetGPUVirtualAddress();

    CmdList->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER tlasBarrier = {};
    tlasBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    tlasBarrier.UAV.pResource = mTLAS.accelerationStructure.GetResource();
    CmdList->ResourceBarrier(1, &tlasBarrier);

    mAccelStructureBuilt = true;
}

void DXRRenderer::CreateRootSignature()
{
    CD3DX12_ROOT_PARAMETER1 rootParams[3];
    rootParams[0].InitAsShaderResourceView(0, 200);  // t0, space200: TLAS
    rootParams[1].InitAsDescriptorTable(1, nullptr); // UAV descriptor table
    {
        CD3DX12_DESCRIPTOR_RANGE1 range;
        range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
        rootParams[1].InitAsDescriptorTable(1, &range);
    }
    rootParams[2].InitAsConstantBufferView(0, 0);    // b0: constants

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(3, rootParams, 0, nullptr,
                         D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_1, &serializedRootSig, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            EngineLog::AddLog(L"DXR Root Signature Error: %hs",
                (char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);
    }

    ThrowIfFailed(g_Device->CreateRootSignature(0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));
}

void DXRRenderer::CreatePipelineStateObject()
{
    // Compile the HLSL library to DXIL using dxc
    ShaderBinary rayGenBinary = ShaderCompiler::Get().Compile(
        "Shaders/DXR/RayTracing.hlsl", "RayGenShader", "lib_6_6", ShaderDefinitions());

    if (!rayGenBinary.IsValid())
    {
        EngineLog::AddLog(L"Failed to compile RayTracing.hlsl");
        return;
    }

    // Define DXIL library subobject
    D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
    dxilLibDesc.DXILLibrary.pShaderBytecode = rayGenBinary.ByteCode.pShaderBytecode;
    dxilLibDesc.DXILLibrary.BytecodeLength = rayGenBinary.ByteCode.BytecodeLength;

    LPCWSTR exports[] = { L"RayGenShader", L"ClosestHitShader", L"MissShader" };
    D3D12_EXPORT_DESC exportDescs[3] = {};
    for (int i = 0; i < 3; ++i)
    {
        exportDescs[i].Name = exports[i];
        exportDescs[i].Flags = D3D12_EXPORT_FLAG_NONE;
    }
    dxilLibDesc.NumExports = 3;
    dxilLibDesc.pExports = exportDescs;

    // Hit group
    D3D12_HIT_GROUP_DESC hitGroupDesc = {};
    hitGroupDesc.HitGroupExport = L"HitGroup";
    hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroupDesc.ClosestHitShaderImport = L"ClosestHitShader";

    // Shader config
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = sizeof(float) * 4;   // float3 color + pad
    shaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2; // float2 barycentrics

    // Pipeline config
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 1;

    // Global root signature association
    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSig = {};
    globalRootSig.pGlobalRootSignature = mRootSignature.Get();

    // Build state object
    D3D12_STATE_SUBOBJECT subobjects[5] = {};

    subobjects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    subobjects[0].pDesc = &dxilLibDesc;

    subobjects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[1].pDesc = &hitGroupDesc;

    subobjects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subobjects[2].pDesc = &shaderConfig;

    subobjects[3].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subobjects[3].pDesc = &globalRootSig;

    subobjects[4].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[4].pDesc = &pipelineConfig;

    D3D12_STATE_OBJECT_DESC stateObjectDesc = {};
    stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjectDesc.NumSubobjects = 5;
    stateObjectDesc.pSubobjects = subobjects;

    ComPtr<ID3D12Device5> device5;
    ThrowIfFailed(g_Device->QueryInterface(IID_PPV_ARGS(&device5)));
    ThrowIfFailed(device5->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&mPipelineState)));
    ThrowIfFailed(mPipelineState.As(&mPipelineProperties));
}

void DXRRenderer::CreateShaderBindingTable()
{
    const uint32_t shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const uint32_t recordSize = Math::AlignUp(shaderIdentifierSize,
        D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

    void* rayGenId = mPipelineProperties->GetShaderIdentifier(L"RayGenShader");
    void* missId = mPipelineProperties->GetShaderIdentifier(L"MissShader");
    void* hitGroupId = mPipelineProperties->GetShaderIdentifier(L"HitGroup");

    mRayGenTable.Create(L"RayGen SBT", recordSize);
    void* mappedRG = mRayGenTable.Map();
    memcpy(mappedRG, rayGenId, shaderIdentifierSize);
    mRayGenTable.Unmap();

    mMissTable.Create(L"Miss SBT", recordSize);
    void* mappedMiss = mMissTable.Map();
    memcpy(mappedMiss, missId, shaderIdentifierSize);
    mMissTable.Unmap();

    mHitGroupTable.Create(L"HitGroup SBT", recordSize);
    void* mappedHG = mHitGroupTable.Map();
    memcpy(mappedHG, hitGroupId, shaderIdentifierSize);
    mHitGroupTable.Unmap();

    mSBTRecordSize = recordSize;
}

void DXRRenderer::CreateOutputBuffer()
{
    // Use the global scene color buffer as DXR output target
    D3D12_CPU_DESCRIPTOR_HANDLE cpuUAV = GlareEngine::g_SceneColorBuffer.GetUAV();
    int rtIndex = AddToGlobalRayTracingDescriptor(cpuUAV);
    Render::CopyRayTracingBufferHeap();

    mSceneColorUAVDescriptor = Render::gRayTracingBufferHeap[rtIndex];
}

void DXRRenderer::CreateConstantBuffer()
{
    mConstantBuffer.Create(L"DXR Constant Buffer", sizeof(DXRConstants));
    mMappedCBV = static_cast<DXRConstants*>(mConstantBuffer.Map());
}

void DXRRenderer::Render(GraphicsContext& Context)
{
    if (!mInitialized)
        return;

    ID3D12GraphicsCommandList4* CmdList = nullptr;
    Context.GetCommandList()->QueryInterface(IID_PPV_ARGS(&CmdList));

    // Build AS on first frame
    if (!mAccelStructureBuilt)
    {
        BuildAccelerationStructure(CmdList);
    }

    // 1. Update constant buffer
    DXRConstants cb = {};
    Camera* camera = EngineGlobal::gCurrentScene->GetCamera();
    if (camera)
    {
        cb.CameraPos = Math::Vector3(camera->GetPosition3f());
        XMMATRIX viewProj = camera->GetViewProjection();
        cb.InvViewProj = Math::Invert(Math::Matrix4(viewProj));
    }
    cb.Width = Display::g_RenderWidth;
    cb.Height = Display::g_RenderHeight;
    memcpy(mMappedCBV, &cb, sizeof(DXRConstants));

    // 2. Transition scene color buffer to UAV for DXR write
    Context.TransitionResource(GlareEngine::g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // 3. Set root signature and pipeline
    CmdList->SetComputeRootSignature(mRootSignature.Get());
    CmdList->SetPipelineState1(mPipelineState.Get());

    // 4. Bind root parameters
    CmdList->SetComputeRootShaderResourceView(0, mTLAS.accelerationStructure.GetGPUVirtualAddress());
    CmdList->SetComputeRootDescriptorTable(1, mSceneColorUAVDescriptor);
    CmdList->SetComputeRootConstantBufferView(2, mConstantBuffer.GetGPUVirtualAddress());

    // 5. Dispatch rays
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    dispatchDesc.RayGenerationShaderRecord.StartAddress = mRayGenTable.GetGPUVirtualAddress();
    dispatchDesc.RayGenerationShaderRecord.SizeInBytes = mSBTRecordSize;
    dispatchDesc.MissShaderTable.StartAddress = mMissTable.GetGPUVirtualAddress();
    dispatchDesc.MissShaderTable.SizeInBytes = mSBTRecordSize;
    dispatchDesc.MissShaderTable.StrideInBytes = mSBTRecordSize;
    dispatchDesc.HitGroupTable.StartAddress = mHitGroupTable.GetGPUVirtualAddress();
    dispatchDesc.HitGroupTable.SizeInBytes = mSBTRecordSize;
    dispatchDesc.HitGroupTable.StrideInBytes = mSBTRecordSize;
    dispatchDesc.Width = cb.Width;
    dispatchDesc.Height = cb.Height;
    dispatchDesc.Depth = 1;

    CmdList->DispatchRays(&dispatchDesc);

    // 6. Transition scene color buffer back to pixel shader resource for present
    Context.TransitionResource(GlareEngine::g_SceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if (CmdList)
    {
        CmdList->Release();
    }
}
