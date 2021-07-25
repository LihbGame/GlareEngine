#pragma once
#include "EngineUtility.h"
#include "RootSignature.h"
class RanderObject
{
public:
	RanderObject() {}
	~RanderObject() {}
	
	virtual void Draw() = 0;
	virtual void BuildPSO(const RootSignature& rootSignature) = 0;
};

