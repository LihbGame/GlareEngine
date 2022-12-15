#pragma once
#include <cstdint>
#include <string>

namespace GlareEngine
{
    enum TextureConversionFlags
    {
        eSRGB = 1,          // Texture contains sRGB colors
        ePreserveAlpha = 2, // Keep four channels
        eNormalMap = 4,     // Texture contains normals
        eBumpToNormal = 8,  // Generate a normal map from a bump map
        eDefaultBC = 16,    // Apply standard block compression (BC1-5)
        eQualityBC = 32,    // Apply quality block compression (BC6H/7)
        eFlipVertical = 64,
    };

    inline uint8_t TextureOptions(bool sRGB, bool hasAlpha = false, bool invertY = false)
    {
        return (sRGB ? eSRGB : 0) | (hasAlpha ? ePreserveAlpha : 0) | (invertY ? eFlipVertical : 0) | eDefaultBC;
    }

    // If the DDS version of the texture specified does not exist or is older than the source texture, reconvert it.
    void CompileDDSTexture(const std::wstring& originalFile, uint32_t flags);

    // Loads a NON-DDS texture such as TGA, PNG, or JPG, then converts it to a more optimal
    // DDS format with a full mipmap chain. Result file has the same path with the file extension changed to "DDS".
    bool ConvertToDDS(
        const std::wstring& filePath,	// Source file path
        uint32_t Flags                  // Texture Conversion Flags
    );


}

