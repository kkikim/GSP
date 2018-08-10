#pragma once
#include <glut.h>
#include <Windows.h>
class CObstacle {
public:
	GLvoid DrawObstacle();

	GLint GetX();
	GLint GetY();

	GLvoid SetX(GLint _x);
	GLvoid SetY(GLint _y);

	GLvoid SetObstaclePos(GLint, GLint);

	WCHAR message[256];
	DWORD message_time;

private:
	GLint mX;
	GLint mY;

	int text_ob[5];
};