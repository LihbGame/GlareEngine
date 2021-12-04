#pragma once
// Preview:
// 1.Baking Environment Diffuse reflection
// 2.Baking Environment Specular reflection

class IBL
{
public:
	IBL();
	~IBL();

	static void BakingEnvironmentDiffuse();
	static void BakingEnvironmentSpecular();
	static void PreBakeGIData();
private:
};

