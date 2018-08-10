#pragma comment(lib, "ws2_32")
#include <glut.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <Windows.h>
#include <windowsx.h>
#include <cstring>
#include <stdio.h>
#include <string>
#include "..\..\GameSever_TermProject\GameSever_TermProject\protocol.h"
#include "Player.h"
#include "OtherPlayer.h"
#include "CNPC.h"
#include "UI.h"
#include "Define.h"
#include "Town.h"
#include "Obstacle.h"
#include "texture.h"

int TownMapData[100][100];

using namespace std;

//#define CLIENT_TEST
#define IPADDRESS
//---------전역 변수--------
WSAEVENT hEvent;
GLubyte *pBytes;
BITMAPINFO *info;
SOCKET clientsocket;
WSANETWORKEVENTS NetworkEvents;
int networkEvent;
bool tempBool = true;

bool CameraControlling = false;

WSABUF	send_wsabuf;
char 	send_buffer[BUF_SIZE];
WSABUF	recv_wsabuf;
char	recv_buffer[BUF_SIZE];
char	packet_buffer[BUF_SIZE];
DWORD	in_packet_size = 0;
int		saved_packet_size = 0;
int		g_myid;

GLdouble CameraControlX = 0.0;
GLdouble CameraControlY = 0.0;
GLdouble CaemraControlZ =20.0;

Vector2 distVector2;
BOOL playerMove = false;

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
			glVertex3f(-0.5f, -99.5f, 0.0f);
			glVertex3f(99.5f, -99.5f, 0.0f);
			glVertex3f(99.5f, 0.5f, 0.0f);
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
		glColor4f(0.40f, 0.49f, 0.4f, 1.0f);

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
		for (int i = 1; i < 13; ++i)
		{
			glBegin(GL_LINES);
			{
				glVertex3f((8 * i) - 0.5, 0.5, 0.0f);
				glVertex3f((8 * i) - 0.5, -100.5, 0.0f);
			}glEnd();
		}
		//가로줄
		for (int i = 1; i < 13; ++i)
		{
			glBegin(GL_LINES);
			{
				glVertex3f(-0.5, -(8 * i) + 0.5, 0.0f);
				glVertex3f(100.5, -(8 * i) + 0.5, 0.0f);
			}glEnd();
		}
	}
private:
};


//-------전역 변수
CPlayer			player;
COtherPlayer	otherPlayer[MAX_USER];
CNPC			NonPlayerCharacter[NUM_OF_NPC];
CTown			town;
//---------함수 선언----------
GLvoid drawScene();
GLvoid Reshape(int w, int h);
GLvoid Mouse(int button, int state, int x, int y);
GLvoid TimerFunction(int value);
GLvoid Keyboard(unsigned char key, int x, int y);
GLvoid SpecialKeyboard(int key, int x, int y);
GLvoid textureimage();
GLubyte * LoadDIBitmap(const char *filename, BITMAPINFO **info);
GLvoid ReleaseKeyboard(int key, int x, int y);		//key에서 땟을때//움직이는거
GLvoid ReleaseKeyboard2(unsigned char key, int x, int y);		//skill이나 때리는거
GLvoid ReleaseKeyboard_Test(unsigned char key, int x, int y);
GLvoid DrawPortal();
GLvoid SceneChange();
GLvoid ProcessPacket(char *ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case SC_PUT_PLAYER:
	{
		sc_packet_put_player *my_packet = reinterpret_cast<sc_packet_put_player *>(ptr);
		int id = my_packet->id;
		if (first_time) {
			first_time = false;
			g_myid = id;
		}
		if (id == g_myid) {
			player.SetX(my_packet->x);
			player.SetY(my_packet->y);
		}
		else if (id < NPC_START) {
			otherPlayer[id].SetX(my_packet->x);
			otherPlayer[id].SetY(my_packet->y);
			otherPlayer[id].connecting = true;
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
	case SC_HIT:
	{
		cout << "sc_chatn\n";
		sc_packet_hit *my_packet = reinterpret_cast<sc_packet_hit *>(ptr);
		int other_id = my_packet->id;
		cout << "SC_CHAT ID :  " << other_id << endl;
		if (other_id == g_myid)
		{
			strcpy(player.message, my_packet->message);
			//wcsncpy_s(player.message, my_packet->message, 256);
			player.message_time = GetTickCount();
		}
		else if (other_id < NPC_START)
		{
			strcpy(otherPlayer[other_id].message, my_packet->message);
			//wcsncpy_s(otherPlayer[other_id].message, my_packet->message, 256);
			otherPlayer[other_id].message_time = GetTickCount();
		}
		else
		{
			NonPlayerCharacter[other_id - NPC_START].hello = true;

			strcpy(NonPlayerCharacter[other_id - NPC_START].message, my_packet->message);
			//wcsncpy_s(NonPlayerCharacter[other_id - NPC_START].message, my_packet->message, 256);
			cout << my_packet->message << endl;
			cout << NonPlayerCharacter[other_id - NPC_START].message << endl;
			NonPlayerCharacter[other_id - NPC_START].message_time = GetTickCount();
			player.SetCurrentHealth(my_packet->hp);
		}
		break;
	}
	case SC_BASIC_DATA: {
		cout << "sc_basic_data packet \n";
		sc_packet_basicdata *my_packet = reinterpret_cast<sc_packet_basicdata *>(ptr);
		int other_id = my_packet->id;
		cout << "sc_basic ID :  " << other_id << endl;
		if (other_id == g_myid){
			player.SetMaxHealth(my_packet->maxhp);
			player.SetCurrentHealth(my_packet->hp);
			player.SetPlayerAttack(my_packet->attack);
			player.SetPlayerLevel(my_packet->level);
		}
		break;
	}
	case SC_NPC_DEAD: {

		sc_packet_npcdead *my_packet = reinterpret_cast<sc_packet_npcdead *>(ptr);
		cout << "sc_npc_dead, NPC ID : " << my_packet->id << endl;
		int npcid = my_packet->id;

		NonPlayerCharacter[npcid - NPC_START].npcVisible = false;

		break;	
	}
	case SC_LEVELUP: {
		sc_packet_levelup *my_packet = reinterpret_cast<sc_packet_levelup *>(ptr);
		cout << "sc_levelup_packet \n";
		int maxhp = my_packet->maxhp;
		
		player.SetMaxHealth(maxhp);
		player.SetCurrentHealth(maxhp);

		break;
	}
	case SC_HP_RECOVRY: {
		sc_packet_hp_recovery *my_packet = reinterpret_cast<sc_packet_hp_recovery *>(ptr);
		cout << "sc_hp_recovery_packet \n";
		int hp = my_packet->currentHP;

		player.SetCurrentHealth(hp);
		break;
	}
	case SC_USER_DEAD: {
		sc_packet_user_dead * my_packet = reinterpret_cast<sc_packet_user_dead *>(ptr);
		cout << "sc_user_dead \n";
		int packet_id = my_packet->id;

		if (packet_id == g_myid) {
			player.SetPlayerZone(CharZone::TOWN);
			player.SetPlayerPos(my_packet->x, my_packet->y);
		}
		else {
			otherPlayer[packet_id].connecting = false;
		}


		break;
	}

	case SC_CONNECT_FAIL:
	{
		exit(1);
		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}

	SceneChange();
	
}
GLvoid SceneChange() {
	//Scene 전환 패킷
	DWORD iobyte;

	if (player.GetPlayerZone() == CharZone::TOWN) {
		//Town->Low1
		if (player.GetPlayerPos().x == 4) {
			if (player.GetPlayerPos().y == -53) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::LowZone1;
				player.SetPlayerZone(CharZone::LowZone1);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
		//Town->Mid1
		if (player.GetPlayerPos().x == 45) {
			if (player.GetPlayerPos().y == -6) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::MediumZone1;
				player.SetPlayerZone(CharZone::MediumZone1);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
		//Town->High1
		if (player.GetPlayerPos().x == 94) {
			if (player.GetPlayerPos().y == -53) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::HighZone1;
				player.SetPlayerZone(CharZone::HighZone1);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
	else if (player.GetPlayerZone() == CharZone::LowZone1)
	{
		if (player.GetPlayerPos().x == 92) {
			if (player.GetPlayerPos().y == -57) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::TOWN;
				player.SetPlayerZone(CharZone::TOWN);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
		if (player.GetPlayerPos().x == 40) {
			if (player.GetPlayerPos().y == -4) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::LowZone2;
				player.SetPlayerZone(CharZone::LowZone2);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
	else if (player.GetPlayerZone() == CharZone::LowZone2) {
		if (player.GetPlayerPos().x == 44) {
			if (player.GetPlayerPos().y == -92) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::LowZone1;
				player.SetPlayerZone(CharZone::LowZone1);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
	else if (player.GetPlayerZone() == CharZone::MediumZone1) {
		if (player.GetPlayerPos().x == 44) {
			if (player.GetPlayerPos().y == -93) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::TOWN;
				player.SetPlayerZone(CharZone::TOWN);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
		if (player.GetPlayerPos().x == 92) {
			if (player.GetPlayerPos().y == -47) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::MediumZone2;
				player.SetPlayerZone(CharZone::MediumZone2);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
	else if (player.GetPlayerZone() == CharZone::MediumZone2) {
		if (player.GetPlayerPos().x == 6) {
			if (player.GetPlayerPos().y == -48) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::MediumZone1;
				player.SetPlayerZone(CharZone::MediumZone1);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
	else if (player.GetPlayerZone() == CharZone::HighZone1) {
		if (player.GetPlayerPos().x == 6) {
			if (player.GetPlayerPos().y == -43) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::TOWN;
				player.SetPlayerZone(CharZone::TOWN);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
		if (player.GetPlayerPos().x == 51) {
			if (player.GetPlayerPos().y == -92) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::HighZone2;
				player.SetPlayerZone(CharZone::HighZone2);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
	else if (player.GetPlayerZone() == CharZone::HighZone2) {
		if (player.GetPlayerPos().x == 49) {
			if (player.GetPlayerPos().y == -7) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::HighZone1;
				player.SetPlayerZone(CharZone::HighZone1);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
		if (player.GetPlayerPos().x == 6) {
			if (player.GetPlayerPos().y == -48) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::HighZone3;
				player.SetPlayerZone(CharZone::HighZone3);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
	else if (player.GetPlayerZone() == CharZone::HighZone3) {
		if (player.GetPlayerPos().x == 92) {
			if (player.GetPlayerPos().y == -48) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::HighZone2;
				player.SetPlayerZone(CharZone::HighZone2);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
		if (player.GetPlayerPos().x == 6) {
			if (player.GetPlayerPos().y == -48) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::FRaidZone;
				player.SetPlayerZone(CharZone::FRaidZone);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
	else if (player.GetPlayerZone() == CharZone::FRaidZone) {
		if (player.GetPlayerPos().x == 92) {
			if (player.GetPlayerPos().y == -46) {
				cs_packet_scene *scene_packet = reinterpret_cast<cs_packet_scene *>(send_buffer);
				scene_packet->size = sizeof(cs_packet_scene);
				scene_packet->type = CS_SCENE;
				send_wsabuf.len = sizeof(cs_packet_scene);
				scene_packet->scene = CharZone::HighZone3;
				player.SetPlayerZone(CharZone::HighZone3);
				WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			}
		}
	}
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

	cout << "ID 입력 (최대 10자) \n";
	char ID[10];
	//strcpy(ID,"stl");
	cin >> ID;

	int Result = WSAConnect(clientsocket, (sockaddr *)&ServerAddr, sizeof(ServerAddr), NULL, NULL, NULL, NULL);
	if (retval == SOCKET_ERROR)
		err_quit("connect()");
	

	retval = send(clientsocket, ID, strlen(ID), 0);
	if (retval == SOCKET_ERROR) {
		cout << "SEND Error \n";


#ifdef CLIENTTEST

#else
		exit(1);
#endif 	
	}

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
GLvoid ShutDown_Game() {
	closesocket(clientsocket);
	WSACleanup();
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
	glutSpecialUpFunc(ReleaseKeyboard);

#ifdef CLIENT_TEST
	glutKeyboardUpFunc(ReleaseKeyboard_Test);
#else
	glutKeyboardUpFunc(ReleaseKeyboard2);
#endif // CLIENTTEST
	glutKeyboardFunc(Keyboard);
	glutTimerFunc(10, TimerFunction, 1);

	glutMainLoop();

	ShutDown_Game();
}
GLvoid DrawPortal() {
	glColor3f(0.0f, 0.2f, 1.0f);
	if (player.GetPlayerZone() == CharZone::TOWN) {
		//Town->Low1
		glPushMatrix();
		glTranslated(4,-53,0);
		glutWireCube(1);
		glPopMatrix();
		//Town->Mid1
		glPushMatrix();
		glTranslated(45, -6, 0);
		glutWireCube(1);
		glPopMatrix();
		//Town->High1
		glPushMatrix();
		glTranslated(94, -53, 0);
		glutWireCube(1);
		glPopMatrix();
	}
	if (player.GetPlayerZone() == CharZone::LowZone1) {
		//Low1->Town
		glPushMatrix();
		glTranslated(92, -57, 0);
		glutWireCube(1);
		glPopMatrix();
		//Low1->Low2
		glPushMatrix();
		glTranslated(40, -4, 0);
		glutWireCube(1);
		glPopMatrix();
	}
	if (player.GetPlayerZone() == CharZone::LowZone2) {
		//Low2->Low1
		glPushMatrix();
		glTranslated(44, -92, 0);
		glutWireCube(1);
		glPopMatrix();
	}
	if (player.GetPlayerZone() == CharZone::MediumZone1) {
		//medium1->town
		glPushMatrix();
		glTranslated(44, -93, 0);
		glutWireCube(1);
		glPopMatrix();
		//Medium1->mid2
		glPushMatrix();
		glTranslated(92, -47, 0);
		glutWireCube(1);
		glPopMatrix();
	}
	if (player.GetPlayerZone() == CharZone::MediumZone2) {
		//mid2->mid1
		glPushMatrix();
		glTranslated(6, -48, 0);
		glutWireCube(1);
		glPopMatrix();
	}
	if (player.GetPlayerZone() == CharZone::HighZone1){
		//Highzone1->town
		glPushMatrix();
		glTranslated(6, -43, 0);
		glutWireCube(1);
		glPopMatrix();
		//High1->High2
		glPushMatrix();
		glTranslated(51, -92, 0);
		glutWireCube(1);
		glPopMatrix();
	}
	if (player.GetPlayerZone() == CharZone::HighZone2){
		//Highzone2->high1
		glPushMatrix();
		glTranslated(49, -7, 0);
		glutWireCube(1);
		glPopMatrix();
		//high2->high3
		glPushMatrix();
		glTranslated(6, -48, 0);
		glutWireCube(1);
		glPopMatrix();
	}
	if (player.GetPlayerZone() == CharZone::HighZone3) {
		//Highzone3->high2
		glPushMatrix();
		glTranslated(92, -48, 0);
		glutWireCube(1);
		glPopMatrix();
		//Highzone3->FR
		glPushMatrix();
		glTranslated(6, -48, 0);
		glutWireCube(1);
		glPopMatrix();
	}
	if (player.GetPlayerZone() == CharZone::FRaidZone) {
		//Fr->high3
		glPushMatrix();
		glTranslated(92, -46, 0);
		glutWireCube(1);
		glPopMatrix();
	}


}

GLvoid drawScene()
{
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glFrontFace(GL_CCW);			//반시계방향
	glLineWidth(3.0f);

	//glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

	//Camera // Player Mode// Observer Mode
	glLoadIdentity();
	if (false == CameraControlling)
		gluLookAt(player.GetX(), player.GetY(), 20, player.GetX(), player.GetY(), -1.0, 0.0, 1.0, 0.0);
	else{
		gluLookAt(CameraControlX, CameraControlY, CaemraControlZ,
			CameraControlX, CameraControlY,
			-1.0, 0.0, 1.0, 0.0);
	}

	//network event select
	networkEvent = ::WSAEnumNetworkEvents(clientsocket, hEvent, &NetworkEvents);

	if (networkEvent == SOCKET_ERROR)
		cout << "networkEvent Error \n";

	if (NetworkEvents.lNetworkEvents & FD_READ || NetworkEvents.lNetworkEvents &FD_WRITE)
	{
		ReadPacket(clientsocket);
	}
	
	
	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[player.GetPlayerZone()]);
	glBegin(GL_QUADS);
	{
		glColor4f(1.0, 1.0, 1.0, 1.0f);

		glTexCoord2d(0.0, 1.0);
		glVertex3f(-0.5f, 0.5f, 0);

		glTexCoord2d(0.0, 0.0);
		glVertex3f(-0.5f, -99.5f, 0);

		glTexCoord2d(1.0, 0.0);
		glVertex3f(99.5f, -99.5f, 0);

		glTexCoord2d(1.0, 1.0);
		glVertex3f(99.5f, 0.5f, 0);
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);

	//---NPC Draw
	for (int i = 0; i < NUM_OF_NPC; ++i)
	{
		if (true == NonPlayerCharacter[i].npcVisible)		//죽은 NPC 안보이게하기.
				NonPlayerCharacter[i].DrawNPC();
	}

	//---Other Player Draw
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (true == otherPlayer[i].connecting)
			otherPlayer[i].DrawPlayer();
	}
	DrawPortal();
	player.DrawPlayer();
	if (true == player.attacking)
		player.DrawAttack();
	
	

	//---Player DRaw

	//cout << "x : " << player.GetX() << "y : " << player.GetY() << endl;
	player.DrawPosition();


	for (int i = 0; i < NUM_OF_NPC; ++i)
	{
		if (false == NonPlayerCharacter[i].npcVisible)
			continue;
		if (true == NonPlayerCharacter[i].hello)
			NonPlayerCharacter[i].DrawHello();
	}
	

	CUI::GetInstance()->DrawUI
	(player.GetCurrentHealth(), player.GetMaxHealth(), player.GetPlayerLevel(),player.GetPlayerAttack());
	glutSwapBuffers(); // 결과 출력
}
GLvoid Reshape(int w, int h)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0, 0, w, h);
	gluPerspective(60.0, w / h, 1.0, 1000.0);
	//glMatrixMode(GL_MODELVIEW);
	//
	glutPostRedisplay();
}
GLvoid Reshape2(int w, int h)
{
	glViewport(0, 0, w, h);
	glOrtho(0.0, 700.0,700, 0, -1.0, 1.0);
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
	case 'w':
	case 'W':{
		CameraControlY += 3.0;
		break;
	}
	case 's':
	case 'S': {
		CameraControlY -= 3.0;
		break;
	}
	case 'a':
	case 'A': {
		CameraControlX -= 3.0;
		break;
	}
	case 'd':
	case 'D': {
		CameraControlX += 3.0;
		break;
	}
	case 'q':
	case 'Q': {
		CaemraControlZ -= 1;
		break;
	}
	case 'e':
	case 'E': {
		CaemraControlZ += 1;
		break;
	}
	default:
		break;
	}

}
GLvoid ReleaseKeyboard2(unsigned char key, int x, int y)
{
	switch (key)
	{
		case 'x':
		case 'X': {
			player.mT_AttackStart=GetTickCount();
			
			player.attacking = true;

			cs_packet_attack *my_packet = reinterpret_cast<cs_packet_attack *>(send_buffer);
			my_packet->size = sizeof(cs_packet_attack);
			send_wsabuf.len = sizeof(cs_packet_attack);
			DWORD iobyte;

			my_packet->type = CS_ATTACK;

			int ret = WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			if (ret){
				int error_code = WSAGetLastError();
				printf("Error while sending packet [%d], in attack key", error_code);
			}
			break;
		}
		//여유되면 투사체 스킬?
	}
}
GLvoid ReleaseKeyboard(int key, int x, int y)
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

	glutSwapBuffers();
}
GLvoid ReleaseKeyboard_Test(unsigned char key, int x, int y)
{
	int inputX = 0, inputY = 0;
	switch (key)
	{

	case 'x':
	case 'X': {
	player.attacking = true;
	player.mT_AttackStart = GetTickCount();
	break;
	}

	case GLUT_KEY_UP: 
	{
		inputY += 1;

		Vector2 temp = player.GetPlayerPos();

		temp.y += 1;

		if (TownMapData[abs(temp.y)][temp.x] !=2)
		break;

		player.SetY(player.GetY() + 1);
		break;
	}
	case GLUT_KEY_DOWN: {
		inputY -= 1;
		Vector2 temp = player.GetPlayerPos();

		temp.y -= 1;
		if (TownMapData[abs(temp.y)][temp.x] != 2)
		break;
		player.SetY(player.GetY() -1);
		break;
	}
		case GLUT_KEY_LEFT: {
		Vector2 temp = player.GetPlayerPos();

		temp.x -= 1;
		if (TownMapData[abs(temp.y)][temp.x] != 2)
		break;


		player.SetX(player.GetX() - 1);
		inputX -= 1;
		break;
	}
	case GLUT_KEY_RIGHT: {
		Vector2 temp = player.GetPlayerPos();

		temp.x += 1;

		if (TownMapData[abs(temp.y)][temp.x] != 2)
		break;

		player.SetX(player.GetX() + 1);
		inputX += 1;
		break;
	}
	case GLUT_KEY_F1: {
		if (true == CameraControlling)
			CameraControlling = false;
		else
			CameraControlling = true;
		break; 
	}
	case GLUT_KEY_F2:
	{
		cs_packet_chat *my_packet = reinterpret_cast<cs_packet_chat *>(send_buffer);
		my_packet->size = sizeof(my_packet);
		my_packet->type = CS_HIT;
		send_wsabuf.len = sizeof(my_packet);
		DWORD iobyte;
		wcscpy(my_packet->message,L"안녕하세요");
		int ret = WSASend(clientsocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
		if (ret)
		{
			int error_code = WSAGetLastError();
			printf("Error while sending packet [%d]", error_code);
		}

		break;

	}
	case GLUT_KEY_F3:
	{
		break;
	}
	case GLUT_KEY_F4:
	{
		glutFullScreen();
	}

	default:
		break;
	}

	glutSwapBuffers();
}
GLvoid SpecialKeyboard(int key, int x, int y)
{
	switch (key)
	{

	default:
		break;
	}
}
GLvoid TimerFunction(int value)
{
	//if (true==tempBool){
	//	player.SetCurrentHealth(player.GetCurrentHealth() + 5);
	//	if (player.GetCurrentHealth() >= player.GetMaxHealth())
	//		tempBool = false;
	//}
	//else
	//{
	//	player.SetCurrentHealth(player.GetCurrentHealth() - 1);
	//	if (player.GetCurrentHealth() <= 1)
	//		tempBool = true;
	//}

	//공격관련
	if (true == player.attacking){
		if (GetTickCount() -player.mT_AttackStart  > 400){
			player.attacking = false;
		}
	}

	for (int i = 0; i < NUM_OF_NPC; ++i) {
		if (false == NonPlayerCharacter[i].npcVisible)
			continue;
		if (true == NonPlayerCharacter[i].hello)
			if (GetTickCount() - NonPlayerCharacter[i].message_time > 2000)
				NonPlayerCharacter[i].hello = false;
	}
	

	glutPostRedisplay();
	glutTimerFunc(10, TimerFunction, 1);

	glMatrixMode(GL_MODELVIEW);
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
	glGenTextures(9, CTexture::GetInstance()->mTexture_Object);
	//반복
	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[0]);
	pBytes = LoadDIBitmap("Resource/Town.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//구간
	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[1]);
	pBytes = LoadDIBitmap("Resource/LowZone.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[2]);
	pBytes = LoadDIBitmap("Resource/LowZone2.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[3]);
	pBytes = LoadDIBitmap("Resource/MediumZone1.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[4]);
	pBytes = LoadDIBitmap("Resource/MediumZone2.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[5]);
	pBytes = LoadDIBitmap("Resource/HighZone1.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[6]);
	pBytes = LoadDIBitmap("Resource/HighZone2.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[7]);
	pBytes = LoadDIBitmap("Resource/HighZone3.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, CTexture::GetInstance()->mTexture_Object[8]);
	pBytes = LoadDIBitmap("Resource/FieldRaid.bmp", &info);	// 왜 널값??
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 1600, 1600, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pBytes); //w,, h
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//구간
	//구간
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, GL_MODULATE);
	glEnable(GL_TEXTURE_2D);
}