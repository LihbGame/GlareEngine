#include "FrameResource.h"
#define InstanceCount 25
FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT InstanceModelSubMeshNum,UINT  skinnedObjectCount,UINT objectCount, UINT materialCount, UINT waveVertCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

  //  FrameCB = std::make_unique<UploadBuffer<FrameConstants>>(device, 1, true);
    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
    SimpleObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
    SkinnedCB = std::make_unique<UploadBuffer<SkinnedConstants>>(device, skinnedObjectCount, true);
    for (int i = 0; i < InstanceModelSubMeshNum; ++i)
    {
        InstanceSimpleObjectCB.push_back(std::make_unique<UploadBuffer<InstanceConstants>>(device, InstanceCount, false));
    }
    WavesVB = std::make_unique<UploadBuffer<PosNormalTexc>>(device, waveVertCount, false);
}

FrameResource::~FrameResource()
{

}