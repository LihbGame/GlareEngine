#pragma once
#include "L3DUtil.h"

class Grass
{
public:
	Grass();
	~Grass();
	void BuildGrassVB(ID3D11Device* device);
	void Init(ID3D11Device* device, float GrassWidth, float GrassDepth, int VertRows, int VertCols, ID3D11ShaderResourceView* RandomTexSRV, ID3D11ShaderResourceView* HeightMapSRV);
	void Draw(ID3D11DeviceContext* dc, const Camera& cam, DirectionalLight* lights);
private:
	ID3D11Buffer* mGrassVB;
	ID3D11ShaderResourceView* mGrassTexSRV;
	ID3D11ShaderResourceView* mGrassBlendTexSRV;
	ID3D11ShaderResourceView* mRandomSRV;
	ID3D11ShaderResourceView* mHeightMapSRV;
	int mNumVertRows;
	int mNumVertCols;
	float mGrassWidth;
	float mGrassDepth;
	float mGameTime;
	Material mGrassMat;
};

