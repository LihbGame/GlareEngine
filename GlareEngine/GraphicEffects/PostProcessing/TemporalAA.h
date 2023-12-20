#pragma once
#include "Engine/EngineProfiling.h"

class GlareEngine::CommandContext;

namespace TemporalAA
{
	void Initialize(void);

	void Shutdown(void);

	// Call once per frame to increment the internal frame counter and, in the case of TAA, choosing the next
	// jittered sample position.
	void Update(uint64_t FrameIndex);

	// Returns whether the frame is odd or even, relevant to checkerboard rendering.
	uint32_t GetFrameIndexMod2(void);

	// Jittering only occurs when TemporalAA is enabled.  
	// You can use these values to jitter your viewport or projection matrix.
	XMFLOAT2 GetJitterOffset();

	void ApplyTemporalAA(ComputeContext& Context, ColorBuffer& scenecolor);

	void ClearHistory(CommandContext& Context);

}	// namespace TemporalAA
