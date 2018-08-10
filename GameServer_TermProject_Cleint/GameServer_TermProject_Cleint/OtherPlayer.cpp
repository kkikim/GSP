#include "OtherPlayer.h"

GLint COtherPlayer::GetX() { return mX; }
GLint COtherPlayer::GetY() { return mY; }

GLvoid COtherPlayer::SetX(GLfloat _x) { mX = _x; }
GLvoid COtherPlayer::SetY(GLfloat _y) { mY = _y; }

GLint COtherPlayer::GetID() { return ID; }
GLvoid COtherPlayer::SetID(GLint _id) { ID = _id; };

GLvoid COtherPlayer::DrawPlayer()
{
	//glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	{

		glColor4f(0.0, 0.0, 1.0, 1.0f);

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
	//glDisable(GL_TEXTURE_2D);
}