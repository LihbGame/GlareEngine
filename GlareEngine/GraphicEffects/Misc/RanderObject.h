#pragma once
#include "EngineUtility.h"
#include "RootSignature.h"

namespace GlareEngine {
	namespace DirectX12Graphics
	{
		class GraphicsContext;
	}
}

class RanderObject
{
public:
	RanderObject() {}
	~RanderObject() {}
	
	virtual void Draw(GraphicsContext& Context) = 0;
	virtual void BuildPSO(const RootSignature& rootSignature) = 0;
};

