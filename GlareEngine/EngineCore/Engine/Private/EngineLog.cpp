#include "EngineLog.h"

vector<wstring> EngineLog::Logs = {};

vector<wstring>& EngineLog::GetLogs()
{
	return Logs;
}

void EngineLog::ClearLogs()
{
	Logs.clear();
}

void EngineLog::AddLog(const wstring log)
{
	Logs.push_back(log);
}
