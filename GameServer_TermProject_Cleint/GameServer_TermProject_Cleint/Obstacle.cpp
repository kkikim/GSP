#include "Obstacle.h"

GLint CObstacle::GetX() { return mX; }
GLint CObstacle::GetY() { return mY; }

GLvoid CObstacle::SetX(GLint _x) { mX = _x; }
GLvoid CObstacle::SetY(GLint _y) { mY = _y; }
GLvoid CObstacle::SetObstaclePos(GLint _x, GLint _y) { mX = _x; mY = _y; }

GLvoid CObstacle::DrawObstacle()
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, text_ob[0]);
	glBegin(GL_QUADS);
	{
		glColor4f(0.0, 0.0, 0.0, 1.0);

		glTexCoord2d(0.0, 1.0);
		glVertex3f(mX - 0.5, mY + 0.5, 0);

		glTexCoord2d(0.0, 0.0);
		glVertex3f(mX - 0.5, mY - 0.5, 0);

		glTexCoord2d(1.0, 0.0);
		glVertex3f(mX + 0.5, mY - 0.5, 0);

		glTexCoord2d(1.0, 1.0);
		glVertex3f(mX + 0.5, mY + 0.5, 0);
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);
}
