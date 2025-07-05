#include "FSR.h"
#include "Engine/EngineLog.h"
#include "Graphics/CommandContext.h"



FSR::~FSR()
{
}

void FSR::DrawUI()
{



}

void FSR::FSRMsgCallback(uint32_t type, const wchar_t* message)
{
	if (type == FFX_API_MESSAGE_TYPE_ERROR)
	{
		EngineLog::AddLog(L"FSR_API_DEBUG_ERROR: %ls", message);
	}
	else if (type == FFX_API_MESSAGE_TYPE_WARNING)
	{
		EngineLog::AddLog(L"FSR_API_DEBUG_WARNING: %ls", message);
	}
}
