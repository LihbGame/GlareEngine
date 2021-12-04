#pragma once
#include "EngineUtility.h"
class CubeRenderTarget
{
public:
	CubeRenderTarget(UINT width, UINT height,DXGI_FORMAT format);
	~CubeRenderTarget();
private:
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat;
};

