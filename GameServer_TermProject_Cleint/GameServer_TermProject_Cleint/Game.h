#pragma once
#pragma comment(lib, "ws2_32")
//#include "..\..\GameSever_TermProject\GameSever_TermProject\protocol.h"
#include <gl\glut.h>
#include <WinSock2.h>
class CGame
{
public:
	static CGame * GetInstance();
	void Run();
	void Init();

	static void drawScene();
	static void Reshape(int, int);
private:
	static CGame* instance;
};

