#include "CNPC.h"
CNPC::CNPC() { npcVisible = false; }
GLvoid CNPC::DrawNPC()
{
	//glEnable(GL_TEXTURE_2D);
	glBegin(GL_POLYGON);
	{
		glColor4f(0.0, 1.0, 0.0, 1.0f);

	//	glTexCoord2d(0.0, 1.0);
		glVertex3f(mX , mY +0.3, 0);

		//glTexCoord2d(0.0, 0.0);
		glVertex3f(mX - 0.3, mY, 0);

		//glTexCoord2d(1.0, 0.0);
		glVertex3f(mX -0.15, mY - 0.3, 0);

		//glTexCoord2d(1.0, 1.0);
		glVertex3f(mX + 0.15, mY - 0.3, 0);

		//glTexCoord2d(0.0, 0.0);
		glVertex3f(mX + 0.3, mY , 0);

	}
	glEnd();

	//glDisable(GL_TEXTURE_2D);
}
GLint CNPC::GetX() { return mX; }
GLint CNPC::GetY() { return mY; }

GLvoid CNPC::SetX(GLfloat _x) { mX = _x; }
GLvoid CNPC::SetY(GLfloat _y) { mY = _y; }

GLint CNPC::GetID() { return ID; }
GLvoid CNPC::SetID(GLint _id) { ID = _id; };

GLvoid CNPC::SetZone(GLint _playerZone) { Character_Zone = _playerZone; };
GLint  CNPC::GetZone() { return Character_Zone; }

GLvoid CNPC::DrawHello()
{
	//ÇÐ¹ø
	glColor4f(0.99f, 0.4f, 0.2f, 1.0f);

	char  string2[10];
	wsprintf(string2, "%s", message);
	//sprintf(string2, "%s", message);
	glRasterPos2f(mX - 1.2, mY - 1.2);
	int len = (int)strlen(string2);
	for (int i = 0; i < len; i++)
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, string2[i]);
}