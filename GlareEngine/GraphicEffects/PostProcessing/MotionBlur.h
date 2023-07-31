#pragma once
#include "Graphics/CommandContext.h"

namespace MotionBlur
{
	extern bool IsEnable;

	void Initialize(void);

	void Shutdown(void);

	void GenerateCameraVelocityBuffer(CommandContext& BaseContext, const Camera& camera, bool UseLinearZ = true);

	void RenderMotionBlur(CommandContext& BaseContext, ColorBuffer& velocityBuffer, ColorBuffer* Input);

	void DrawUI();
};

