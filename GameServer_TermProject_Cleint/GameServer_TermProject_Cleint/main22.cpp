#pragma comment(lib, "ws2_32")
#include <gl/glut.h>
#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <windowsx.h>
#include <cstring>
#include "..\..\GameSever_TermProject\GameSever_TermProject\protocol.h"
using namespace std;
//#define IPADDRESS

#define DISTANCE		75.0f
#define SERVERPORT 7000
#define BUF_SIZE 1024
#define SERVERIP "127.0.0.1"
#define BOARDNUM 200
//---------전역 변수--------
WSAEVENT hEvent;
GLuint text_ob[6];
GLubyte *pBytes;
BITMAPINFO *info;
BITMAPINFO *info2;
SOCKET clientsocket;
WSANETWORKEVENTS NetworkEvents;
int networkEvent;

WSABUF	send_wsabuf;
char 	send_buffer[BUF_SIZE];
WSABUF	recv_wsabuf;
char	recv_buffer[BUF_SIZE];
char	packet_buffer[BUF_SIZE];

DWORD	in_packet_size = 0;
int		saved_packet_size = 0;
int		g_myid;

class CBoard
{
public:
	GLvoid DrawBoardLine()
	{
		//보드판 겉테두리
		glLineWidth(3.0f);
		glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		glBegin(GL_LINE_LOOP);
		{
			glVertex3f(-0.5f, 0.5f, 0.0f);
			glVertex3f(-0.5f, -399.5f, 0.0f);
			glVertex3f(399.5f, -399.5f, 0.0f);
			glVertex3f(399.5f, 0.5f, 0.0f);
		}glEnd();

		//glRectf(10.0f,10.0f,20.0f,20.0f);
	}
	GLvoid DrawBoard()
	{
		//누리끼리한색
		glColor4f(0.97f, 0.95f, 0.92f, 1.0f);
		glRectf(-10.0f, 10.0f, 410.0f, -410.0f);
	}
	GLvoid DrawBoard2()
	{
		//Grey
		glColor4f(0.40f, 0.19f, 0.f, 1.0f);

		//홀수라인
		GLfloat x = -0.5; GLfloat y = 0.5;
		GLfloat x2 = 0.5; GLfloat y2 = -0.5;
		for (int i = 0; i < BOARDNUM; ++i)
		{
			for (int j = 0; j < BOARDNUM; ++j)
			{
				glRectf(x, y, x2, y2);
				x += 2;  x2 += 2;
			}
			x = -0.5; y -= 2;
			x2 = 0.5; y2 -= 2;
		}
		//짞쑤라인
		x = 0.5;  y = -0.5;
		x2 = 1.5;  y2 = -1.5;
		for (int i = 0; i < BOARDNUM; ++i)
		{
			for (int j = 0; j < BOARDNUM; ++j)
			{
				glRectf(x, y, x2, y2);
				x += 2;  x2 += 2;
			}
			x = 0.5; y -= 2;
			x2 = 1.5; y2 -= 2;
		}
	}
	GLvoid DrawBoard3()//간격표시자
	{

		glLineWidth(4.0f);
		glColor4f(0.1f, 0.9f, 0.23f, 1.0f);

		//세로줄
		for (int i = 1; i < 52; ++i)
		{
			glBegin(GL_LINES);
			{
				glVertex3f((8 * i) - 0.5, 0.5, 0.0f);
				glVertex3f((8 * i) - 0.5, -400.5, 0.0f);
			}glEnd();
		}
		//가로줄
		for (int i = 1; i < 52; ++i)
		{
			glBegin(GL_LINES);
			{
				glVertex3f(-0.5, -(8 * i) + 0.5, 0.0f);
				glVertex3f(400.5, -(8 * i) + 0.5, 0.0f);
			}glEnd();
		}
	}
private:
};
class CPlayer {
public:
	CPlayer() {}

	GLvoid DrawPlayer()
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, text_ob[0]);
		glBegin(GL_QUADS);
		{

			glColor4f(1.0, 1.0, 1.0, 1.0);

			glTexCoord2d(0.0, 1.0);
			glVertex3f(mX - 0.3, mY + 0.3, 0);

			glTexCoord2d(0.0, 0.0);
			glVertex3f(mX - 0.3, mY - 0.3, 0);

			glTexCoord2d(1.0, 0.0);
			glVertex3f(mX + 0.3, mY - 0.3, 0);

			glTexCoord2d(1.0, 1.0);
			glVertex3f(mX + 0.3, mY + 0.3, 0);
		}
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}
	GLvoid DrawPosition()
	{
		//학번
		glColor4f(0.4f, 0.4f, 0.9f, 1.0f);
		/*char * string = "2011182004";
		glRasterPos2f(-320, 320);
		int len = (int)strlen(string);
		for (int i = 0; i < len; i++)
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, string[i]);
		*/
		char  string2[20];
		//wsprintf(string2, " ME");
		wsprintf(string2, "(%d,%d) \n", mX, abs(mY));
		//glRasterPos2f(100, 320); 
		glRasterPos2f(mX - 1.2, mY - 1.2);
		int len = (int)strlen(string2);
		for (int i = 0; i < len; i++)
			glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, string2[i]);


	}

	GLint GetX() { return mX; }
	GLint GetY() { return mY; }

	GLvoid SetX(GLint _x) { mX = _x; }
	GLvoid SetY(GLint _y) { mY = _y; }

	GLint GetID() { return ID; }
	GLvoid SetID(GLint _id) { ID = _id; };

	WCHAR message[256];
	DWORD message_time;

private:
	GLint mX;
	GLint mY;
	GLint ID;

};
class COtherPlayer {
public:
	COtherPlayer() { connecting = false; }
	GLvoid DrawPlayer()
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, text_ob[1]);
		glBegin(GL_QUADS);
		{

			glColor4f(1.0, 1.0, 1.0, 1.0f);

			glTexCoord2d(0.0, 1.0);
			glVertex3f(mX - 0.3, mY + 0.3, 0);

			glTexCoord2d(0.0, 0.0);
			glVertex3f(mX - 0.3, mY - 0.3, 0);

			glTexCoord2d(1.0, 0.0);
			glVertex3f(mX + 0.3, mY - 0.3, 0);

			glTexCoord2d(1.0, 1.0);
			glVertex3f(mX + 0.3, mY + 0.3, 0);
		}
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}

	GLint GetX() { return mX; }
	GLint GetY() { return mY; }

	GLvoid SetX(GLfloat _x) { mX = _x; }
	GLvoid SetY(GLfloat _y) { mY = _y; }

	GLint GetID() { return ID; }
	GLvoid SetID(GLint _id) { ID = _id; };

	GLboolean connecting;

	WCHAR message[256];
	DWORD message_time;
private:
	GLint mX;
	GLint mY;
	GLint ID;
};
class CNPC
{
public:
	CNPC() { npcVisible = false; }
	GLvoid DrawNPC()
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, text_ob[2]);
		glBegin(GL_QUADS);
		{
			glColor4f(1.0, 1.0, 1.0, 1.0f);

			glTexCoord2d(0.0, 1.0);
			glVertex3f(mX - 0.3, mY + 0.3, 0);

			glTexCoord2d(0.0, 0.0);
			glVertex3f(mX - 0.3, mY - 0.3, 0);

			glTexCoord2d(1.0, 0.0);
			glVertex3f(mX + 0.3, mY - 0.3, 0);

			glTexCoord2d(1.0, 1.0);
			glVertex3f(mX + 0.3, mY + 0.3, 0);
		}
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}

	GLint GetX() { return mX; }
	GLint GetY() { return mY; }

	GLvoid SetX(GLfloat _x) { mX = _x; }
	GLvoid SetY(GLfloat _y) { mY = _y; }

	GLint GetID() { return ID; }
	GLvoid SetID(GLint _id) { ID = _id; };

	GLboolean npcVisible;

	WCHAR message[256];
	DWORD message_time;
private:
	GLint mX;
	GLint mY;
	GLint ID;

};
CBoard			board;			//보드판
CPlayer			player;
COtherPlayer	otherPlayer[MAX_USER];
CNPC			NonPlayerCharacter[NUM_OF_NPC];
//---------함수 선언----------
GLvoid drawScene();
GLvoid Reshape(int w, int h);
GLvoid Mouse(int button, int state, int x, int y);
GLvoid TimerFunction(int value);
GLvoid Keyboard(unsigned char key, int x, int y);
GLvoid SpecialKeyboard(int key, int x, int y);
GLvoid textureimage();
GLubyte * LoadDIBitmap(const char *filename, BITMAPINFO **info);
GLvoid ProcessPacket(char* buf);
GLvoid ReadPacket(SOCKET sock);
GLvoid err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}
GLvoid Init()
{
	int retval;

	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	clientsocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);

	if (clientsocket == INVALID_SOCKET)
		err_quit("socket()");

	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(SERVERPORT);
#ifdef IPADDRESS
	ServerAddr.sin_addr.s_addr = inet_addr(SERVERIP);
#else
	char IPaddress[20];
	cout << "IP입력 : "; cin >> IPaddress;
	ServerAddr.sin_addr.s_addr = inet_addr(IPaddress);
#endif // DEBUG

	int Result = WSAConnect(clientsocket, (sockaddr *)&ServerAddr, sizeof(ServerAddr), NULL, NULL, NULL, NULL);
	if (retval == SOCKET_ERROR)
		err_quit("connect()");

	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = BUF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = BUF_SIZE;

	hEvent = WSACreateEvent();
	if (hEvent == WSA_INVALID_EVENT)
	{
		err_quit("hEVEnt");
	}
	::WSAEventSelect(clientsocket, hEvent, FD_CLOSE | FD_READ);

}
GLvoid main(int argc, char *argv[])
{
	Init();
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(700, 700);
	glutCreateWindow("2011182004 김기남");
	glutDisplayFunc(drawScene);
	glutReshapeFunc(Reshape);
	textureimage();
	glutMouseFunc(Mouse);
	glutSpecialFunc(SpecialKeyboard);
	glutKeyboardFunc(Keyboard);
	glutTimerFunc(10, TimerFunction, 1);

	glutMainLoop();

	closesocket(clientsocket);
	WSACleanup();
}
GLvoid drawScene()
{
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glFrontFace(GL_CCW);			//반시계방향
	glLineWidth(3.0f);

	//glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
	glLoadIdentity();
	gluLookAt(player.GetX(), player.GetY(), 20, player.GetX(), player.GetY(), -1.0, 0.0, 1.0, 0.0);

	networkEvent = ::WSAEnumNetworkEvents(clientsocket, hEvent, &NetworkEvents);

	if (networkEvent == SOCKET_ERROR)
		cout << "networkEvent Error \n";

	if (NetworkEvents.lNetworkEvents & FD_READ || NetworkEvents.lNetworkEvents &FD_WRITE)
	{
		ReadPacket(clientsocket);
	}

	board.DrawBoard();
	board.DrawBoard2();
	board.DrawBoardLine();
	board.DrawBoard3();

	//---NPC Draw
	for (int i = 0; i < NUM_OF_NPC; ++i)
	{
		if (true == NonPlayerCharacter[i].npcVisible)
			NonPlayerCharacter[i].DrawNPC();
	}

	//---Other Player Draw
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (true == otherPlayer[i].connecting)
			otherPlayer[i].DrawPlayer();
	}

	//---Player DRaw
	player.DrawPlayer();
	//cout << "x : " << player.GetX() << "y : " << player.GetY() << endl;
	player.DrawPosition();

	glutSwapBuffers(); // 결과 출력
}
GLvoid ReadPacket(SOCKET sock)
{
	DWORD iobyte, ioflag = 0;

	int ret = WSARecv(sock, &recv_wsabuf, 1, &iobyte, &ioflag, NULL, NULL);
	if (ret)
	{
		int err_code = WSAGetLastError();
		printf("Recv Error [%d]\n", err_code);
	}

	BYTE *ptr = reinterpret_cast<BYTE *>(recv_buffer);

	while (0 != iobyte)
	{
		if (0 == in_packet_size)
			in_packet_size = ptr[0];

		if (iobyte + saved_packet_size >= in_packet_size)
		{
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			iobyte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else
		{
			memcpy(packet_buffer + saved_packet_size, ptr, iobyte);
			saved_packet_size += iobyte;
			iobyte = 0;
		}
	}
}

GLvoid ProcessPacket(char *ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case SC_PUT_PLAYER:
	{
		sc_packet_put_player *my_packet = reinterpret_cast<sc_packet_put_player *>(ptr);
		int id = my_packet->id;
		if (id < NPC_START) {
			if (true == first_time) {
				first_time = false;
				g_myid = id;
				cout << "my_id : " << g_myid << endl;
			}
		}
		if (id == g_myid) {
			player.SetX(my_packet->x);
			player.SetY(my_packet->y);

		}
		else if (id < NPC_START) {
			otherPlayer[id].connecting = true;
			otherPlayer[id].SetID(id);
			otherPlayer[id].SetX(my_packet->x);
			otherPlayer[id].SetY(my_packet->y);
		}
		else {
			NonPlayerCharacter[id - NPC_START].SetX(my_packet->x);
			NonPlayerCharacter[id - NPC_START].SetY(my_packet->y);
			NonPlayerCharacter[id - NPC_START].npcVisible = true;
		}
		break;
	}
	case SC_POS:
	{
		sc_packet_pos *my_packet = reinterpret_cast<sc_packet_pos *>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			player.SetX(my_packet->x);
			player.SetY(my_packet->y);
		}
		else if (other_id < NPC_START) {
			otherPlayer[other_id].SetX(my_packet->x);
			otherPlayer[other_id].SetY(my_packet->y);
		}
		else {
			NonPlayerCharacter[other_id - NPC_START].npcVisible = true;
			NonPlayerCharacter[other_id - NPC_START].SetX(my_packet->x);
			NonPlayerCharacter[other_id - NPC_START].SetY(my_packet->y);
		}
		break;
	}

	case SC_REMOVE_PLAYER:
	{
		sc_packet_remove_player *my_packet = reinterpret_cast<sc_packet_remove_player *>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			//player.attr &= ~BOB_ATTR_VISIBLE;
		}
		else if (other_id < NPC_START) {
			otherPlayer[other_id].connecting = false;
		}
		else {
			NonPlayerCharacter[other_id - NPC_START].npcVisible = false;
		}
		break;
	}
	case SC_CHAT:
	{
		sc_packet_chat *my_packet = reinterpret_cast<sc_packet_chat *>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid)
		{
			wcsncpy_s(player.message, my_packet->message, 256);
			player.message_time = GetTickCount();
		}
		else if (other_id < NPC_START)
		{
			wcsncpy_s(otherPlayer[other_id].message, my_packet->message, 256);
			otherPlayer[other_id].message_time = GetTickCount();
		}
		else
		{
			wcsncpy_s(NonPlayerCharacter[other_id - NPC_START].message, my_packet->message, 256);
			NonPlayerCharacter[other_id - NPC_START].message_time = GetTickCount();
		}
		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}
GLvoid Reshape(int w, int h)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(60.0, w / h, 1.0, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glViewport(0, 0, w, h);
	glutPostRedisplay();
}
GLvoid Mouse(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
	{
	}
	glutPostRedisplay();
}
GLvoid Keyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
	default:
		break;
	}

}
GLvoid TimerFunction(int value)
{
	glutPostRedisplay();
	glutTimerFunc(100, TimerFunction, 1);

	glMatrixMode(GL_MODELVIEW);
}
GLvoid SpecialKeyboard(int key, int x, int y)
{
	int inputX = 0, inputY = 0;
	switch (key)
	{
	case GLUT_KEY_UP: {
		inputY += 1;
		break;
	}
	case GLUT_KEY_DOWN: {
		inputY -= 1;
		break;
	}
	case GLUT_KEY_LEFT: {
		inputX -= 1;
		break;
	}
	case GLUT_KEY_RIGHT: {
		inputX += 1;
		break;
	}
	default:
		break;
	}
	cs_packet_up *my_packet = reinterpret_cast<cs_packet_up *>(send_buffer);
	my_packet->size = sizeof(cs_packet_up);
	send_wsabuf.len = sizeof(cs_packet_up);
	DWORD iobyte;

	if (0 != inputX)
	{
		if (1 == inputX)
			my_packet->type = CS_RIGHT;
		else
			my_packet->type = CS_LEFT;
		int ret = WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
		if (ret)
		{
			int error_code = WSAGetLastError();
			printf("Error while sending packet [%d]", error_code);
		}
	}
	if (0 != inputY)
	{
		if (1 == inputY)
			my_packet->type = CS_UP;
		else
			my_packet->type = CS_DOWN;
		WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	}
}

GLubyte * LoadDIBitmap(const char *filename, BITMAPINFO **info)
{
	FILE *fp;
	GLubyte *bits;
	int bitsize, infosize;
	BITMAPFILEHEADER header;
	// 바이너리 읽기 모드로 파일을 연다
	if ((fp = fopen(filename, "rb")) == NULL)
		return NULL;
	// 비트맵 파일 헤더를 읽는다.
	if (fread(&header, sizeof(BITMAPFILEHEADER), 1, fp) < 1) {
		fclose(fp);
		return NULL;
	}
	// 파일이 BMP 파일인지 확인핚다.
	if (header.bfType != 'MB') {
		fclose(fp);
		return NULL;
	}
	// BITMAPINFOHEADER 위치로 갂다.
	infosize = header.bfOffBits - sizeof(BITMAPFILEHEADER);
	// 비트맵 이미지 데이터를 넣을 메모리 핛당을 핚다.
	if ((*info = (BITMAPINFO *)malloc(infosize)) == NULL) {
		fclose(fp);
		exit(0);
		return NULL;
	}
	// 비트맵 인포 헤더를 읽는다.
	if (fread(*info, 1, infosize, fp) < (unsigned int)infosize) {
		free(*info);
		fclose(fp);
		return NULL;
	}
	// 비트맵의 크기 설정
	if ((bitsize = (*info)->bmiHeader.biSizeImage) == 0)
		bitsize = ((*info)->bmiHeader.biWidth *
		(*info)->bmiHeader.biBitCount + 7) / 8.0 *
		abs((*info)->bmiHeader.biHeight);
	// 비트맵의 크기만큼 메모리를 핛당핚다.
	if ((bits = (unsigned char *)malloc(bitsize)) == NULL) {
		free(*info);
		fclose(fp);
		return NULL;
	}
	// 비트맵 데이터를 bit(GLubyte 타입)에 저장핚다.
	if (fread(bits, 1, bitsize, fp) < (unsigned int)bitsize) {
		free(*info); free(bits);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	return bits;
}
GLvoid textureimage()
{
	glGenTextures(6, text_ob);
	//반복
	glBindTexture(GL_TEXTURE_2D, text_ob[0]);
	pBytes = LoadDIBitmap("MyChess.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 81, 144, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//구간
	glBindTexture(GL_TEXTURE_2D, text_ob[1]);
	pBytes = LoadDIBitmap("MyChess2.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 110, 179, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, text_ob[2]);
	pBytes = LoadDIBitmap("MyChess3.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 110, 135, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, text_ob[3]);
	pBytes = LoadDIBitmap("MyChess2.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 110, 179, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//구간
	//구간
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, GL_DECAL);
	glEnable(GL_TEXTURE_2D);
}