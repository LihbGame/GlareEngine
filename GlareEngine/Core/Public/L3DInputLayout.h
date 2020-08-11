#pragma once
#include "L3DUtil.h"
class L3DInputLayout
{
public:
	static std::vector<D3D12_INPUT_ELEMENT_DESC> Pos;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> PosNormalTexc;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> PosNormalTangentTexc;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> InstancePosNormalTangentTexc;
};

