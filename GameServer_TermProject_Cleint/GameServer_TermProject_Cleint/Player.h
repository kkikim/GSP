#pragma once
#include "Define.h"
#include "texture.h"
#include <glut.h>
#include <Windows.h>
class CPlayer {
public:
	CPlayer();

	GLvoid DrawPlayer();
	GLvoid DrawPosition();
	GLvoid DrawAttack();

	GLint GetX();
	GLint GetY();

	GLvoid SetX(GLint _x);
	GLvoid SetY(GLint _y);
	
	GLint GetID();
	GLvoid SetID(GLint _id);

	GLvoid SetCurrentHealth(GLint);
	GLint GetCurrentHealth();

	GLvoid SetMaxHealth(GLint);
	GLint GetMaxHealth();

	GLvoid SetPlayerLevel(GLint);
	GLint  GetPlayerLevel();

	GLvoid SetPlayerZone(GLint);
	GLint  GetPlayerZone();

	GLvoid SetPlayerPos(GLint, GLint);
	Vector2 GetPlayerPos();

	GLvoid SetPlayerAttack(GLint);
	GLint  GetPlayerAttack();

	char message[256];
	DWORD message_time;

	GLboolean attacking;
	DWORD mT_AttackStart;
	DWORD mT_AttackEnd;
private:
	GLint mX;
	GLint mY;
	GLint ID;

	GLint playerLevel;

	GLint currentHealth;			//일단 임의로 지정
	GLint MaxHealth;

	GLint attack;			//공격력
	int Character_Zone;
};