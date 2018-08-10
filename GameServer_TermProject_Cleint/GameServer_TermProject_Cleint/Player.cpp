#include "Player.h"
#include "..\..\GameSever_TermProject\GameSever_TermProject\protocol.h"

CPlayer ::CPlayer()
{
	currentHealth = 100;			//일단 임의로 지정
	MaxHealth = 100;
	playerLevel = 1;
	Character_Zone = CharZone::TOWN;
	attacking = false;
}

GLint CPlayer::GetX() { return mX; }
GLint CPlayer::GetY() { return mY; }

GLvoid CPlayer::SetX(GLint _x) { mX = _x; }
GLvoid CPlayer::SetY(GLint _y) { mY = _y; }
GLvoid CPlayer::SetPlayerPos(GLint _x, GLint _y) { mX = _x; mY = _y; }

GLint CPlayer::GetID() { return ID; }
GLvoid CPlayer::SetID(GLint _id) { ID = _id; };

GLvoid CPlayer::SetCurrentHealth(GLint _health) { currentHealth = _health; };
GLint CPlayer::GetCurrentHealth() { return currentHealth; };

GLvoid CPlayer::SetMaxHealth(GLint _MaxHealth) { MaxHealth = _MaxHealth; };
GLint CPlayer::GetMaxHealth() { return MaxHealth; };

GLvoid CPlayer::SetPlayerLevel(GLint _playerLevel) { playerLevel = _playerLevel; }
GLint  CPlayer::GetPlayerLevel() { return playerLevel; }

GLvoid CPlayer::SetPlayerZone(GLint _playerZone) { Character_Zone = _playerZone; };
GLint  CPlayer::GetPlayerZone() { return Character_Zone; }

GLvoid CPlayer::SetPlayerAttack(GLint _attack) { attack = _attack; };
GLint  CPlayer::GetPlayerAttack() { return attack; }


Vector2 CPlayer::GetPlayerPos() {
	Vector2 playerPos{ mX,mY };
	return playerPos;
}

GLvoid CPlayer::DrawPlayer()
{
	//glEnable(GL_TEXTURE_2D);
	//glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[1]);
	glBegin(GL_QUADS);
	{
		glColor4f(1.0, 1.0, 1.0, 0.0);

		//glTexCoord2d(0.0, 1.0);
		glVertex3f(mX - 0.3, mY + 0.3, 0);

		//glTexCoord2d(0.0, 0.0);
		glVertex3f(mX - 0.3, mY - 0.3, 0);

		//glTexCoord2d(1.0, 0.0);
		glVertex3f(mX + 0.3, mY - 0.3, 0);

		//glTexCoord2d(1.0, 1.0);
		glVertex3f(mX + 0.3, mY + 0.3, 0);
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);
}
GLvoid CPlayer::DrawPosition()
{
	//학번
	glColor4f(0.4f, 0.4f, 0.9f, 1.0f);
	char  string2[20];
	//wsprintf(string2, " ME");
	wsprintf(string2, "(%d,%d) \n", mX, abs(mY));
	glRasterPos2f(mX - 1.2, mY - 1.2);
	int len = (int)strlen(string2);
	for (int i = 0; i < len; i++)
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, string2[i]);
}
GLvoid CPlayer::DrawAttack()
{
	glLineWidth(3.0);
	glBegin(GL_LINES);
	{
		glColor3f(1.0f, 0.0f, 0.0f);
		//Left
		glVertex3f(mX - 0.4, mY + 0.2, 1.0f);
		glVertex3f(mX - 0.8, mY - 0.2, 1.0f);

		glVertex3f(mX - 0.8, mY + 0.2, 1.0f);
		glVertex3f(mX - 0.4, mY - 0.2, 1.0f);

		//Top
		glVertex3f(mX + 0.2, mY + 0.9, 1.0f);
		glVertex3f(mX - 0.2, mY + 0.4, 1.0f);

		glVertex3f(mX + 0.2, mY + 0.4, 1.0f);
		glVertex3f(mX - 0.2, mY + 0.9, 1.0f);

		//Right
		glVertex3f(mX + 0.9, mY + 0.2, 1.0f);
		glVertex3f(mX + 0.4, mY - 0.2, 1.0f);

		glVertex3f(mX + 0.4, mY + 0.2, 1.0f);
		glVertex3f(mX + 0.9, mY - 0.2, 1.0f);


		//Bottom
		glVertex3f(mX + 0.2, mY - 0.4, 1.0f);
		glVertex3f(mX - 0.2, mY - 0.9, 1.0f);

		glVertex3f(mX + 0.2, mY - 0.9, 1.0f);
		glVertex3f(mX - 0.2, mY - 0.4, 1.0f);

	}glEnd();
}