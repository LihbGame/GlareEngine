#include "EngineLog.h"
#include "EngineUtility.h"

std::vector<std::wstring> EngineLog::DisplayLogs = {};
std::vector<std::wstring> EngineLog::FilterDisplayLogs = {};
std::wstring EngineLog::OldFilter;
std::vector<std::wstring> EngineLog::FilterLogs = {};

std::vector<std::wstring>& EngineLog::GetLogs()
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
	std::wstring log(buffer);

	DisplayLogs.push_back(log);
	//统一改为小写字符
	for (std::wstring::iterator it = log.begin(); it != log.end(); ++it)
	{
		*it =tolower(*it);
	}
	FilterLogs.push_back(log);
}

void EngineLog::Filter(std::wstring Filter)
{
	if (Filter == OldFilter || Filter == L"")
		return;

	//统一改为小写字符
	for (std::wstring::iterator it = Filter.begin(); it != Filter.end(); ++it)
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
	std::wstring OutputPath = GetBasePath(filePath);
	std::wofstream outfile;
	outfile.open(OutputPath + L"EngineLog.txt");
	for (auto &log: DisplayLogs)
	{
		outfile << log << std::endl;
	}
	outfile.close();
}

std::vector<std::wstring>& EngineLog::GetFilterLogs()
{
	return FilterDisplayLogs;
}