#pragma once
#include <vector>
#include <string>
using namespace std;
class EngineLog
{
public:
	EngineLog() {};
	~EngineLog() {};

	static vector<wstring>& GetLogs();
	static vector<wstring>& GetFilterLogs();
	static void ClearLogs();
	static void AddLog(const wstring log);
	static void Filter(wstring Filter);
private:
	static vector<wstring> DisplayLogs;
	static vector<wstring> FilterLogs;
	static vector<wstring> FilterDisplayLogs;
	static wstring OldFilter;
};

