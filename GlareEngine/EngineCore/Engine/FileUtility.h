#pragma once
#include "EngineUtility.h"
#include <vector>
#include <string>
#include <ppl.h>

namespace GlareEngine
{
	using namespace concurrency;

	extern ByteArray NullFile;


	class FileUtility
	{
	public:
		// Reads the entire contents of a binary file.
		// This operation blocks until the entire file is read.
		static ByteArray ReadFileSync(const std::wstring& fileName);

		// Same as previous except that it does not block but instead returns a task.
		static task<ByteArray> ReadFileAsync(const std::wstring& fileName);

	};



}




