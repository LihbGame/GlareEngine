#pragma once
#include "EngineUtility.h"
class RanderObject
{
public:
	RanderObject() {}
	~RanderObject() {}
	
	virtual void Draw() = 0;
	virtual void SetPSO() = 0;
};

