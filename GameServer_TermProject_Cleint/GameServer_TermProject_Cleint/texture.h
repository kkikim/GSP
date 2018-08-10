#pragma once
#include <glut.h>
class CTexture
{
public:
	CTexture();
	~CTexture();
	static CTexture* GetInstance();
	int GetTexture(int);
	GLuint mTexture_Object[9];
private:
	static CTexture* inst;
	
};

