#pragma once
#include <glut.h>
class CTown
{
public:
	CTown();
	~CTown();
	void DrawTown()
	{
		glBegin(GL_QUADS);
		{
			glColor3f(0.0f,0.5f,0.0f);
			glVertex3f(-50.0f,50.0f,1.0f);
			glVertex3f(50.0f, 50.0f, 1.0f);
			glVertex3f(50.0f, -50.0f, 1.0f);
			glVertex3f(-50.0f, -50.0f, 1.0f);
		}glEnd();
	}
};

