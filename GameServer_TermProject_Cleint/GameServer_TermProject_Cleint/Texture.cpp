#include "texture.h"

CTexture* CTexture::inst = nullptr;
CTexture::CTexture(){}
CTexture::~CTexture(){}
CTexture* CTexture::GetInstance()
{
	if (inst == nullptr)
		inst = new CTexture();

	return inst;
}
int CTexture::GetTexture(int num) { return mTexture_Object[num]; }
