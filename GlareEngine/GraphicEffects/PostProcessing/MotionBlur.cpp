#include "MotionBlur.h"
#include "Graphics/PipelineState.h"


namespace MotionBlur
{
	bool IsEnable = false;

	ComputePSO s_CameraMotionBlurPrePassCS[2] = { {L"Camera Motion Blur PrePass CS"}, { L"Camera Motion Blur PrePass Linear Z CS" } };
	ComputePSO s_MotionBlurPrePassCS(L"Motion Blur PrePass CS");
	ComputePSO s_MotionBlurFinalPassCS(L"Motion Blur Final Pass CS");
	ComputePSO s_CameraVelocityCS[2] = { { L"Camera Velocity CS" },{ L"Camera Velocity Linear Z CS" } };

};


void MotionBlur::Initialize(void)
{

}

void MotionBlur::Shutdown(void)
{

}