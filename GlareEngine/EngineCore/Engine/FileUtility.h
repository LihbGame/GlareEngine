#pragma once
#include "EngineUtility.h"
#include <vector>
#include <string>
#include <ppl.h>

namespace GlareEngine
{
	using namespace std;
	using namespace concurrency;

	extern ByteArray NullFile;

	enum class EFileMode
	{
		Text,
		Binary
	};

	class FileUtility
	{
	public:
		// Reads the entire contents of a binary file.
		// This operation blocks until the entire file is read.
		static ByteArray ReadFileSync(const wstring& fileName);

		// Same as previous except that it does not block but instead returns a task.
		static task<ByteArray> ReadFileAsync(const wstring& fileName);

		static bool IsFileExists(const char* filePath);

		static size_t GetFileSize(const char* filePath);

		static std::time_t GetFileLastWriteTime(const char* filePath);

		static ByteArray ReadFile(const char* filePath, EFileMode mode);
	};



}




