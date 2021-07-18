#pragma once
#include "EngineUtility.h"
namespace GlareEngine
{
	class TextureManager
	{
	public:
		TextureManager() {}
		~TextureManager() {}

		void Initialize();
		void CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB = true);
		std::unique_ptr<Texture>& GetTexture(std::wstring name, bool ForceSRGB = true);
	private:

	};

}