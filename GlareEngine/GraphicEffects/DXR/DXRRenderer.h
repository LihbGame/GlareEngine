#pragma once

#include "Graphics/GPUBuffer.h"
#include "Graphics/UploadBuffer.h"
#include "Graphics/ColorBuffer.h"
#include "Graphics/RaytracingAccelerationStructureBuffer.h"
#include "Graphics/CommandContext.h"
#include "D3D12RaytracingHelper.h"
#include "Math/VectorMath.h"
#include "Math/Common.h"

using Microsoft::WRL::ComPtr;

struct DXRConstants
{
    Math::Matrix4 InvViewProj;
    Math::Vector3 CameraPos;
    float         Padding0;
    uint32_t      Width;
    uint32_t      Height;
    float         Padding1[2];
};

struct SimpleVertex
{
    Math::Vector3 Position;
};

class DXRRenderer
{
public:
    DXRRenderer() = default;
    ~DXRRenderer();

    DXRRenderer(const DXRRenderer&) = delete;
    DXRRenderer& operator=(const DXRRenderer&) = delete;

    void Initialize();
    void Update(float DeltaTime);
    void Render(GraphicsContext& Context);
    void Cleanup();

private:
    void CreateTriangleGeometry();
    void BuildAccelerationStructure(ID3D12GraphicsCommandList4* CmdList);
    void CreateRootSignature();
    void CreatePipelineStateObject();
    void CreateShaderBindingTable();
    void CreateOutputBuffer();
    void CreateConstantBuffer();

    GlareEngine::ByteAddressBuffer mVertexBuffer;
    GlareEngine::ByteAddressBuffer mIndexBuffer;

    struct RaytracingASBuffers
    {
        GlareEngine::RaytracingAccelerationStructureBuffer scratch;
        GlareEngine::RaytracingAccelerationStructureBuffer accelerationStructure;
        GlareEngine::UploadBuffer                            instanceDesc;
        UINT64                                               ResultDataMaxSizeInBytes = 0;
    };
    RaytracingASBuffers mBLAS;
    RaytracingASBuffers mTLAS;

    ComPtr<ID3D12RootSignature>    mRootSignature;
    ComPtr<ID3D12StateObject>      mPipelineState;
    ComPtr<ID3D12StateObjectProperties> mPipelineProperties;

    GlareEngine::UploadBuffer mRayGenTable;
    GlareEngine::UploadBuffer mMissTable;
    GlareEngine::UploadBuffer mHitGroupTable;

    D3D12_GPU_DESCRIPTOR_HANDLE mSceneColorUAVDescriptor = {};

    GlareEngine::UploadBuffer mConstantBuffer;
    DXRConstants*             mMappedCBV = nullptr;

    uint32_t mSBTRecordSize = 0;

    bool mInitialized = false;
    bool mAccelStructureBuilt = false;
};
