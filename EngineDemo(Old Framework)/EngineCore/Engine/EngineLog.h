#pragma once

#include <vector>
#include <xstring>
#include <stdarg.h>

class EngineLog
{
public:
	EngineLog() {};
	~EngineLog() {};

	static std::vector<std::wstring>& GetLogs();
	static std::vector<std::wstring>& GetFilterLogs();
	static void ClearLogs();
	static void AddLog(const wchar_t* format, ...);
	static void Filter(std::wstring Filter);
	static void SaveLog();
private:
	static std::vector<std::wstring> DisplayLogs;
	static std::vector<std::wstring> FilterLogs;
	static std::vector<std::wstring> FilterDisplayLogs;
	static std::wstring OldFilter;
};

