#include "Game.h"

CGame* CGame::instance = nullptr;

CGame * CGame::GetInstance()
{
	if (instance == nullptr)
		instance = new CGame();

	return instance;
}
void CGame::Run()
{
	glutMainLoop();
}
void CGame::drawScene()
{
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glFrontFace(GL_CCW);			//반시계방향
	glLineWidth(3.0f);

	//glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
	glLoadIdentity();
	//gluLookAt(player.GetX(), player.GetY(), 20, player.GetX(), player.GetY(), -1.0, 0.0, 1.0, 0.0);

	glutSwapBuffers(); // 결과 출력
}
void CGame::Reshape(int w, int h)
{
	/*glMatrixMode(GL_PROJECTION);
	

	gluPerspective(60.0, w / h, 1.0, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glViewport(0, 0, w, h);
	glutPostRedisplay();*/
	glLoadIdentity();
}

void CGame::Init()
{
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(700, 700);
	glutCreateWindow("2011182004 김기남");
	glutDisplayFunc(drawScene);
	glutReshapeFunc(Reshape);
	////textureimage();
	//glutMouseFunc(Mouse);
	//glutSpecialFunc(SpecialKeyboard);
	//glutKeyboardFunc(Keyboard);
	//glutTimerFunc(10, TimerFunction, 1);
}
