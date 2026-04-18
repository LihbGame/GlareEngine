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
