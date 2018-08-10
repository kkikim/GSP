#pragma once
#include <glut.h>
#include <Windows.h>
#include <vector>
class CUI
{
public:
	CUI() {};
	~CUI() {};
	static CUI* GetInstance();
	GLvoid DrawUI(float, float,int, int);
private:
	static CUI* inst;
	GLfloat uiX1, uiY1;
};

