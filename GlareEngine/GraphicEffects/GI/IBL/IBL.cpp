#include "IBL.h"



IBL::IBL()
{
}

IBL::~IBL()
{
}


void IBL::Initialize()
{
	mIndirectDiffuseCube = make_unique<CubeRenderTarget>(BAKECUBESIZE, BAKECUBESIZE);

}

void IBL::BakingEnvironmentDiffuse()
{
	assert(m_pSky);


}

void IBL::BakingEnvironmentSpecular()
{
}

void IBL::PreBakeGIData(RenderObject* Object)
{
	m_pSky = Object;
	BakingEnvironmentDiffuse();
	BakingEnvironmentSpecular();
}




