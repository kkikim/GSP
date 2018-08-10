#pragma once
#include <glut.h>
#include <Windows.h>
class COtherPlayer {
public:
	COtherPlayer() { connecting = false; }
	GLvoid DrawPlayer();

	GLint GetX();
	GLint GetY();

	GLvoid SetX(GLfloat _x);
	GLvoid SetY(GLfloat _y);

	GLint GetID();
	GLvoid SetID(GLint _id);

	GLboolean connecting;

	char message[256];
	DWORD message_time;

	int text_ob[5];
private:
	GLint mX;
	GLint mY;
	GLint ID;
};