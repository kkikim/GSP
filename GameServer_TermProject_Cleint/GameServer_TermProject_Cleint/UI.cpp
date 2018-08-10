#include "UI.h"

CUI* CUI::inst = nullptr;

CUI* CUI::GetInstance()
{
	if (inst == nullptr)
		inst = new CUI();

	return inst;
}

GLvoid CUI::DrawUI(float currentHealth, float maxHealth, int playerLevel ,int playerAttack)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.0, 1.0, 1.0, 0.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glPushMatrix();
	//TODD : 이곳에 UI 데이터
	//HP Bar Square
	glColor3f(0.0f, 0.0f, 0.0f);
	glBegin(GL_LINE_LOOP);
	{
		glLineWidth(1.0f);
		glVertex2f(0.05f,0.05f);
		glVertex2f(0.45f,0.05f);
		glVertex2f(0.45f,0.15f);
		glVertex2f(0.05f,0.15f);
	}glEnd();
	glPopMatrix();

	//HP Bar Background
	glBegin(GL_QUADS);
	{
		glColor3f(1.0f, 1.0f, 1.0f);
		glVertex2f(0.05f, 0.05f);
		glVertex2f(0.45f, 0.05f);
		glVertex2f(0.45f, 0.15f);
		glVertex2f(0.05f, 0.15f);
	}glEnd();

	//HP Bar
	maxHealth = playerLevel*100;
	float percentage = (currentHealth / maxHealth) * 100;
	float hpBar = (percentage *0.4f) / 100;
	glBegin(GL_QUADS);
	{
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex2f(0.05f, 0.05f);
		glVertex2f(hpBar + 0.05f, 0.05f);
		glVertex2f(hpBar + 0.05f , 0.15f);
		glVertex2f(0.05f, 0.15f);
	}glEnd();

	//Character Level String
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
	char  levelString[15];
	wsprintf(levelString, " Level : %d", playerLevel);
	glRasterPos2f(0.05f,0.20f);
	int len = (int)strlen(levelString);
	for (int i = 0; i < len; i++)
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, levelString[i]);

	//character attack string
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
	char  attackstring[15];
	wsprintf(attackstring, " Attack : %d", playerAttack);
	glRasterPos2f(0.05f, 0.25f);
	int len2 = (int)strlen(attackstring);
	for (int i = 0; i < len2; i++)
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, attackstring[i]);



	//Chatting Box
	glEnable(GL_BLEND);
	{
		glBlendFunc(GL_ONE,GL_ZERO);
		glBegin(GL_POLYGON);
		{
			glColor4f(0.9f,0.9f,0.9f,0.33333f);
			glVertex2f(0.05f, 0.75f);
			glVertex2f(0.35f, 0.75f);
			glVertex2f(0.35f, 0.95f);
			glVertex2f(0.05f, 0.95f);
		}glEnd();

		glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

		char aaa[10] = "aaa";
		char bbb[10] = "bbb";
		char ccc[10] = "ccc";
		char ddd[10] = "aaa";
		char eee[10] = "bbb";
		char fff[10] = "ccc";
		char chatString[15];

		std::vector<char*> aa;
		aa.push_back(aaa);
		aa.push_back(bbb);
		aa.push_back(ccc);
		aa.push_back(ddd);
		aa.push_back(eee);
		aa.push_back(fff);
		aa.push_back(fff);
		aa.push_back(fff);
		aa.push_back(fff);
		aa.push_back(fff);
		aa.push_back(fff);
		aa.push_back(fff);
		float yHeight = 0.0;
		for (int i = 0; i < aa.size(); ++i)
		{
			wsprintf(chatString, aa[i]);
			glRasterPos2f(0.05f, 0.76f + yHeight);
			int len = (int)strlen(chatString);
			for (int i = 0; i < len; ++i)
				glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, chatString[i]);

			yHeight += 0.017f;
		}
		
	}


	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}
