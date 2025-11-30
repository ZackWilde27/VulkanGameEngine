#pragma once

#include "../engineSettings.h"
#include "../../packages/Microsoft.DXSDK.D3DX.9.29.952.8/build/native/include/d3dx9.h"
#include <d3d9.h>
#include <vector>

struct Mexel
{
	LPD3DXMESH mesh;
};

class Mesh
{
	std::vector<Mexel> mexels;

public:
	Mesh(CHAR_T* filename);
	~Mesh();

	void Draw(LPDIRECT3DDEVICE9 device);
	static Mesh* LoadMesh(CHAR_T* filename);
};