#include "EngineLog.h"
#include "EngineUtility.h"

vector<wstring> EngineLog::DisplayLogs = {};
vector<wstring>  EngineLog::FilterDisplayLogs = {};
wstring EngineLog::OldFilter;
vector<wstring>  EngineLog::FilterLogs = {};

vector<wstring>& EngineLog::GetLogs()
{
	return DisplayLogs;
}

void EngineLog::ClearLogs()
{
	DisplayLogs.clear();
	FilterLogs.clear();
	FilterDisplayLogs.clear();
}

void EngineLog::AddLog(const wchar_t* format, ...)
{
	wchar_t buffer[256];
	va_list ap;
	va_start(ap, format);
	vswprintf(buffer, 256, format, ap);
	wstring log(buffer);

	DisplayLogs.push_back(log);
	//统一改为小写字符
	for (wstring::iterator it = log.begin(); it != log.end(); ++it)
	{
		*it =tolower(*it);
	}
	FilterLogs.push_back(log);
}

void EngineLog::Filter(wstring Filter)
{
	if (Filter == OldFilter || Filter == L"")
		return;

	//统一改为小写字符
	for (wstring::iterator it = Filter.begin(); it != Filter.end(); ++it)
	{
		*it = tolower(*it);
	}

	int Position = 0;
	FilterDisplayLogs.clear();
	for (int i=0;i<FilterLogs.size();++i)
	{
		Position= (int)FilterLogs[i].find(Filter);
		if (Position != FilterLogs[i].npos)
		{
			FilterDisplayLogs.push_back(DisplayLogs[i]);
		}
	}
}

void EngineLog::SaveLog()
{
	wchar_t filePath[MAX_PATH + 1] = { 0 };
	GetModuleFileName(NULL, filePath, MAX_PATH);
	wstring OutputPath = GetBasePath(filePath);
	wofstream outfile;
	outfile.open(OutputPath + L"EngineLog.txt");
	for (auto &log: DisplayLogs)
	{
		outfile << log << endl;
	}
	outfile.close();
}

vector<wstring>& EngineLog::GetFilterLogs()
{
	return FilterDisplayLogs;
}