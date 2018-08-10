#pragma once
#include <glut.h>
#include <Windows.h>
class CNPC
{
public:
	CNPC();
	GLvoid DrawNPC();

	GLint GetX();
	GLint GetY();

	GLvoid SetX(GLfloat _x);
	GLvoid SetY(GLfloat _y);

	GLint GetID();
	GLvoid SetID(GLint _id);

	GLvoid SetZone(GLint);
	GLint GetZone();
	GLvoid DrawHello();


	GLboolean npcVisible;

	char message[256];
	DWORD message_time;
	bool hello;
	int text_ob[6];
private:
	GLint mX;
	GLint mY;
	GLint ID;
	int Character_Zone;
};