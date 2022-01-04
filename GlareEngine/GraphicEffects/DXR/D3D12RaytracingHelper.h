#pragma once

//32 bit align
#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

//Acceleration Structure Buffers
struct AccelerationStructureBuffers
{
	ComPtr<ID3D12Resource> scratch;
	ComPtr<ID3D12Resource> accelerationStructure;
	ComPtr<ID3D12Resource> instanceDesc;    // Used only for top-level AS
	UINT64                 ResultDataMaxSizeInBytes;
};

