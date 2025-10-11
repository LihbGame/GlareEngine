#include "DLSS.h"


DLSS* DLSS::m_pDLSSInstance = nullptr;


DLSS* DLSS::GetInstance()
{
	if (m_pDLSSInstance == nullptr)
	{
		m_pDLSSInstance = new DLSS();
	}
	return m_pDLSSInstance;
}

void DLSS::Shutdown()
{
	if (m_pDLSSInstance)
	{
		delete m_pDLSSInstance;
		m_pDLSSInstance = nullptr;
	}
}

DLSS::~DLSS()
{

}
