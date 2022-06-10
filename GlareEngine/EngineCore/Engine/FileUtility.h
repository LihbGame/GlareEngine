#pragma once
#include "EngineUtility.h"
#include <vector>
#include <string>
#include <ppl.h>

namespace GlareEngine
{
	using namespace std;
	using namespace concurrency;

	typedef shared_ptr<vector<byte> > ByteArray;
	extern ByteArray NullFile;


	class FileUtility
	{
	public:
		// Reads the entire contents of a binary file.
		// This operation blocks until the entire file is read.
		static ByteArray ReadFileSync(const wstring& fileName);

		// Same as previous except that it does not block but instead returns a task.
		static task<ByteArray> ReadFileAsync(const wstring& fileName);

	};



}




