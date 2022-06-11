#include "TextureConvert.h"
#include "EngineUtility.h"
#include "DirectXTex.h"
#include "EngineLog.h"

#define GetFlag(f) ((Flags & f) != 0)

void CompileTextureOnDemand(const std::wstring& originalFile, uint32_t flags)
{
	std::wstring ddsFile = RemoveExtension(originalFile) + L".dds";

	struct _stat64 ddsFileStat, srcFileStat;

	bool srcFileMissing = _wstat64(originalFile.c_str(), &srcFileStat) == -1;
	bool ddsFileMissing = _wstat64(ddsFile.c_str(), &ddsFileStat) == -1;

	if (srcFileMissing && ddsFileMissing)
	{
		EngineLog::AddLog(L"Texture %ws is missing.\n", RemoveBasePath(originalFile).c_str());
		return;
	}

	// If we can find the source texture and the DDS file is older, reconvert.
	if (ddsFileMissing || !srcFileMissing && ddsFileStat.st_mtime < srcFileStat.st_mtime)
	{
		EngineLog::AddLog(L"DDS texture %ws missing or older than source.  Rebuilding.\n", RemoveBasePath(originalFile).c_str());
		//ConvertToDDS(originalFile, flags);
	}
}