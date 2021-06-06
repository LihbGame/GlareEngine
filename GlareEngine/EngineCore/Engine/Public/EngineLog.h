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
	static void ClearLogs();
	static void AddLog(const wstring log);

private:
	static vector<wstring> Logs;
};

