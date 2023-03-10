#pragma once
#include <vector>
#include <string>
#include <stdarg.h>
using namespace std;
class EngineLog
{
public:
	EngineLog() {};
	~EngineLog() {};

	static vector<wstring>& GetLogs();
	static vector<wstring>& GetFilterLogs();
	static void ClearLogs();
	static int AddLog(const wchar_t* format, ...);
	static void ReplaceLog(int location, wchar_t* format, ...);
	static void Filter(wstring Filter);
	static void SaveLog();
private:
	static vector<wstring> DisplayLogs;
	static vector<wstring> FilterLogs;
	static vector<wstring> FilterDisplayLogs;
	static wstring OldFilter;
};

