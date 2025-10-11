#pragma once

class DLSS
{
public:
	static DLSS* GetInstance();
	static void Shutdown();


private:
	DLSS() {}
	~DLSS();

private:
	static DLSS* m_pDLSSInstance;

};

