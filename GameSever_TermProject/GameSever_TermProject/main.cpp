#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")
//#include <winsock.h>
#include <WinSock2.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include "protocol.h"
#include "DB.h"
#include <mutex>
#include <list>
#include <set>
#include <unordered_set>
#include <random>
#include <queue>
#include <chrono>
#include <sqlext.h>
#include <fstream>
extern "C" {
#include "lua_include\lua.h"
#include "lua_include\lauxlib.h"
#include "lua_include\luaconf.h"
#include "lua_include\lualib.h"
}
#define NUM_OF_THREAD	4
#define NAME_LEN 50  
#define PHONE_LEN 20  

#define IDLE 0
#define ATTACK 1

#define USE_DB

using namespace std;
using namespace chrono;

enum OPTYPE { OP_SEND, OP_RECV, OP_MOVE, OP_DO_AI,OP_PLAYER_MOVE_NOTIFY,OP_PLAYER_HP_RECOVERY,OP_ESCAPE};
enum Event_Type { E_MOVE ,E_REVIVENPC,E_RECOVERY_HP, E_ESCAPE};




OVERLAPPED aa;

//확장 오버랩 구조체
struct OverlappedEx{
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	unsigned char IOCP_buf[MAX_BUFF_SIZE];
	OPTYPE event_type;
	int event_target;
};
struct CLIENT			//컨텐츠에 쓰이는 구조체
{
	//TODO : 차후수정 int->vec2  구조체 변경작업
	int x;
	int y;
	//vec2 pos;

	int hp;				//현재체력
	int maxhp;			//최디채력	
	
	int attack;			//공격력

	int exp;		// 현재 경험치	
	int rexp;		// 레벨에 따른 요구경험치 (100*level)
	int level;		// 현재 레벨.

	bool state;		//플레이어의 상태 향후 npc에 체력추가에도 사용가능할듯

	lua_State *L;
	int zone;			//현재 어느 곳(마을, 사냥터 등)인지		//DB연동 필요
	//int sector;		//sector - 현재 존에 시야처리 부하분산을 위한 섹터.


	//last idle time?

	int id;				//Clienet 배열 인덱스
	char cid[10];		//게임내에서 사용할 클라이언트 아이디;


	high_resolution_clock::time_point last_state_time;
	std::chrono::high_resolution_clock::time_point last_move_time; // npc의 움직임시간 제어 + 스피드핵 방지

	bool is_healing;		//플레이어 힐링 상태.

	bool is_active; // npc가 현재 움직였나 안움직였나 판단하기 위함
	bool is_alive;
	unordered_set<int> ViewList;			//걍 set보다 훨씬빠르다!
	mutex vl_lock;						//view list lock
	bool connect;									//접속해서 사용여부
	SOCKET clienet_socket;							//클라이언트 소켓
	OverlappedEx recv_over;
	unsigned char packet_buf[MAX_PACKET_SIZE];		//패킷을 저장하는 버퍼
	int prev_packet_data;							//current_packet_size보다 작다.
	int current_packet_sizes;						//현재 처리하고 있는 패킷의 크기 얼마냐
};
struct Timer_Event {
	int object_id;
	high_resolution_clock::time_point exec_time; // 이 이벤트가 언제 실행되야 하는가
	Event_Type event; // 게임을 확장하며 무브외에 다른것들이 추가될 것
};

class comparison {
	bool reverse;
public:
	comparison() {}
	bool operator() (const Timer_Event first, const Timer_Event second) const {
		return first.exec_time > second.exec_time;
	}
};

//Map Data
WORD TownMapData[100][100];
WORD LowZone1Map[100][100];
WORD LowZone2Map[100][100];
WORD MediumZone1Map[100][100];
WORD MediumZone2Map[100][100];
WORD HighZone1Map[100][100];
WORD HighZone2Map[100][100];
WORD HighZone3Map[100][100];
WORD FieldRaidZoneMap[100][100];


//----------전역 변수--------------
CLIENT g_clients[NUM_OF_NPC];			//나중에 다른 자료구조로 변경..
mutex tq_lock; // 우선순위 큐 제어를 위한 락
priority_queue <Timer_Event, vector<Timer_Event>, comparison> timer_queue; // 우선순위큐, 그냥 쓰면 죽으니까 락을 걸어줘야함
HANDLE g_hiocp;		//IOCP 핸들
SOCKET g_ssocket;
mutex g_mutex;
mutex hitLock;

int g_DBx;	//load
int g_DBy;
int g_DBz;
int g_DBlevel;
int g_DBhp;
int g_DBrexp;
int g_DBexp;


int g_DBx2;	//save
int g_DBy2;
int g_DBz2;
int g_DBlevel2;
int g_DBhp2;
int g_DBrexp2;
int g_DBexp2;

//--------함수 선언-------
void DB_Init();


void error_display(char *msg, int err_no)
{
	WCHAR *lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("%s", msg);
	wprintf(L"에러%s\n", lpMsgBuf);
	LocalFree(lpMsgBuf);
}
void show_error() {
	cout << "Error!\n";
}
bool IsClose(int from, int to)
{
	return (
		(g_clients[from].x - g_clients[to].x) *
		(g_clients[from].x - g_clients[to].x) +
		(g_clients[from].y - g_clients[to].y) *
		(g_clients[from].y - g_clients[to].y) <= VIEW_RADIUS*VIEW_RADIUS);
	//sqrt 가 부하가 좀있어서 위에처럼 하는게 좀 더 효율적
}
bool IsSameZone(int c1,int c2) {
	return (g_clients[c1].zone == g_clients[c2].zone);
}
bool Is_NPC(int id){
	return (id >= NPC_START) && (id < NUM_OF_NPC);
}

void SendPacket(int client_id, void * packet)
{
	//Send용 버퍼 생성
	int packet_size = reinterpret_cast<unsigned char*>(packet)[0];
	int ptype = reinterpret_cast<unsigned char*>(packet)[1];
	OverlappedEx * over = new OverlappedEx;
	over->event_type = OP_SEND;
	memcpy(over->IOCP_buf, packet, reinterpret_cast<unsigned char*>(packet)[0]);	//메모리카피 최대한 억제해아함, 패킷을 한번 만드고 최대한 재사용해야함!
	ZeroMemory(&over->over, sizeof(over->over));
	over->wsabuf.buf = reinterpret_cast<CHAR*>(over->IOCP_buf);
	over->wsabuf.len = packet_size;

	int ret = WSASend(g_clients[client_id].clienet_socket, &over->wsabuf, 1, NULL, 0, &over->over, 0);
	if (ret != 0)
	{
		int error_num = WSAGetLastError();
		if (WSA_IO_PENDING != error_num)			//IO_PENDING 나오면 에러가 아니다!, 무시해도됨.
			error_display("SendPacket : WSASend", error_num);
	}
	//cout << "send packet : " <<  ptype <<"   ID : "<< client_id << endl;
}
void SendChatPacket(int client, int object, char * message)		//왼쪽 인자 클라이언트한테 오른쪽 오브젝트 정보를 보내라!
{
	sc_packet_hit packet;
	packet.id = object;
	packet.size = sizeof(packet);
	packet.type = SC_HIT;
	packet.hp = g_clients[client].hp;
	if (MAX_STR_SIZE <= strlen(message)) {
		cout << "Message too long at SendChatPacket() \n";
		return;
	}
	//wcscpy(packet.message, message);//보안검사 필요!
	strcpy(packet.message, message);
	SendPacket(client, &packet);
}
void SendDisconnectPacket(int id, int object)
{
	sc_packet_connectfail packet;
	packet.size = sizeof(packet);
	packet.type = SC_CONNECT_FAIL;

	SendPacket(id, &packet);
}
void SendPutPlayerPacket(int id, int object)
{
	sc_packet_put_player packet;
	packet.id = object;
	packet.size = sizeof(packet);
	packet.type = SC_PUT_PLAYER;
	packet.x = g_clients[object].x;
	packet.y = g_clients[object].y;
	packet.zone = g_clients[object].zone;
	SendPacket(id, &packet);
}
void SendRemovePlayerPacket(int id, int object)
{
	sc_packet_remove_player packet;
	packet.id = object;
	packet.size = sizeof(packet);
	packet.type = SC_REMOVE_PLAYER;

	SendPacket(id, &packet);
}
void DisconnectClient(int ci)
{
	closesocket(g_clients[ci].clienet_socket);
	g_clients[ci].connect = false;
	unordered_set<int > lvl;		//local view list

	g_clients[ci].vl_lock.lock();
	lvl = g_clients[ci].ViewList;
	g_clients[ci].vl_lock.unlock();

	for (auto target : lvl)
	{
		g_clients[target].vl_lock.lock();
		if (0 != g_clients[target].ViewList.count(ci))
		{
			g_clients[target].ViewList.erase(ci);
			g_clients[target].vl_lock.unlock();
			SendRemovePlayerPacket(target, ci);
		}
		else
			g_clients[target].vl_lock.unlock();
	}
	g_clients[ci].vl_lock.lock();
	g_clients[ci].ViewList.clear();
	g_clients[ci].vl_lock.unlock();

}
void SendPositionPacket(int client, int object)		//왼쪽 인자 클라이언트한테 오른쪽 오브젝트 정보를 보내라!
{
	//현재 동접이 5000이면 패킷을 5000번 만들어서 5000번 보냄
	//한번 만들어서 5000번 보내야함!
	sc_packet_pos packet;
	packet.id = object;
	packet.size = sizeof(packet);
	packet.type = SC_POS;
	packet.x = g_clients[object].x;
	packet.y = g_clients[object].y;

	SendPacket(client, &packet);
}
void SendBasicDataPAcket(int id, int object) {

	sc_packet_basicdata packet;
	packet.size = sizeof(packet);
	packet.type = SC_BASIC_DATA;
	packet.maxhp = g_clients[object].maxhp;
	packet.hp = g_clients[object].hp;
	packet.attack = g_clients[object].attack;
	packet.level = g_clients[object].level;

	SendPacket(id, &packet);
}
void SendNPCDeadPacket(int id, int object) {
	sc_packet_npcdead packet;
	packet.size = sizeof(packet);
	packet.type = SC_NPC_DEAD;
	packet.id = object;

	SendPacket(id, &packet);
}
void SendLevelUpPacket(int id, int object) {
	sc_packet_levelup packet;
	packet.size = sizeof(packet);
	packet.type = SC_LEVELUP;
	packet.maxhp = g_clients[id].maxhp;

	SendPacket(id, &packet);
}
void SendHPRecoveryPacket(int id, int object) {
	sc_packet_hp_recovery packet;
	packet.size = sizeof(packet);
	packet.type = SC_HP_RECOVRY;
	packet.currentHP = g_clients[id].hp;

	SendPacket(id, &packet);
}
void SendPlayerDeadPacket(int id, int object) {
	sc_packet_user_dead packet;
	packet.size = sizeof(packet);
	packet.type = SC_USER_DEAD;
	packet.id = object;
	packet.x = 45;
	packet.y = -10;
	packet.hp = (g_clients[object].hp);

	SendPacket(id, &packet);
}
void WakeUpNPC(int);
void viewfunc(int ci) {

	unordered_set<int> new_view_list;

	//다른 유저들과의시야처리
	for (int i = 0; i < MAX_USER; ++i) {
		if (true == g_clients[i].connect) {
			if (IsSameZone(ci, i))
				if (i != ci)
				{
					if (true == IsClose(ci, i))
						new_view_list.insert(i);
				}
		}
	}
	for (int i = NPC_START; i < NUM_OF_NPC; ++i)
		if (true == IsSameZone(ci, i))
			if (true == IsClose(ci, i))
				if (true == g_clients[i].is_alive)
					new_view_list.insert(i);

	////새로 추가되는 객체 - 

	unordered_set<int> vlc;
	g_clients[ci].vl_lock.lock();
	vlc = g_clients[ci].ViewList;
	g_clients[ci].vl_lock.unlock();

	for (auto target : new_view_list) {
		if (0 == vlc.count(target)) {
			//1.새로 추가되는 객체
			SendPutPlayerPacket(ci, target);
			vlc.insert(target);

			if (true == Is_NPC(target)) {
				WakeUpNPC(target);
				continue;
			}

			g_clients[target].vl_lock.lock();
			//critical section 구간
			if (0 != g_clients[target].ViewList.count(ci)) {
				g_clients[target].vl_lock.unlock();
				SendPositionPacket(target, ci);
			}
			else
			{
				g_clients[target].ViewList.insert(ci);
				g_clients[target].vl_lock.unlock();
				SendPutPlayerPacket(target, ci);
			}

		}
		else {
			//변동없이 존재하는 객체
			//npc는 필요가 없어서 아무것도 해주면안된다
			if (true == Is_NPC(target))continue;
			g_clients[target].vl_lock.lock();
			if (0 != g_clients[target].ViewList.count(ci))
			{
				g_clients[target].vl_lock.unlock();
				SendPositionPacket(target, ci);
			}
			else {
				g_clients[target].ViewList.insert(ci);
				g_clients[target].vl_lock.unlock();
				SendPutPlayerPacket(target, ci);
			}
		}
	}
	//3.시야에서 멀어진 객체
	unordered_set<int> removed_id_list;
	for (auto target : vlc) {
		if (0 == new_view_list.count(target)) {
			SendRemovePlayerPacket(ci, target);
			removed_id_list.insert(target);
			// 글로벌 공유자료라서 여러개가
			// 접근을 하기때문에 터진다. 그러므로 락을 걸어줘야 한다
			// 상대방에게도 내 정보를 지워줌
			// 이미 없을수도 있으니 그냥 지워주면 안되고 검사가 필요
			if (true == Is_NPC(target)) continue; // npc는 이런거 해줄필요가 없다
			g_clients[target].vl_lock.lock();
			if (0 != g_clients[target].ViewList.count(ci)) {
				g_clients[target].ViewList.erase(ci);
				g_clients[target].vl_lock.unlock();
				SendRemovePlayerPacket(target, ci);
			}
			else g_clients[target].vl_lock.unlock();
		}
	}

	g_clients[ci].vl_lock.lock();
	for (auto p : vlc)
		g_clients[ci].ViewList.insert(p);
	for (auto d : removed_id_list)
		g_clients[ci].ViewList.erase(d);
	g_clients[ci].vl_lock.unlock();

	for (auto npc : new_view_list) {
		if (false == Is_NPC(npc))
			continue;
		OverlappedEx * over_ex = new OverlappedEx;
		over_ex->event_target = ci;
		over_ex->event_type = OP_PLAYER_MOVE_NOTIFY;
		//근처에 있느 애들한테만 이동했다고 알려주고
		PostQueuedCompletionStatus(g_hiocp, 1, npc, &over_ex->over);
	}

}

int API_get_x(lua_State *L) {
	int oid = lua_tonumber(L, -1);
	lua_pop(L, 2);
	lua_pushnumber(L, g_clients[oid].x);
	return 1;
}
int API_get_y(lua_State *L) {
	int oid = lua_tonumber(L, -1);
	lua_pop(L, 2);
	lua_pushnumber(L, g_clients[oid].y);
	return 1;
}
int API_SendMessage_HIT(lua_State *L) {
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char *mess = (char *)lua_tostring(L, -1);
	size_t wlen, len = strlen(mess) + 1;
	char wmess[MAX_STR_SIZE];

	lua_pop(L, 3);
	len = (MAX_STR_SIZE - 1<len) ?
		MAX_STR_SIZE - 1 : len;

	//my_id : NPCID
	//userid : userid
	
	hitLock.lock();

	if (true == g_clients[my_id].is_alive) {
		g_clients[my_id].hp -= g_clients[user_id].attack;
		g_clients[user_id].hp -= g_clients[my_id].zone * 1;
		g_clients[user_id].state = ATTACK;
		g_clients[user_id].last_state_time = chrono::high_resolution_clock::now();
	
		g_clients[my_id].is_healing = true;

		if (g_clients[my_id].hp <= 0) {
		
			g_clients[my_id].is_alive = false;

			if (g_clients[user_id].hp<0) {
				g_clients[user_id].hp = g_clients[user_id].maxhp* 0.5;
				g_clients[user_id].zone = CharZone::TOWN;
				g_clients[user_id].x = 45;
				g_clients[user_id].y = -10;
				SendPlayerDeadPacket(user_id, user_id);
				for (auto near_user : g_clients[user_id].ViewList) {
					if(near_user<NPC_START)
						SendPlayerDeadPacket(near_user, user_id);
				}
				viewfunc(user_id);
			}

			Timer_Event npc_revive_event = { my_id, high_resolution_clock::now() + 30s ,E_REVIVENPC };
			timer_queue.push(npc_revive_event);
			SendNPCDeadPacket(my_id, my_id);		

			for (auto npcdead : g_clients[user_id].ViewList) {
				if(npcdead<NPC_START)
					SendNPCDeadPacket(my_id, npcdead);
			}
			g_clients[user_id].exp += 10;

			//플레이어 레벨업 체크.
			if (g_clients[user_id].exp > g_clients[user_id].rexp) {
				g_clients[user_id].level += 1;
				int level = g_clients[user_id].level;
				g_clients[user_id].exp = 0;
				g_clients[user_id].maxhp = level*100;
				g_clients[user_id].hp = level * 100;
				g_clients[user_id].rexp = level * 100;

				SendLevelUpPacket(user_id, user_id);
			}
		}

	}
	hitLock.unlock();

	//레벨리이 체크 필요
	//락을 어떻게 걸어야 잘걸지..

	//mbstowcs_s(&wlen, wmess, len, mess, _TRUNCATE);

	wmess[MAX_STR_SIZE - 1] = (wchar_t)0;

	//플레이어 체력정보도 보내줘야함
	SendChatPacket(user_id, my_id, mess);
	return 0;
}
void NPC_Random_Move(int );
int API_Move_NPC(lua_State *L) {


	int my_id = (int)lua_tointeger(L, -2);
	int dir = (int)lua_tointeger(L, -1);
	int x = g_clients[my_id].x;
	int y = g_clients[my_id].y;


	NPC_Random_Move(my_id);
	return 1;
}

void Initialize_NPC() // npc 초기화 함수
{
	cout << "Init NPC.. \n";

	//Zone당 1500 NPC 총 NPC10500
	//LowZone NPC
	for (int i = NPC_START; i < 1000; ++i) {
		g_clients[i].connect = false;

		while (true)
		{
			g_clients[i].x = rand() % 100;
			g_clients[i].y = -(rand() % 100);
			if (LowZone1Map[abs(g_clients[i].y)][g_clients[i].x] == 2)
				break;
		}
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // 라스트 무브 타임 초기화
		g_clients[i].is_active = false;
		g_clients[i].zone = CharZone::LowZone1;
		g_clients[i].hp = 10* CharZone::LowZone1;
	}
	//LowZone2
	for (int i = 1000; i < 1500; ++i) {
		g_clients[i].connect = false;

		while (true)
		{
			g_clients[i].x = rand() % 100;
			g_clients[i].y = -(rand() % 100);
			if (LowZone2Map[abs(g_clients[i].y)][g_clients[i].x] == 2)
				break;
		}
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // 라스트 무브 타임 초기화
		g_clients[i].is_active = false;
		g_clients[i].zone = CharZone::LowZone2;
		g_clients[i].hp = 10 * CharZone::LowZone2;
	}
	//MidZone1
	for (int i = 1500; i < 2000; ++i) {
		g_clients[i].connect = false;

		while (true)
		{
			g_clients[i].x = rand() % 100;
			g_clients[i].y = -(rand() % 100);
			if (MediumZone1Map[abs(g_clients[i].y)][g_clients[i].x] == 2)
				break;
		}
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // 라스트 무브 타임 초기화
		g_clients[i].is_active = false;
		g_clients[i].zone = CharZone::MediumZone1;
		g_clients[i].hp = 10 * CharZone::MediumZone1;
	}
	//MidZone2
	for (int i = 2000; i <2500 ; ++i) {
		g_clients[i].connect = false;

		while (true)
		{
			g_clients[i].x = rand() % 100;
			g_clients[i].y = -(rand() % 100);
			if (MediumZone2Map[abs(g_clients[i].y)][g_clients[i].x] == 2)
				break;
		}
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // 라스트 무브 타임 초기화
		g_clients[i].is_active = false;
		g_clients[i].zone = CharZone::MediumZone2;
		g_clients[i].hp = 10 * CharZone::MediumZone2;
	}
	//HighZone1
	for (int i = 2500; i <3000; ++i) {
		g_clients[i].connect = false;

		while (true)
		{
			g_clients[i].x = rand() % 100;
			g_clients[i].y = -(rand() % 100);
			if (HighZone1Map[abs(g_clients[i].y)][g_clients[i].x] == 2)
				break;
		}
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // 라스트 무브 타임 초기화
		g_clients[i].is_active = false;
		g_clients[i].zone = CharZone::HighZone1;
		g_clients[i].hp = 10 * CharZone::HighZone1;
	}

	for (int i = 3000; i <3500; ++i) {
		g_clients[i].connect = false;

		while (true)
		{
			g_clients[i].x = rand() % 100;
			g_clients[i].y = -(rand() % 100);
			if (HighZone2Map[abs(g_clients[i].y)][g_clients[i].x] == 2)
				break;
		}
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // 라스트 무브 타임 초기화
		g_clients[i].is_active = false;
		g_clients[i].zone = CharZone::HighZone2;
		g_clients[i].hp = 10 * CharZone::HighZone2;
	}

	for (int i = 3500; i <4000; ++i) {
		g_clients[i].connect = false;

		while (true)
		{
			g_clients[i].x = rand() % 100;
			g_clients[i].y = -(rand() % 100);
			if (HighZone3Map[abs(g_clients[i].y)][g_clients[i].x] == 2)
				break;
		}
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // 라스트 무브 타임 초기화
		g_clients[i].is_active = false;
		g_clients[i].zone = CharZone::HighZone3;
		g_clients[i].hp = 10 * CharZone::HighZone3;
	}

	for (int i = NPC_START; i < 4000; ++i)
	{
		g_clients[i].is_alive = true;
	}

	for (int i = NPC_START; i < NUM_OF_NPC; ++i) {
		lua_State *L = luaL_newstate();
		luaL_openlibs(L);

		luaL_loadfile(L, "monster.lua");
		if (0 != lua_pcall(L, 0, 0, 0))
		{
			printf("lua error \n", lua_tostring(L, -1));
			lua_close(L);
		}
		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 1, 0);
		lua_pop(L, 1);

		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		lua_register(L, "API_HIT", API_SendMessage_HIT);

		//
		lua_register(L, "API_Move_NPC", API_Move_NPC);
		g_clients[i].L = L;
	}

	cout << "Inin NPC Complete.. \n";
}
void WakeUpNPC(int id)
{
	if (true == g_clients[id].is_active) return;
	g_clients[id].is_active = true;
	Timer_Event event = { id, high_resolution_clock::now() + 1s ,E_MOVE };
	tq_lock.lock();
	timer_queue.push(event);
	tq_lock.unlock();
}
void LoadMap() {
	ifstream MyMapFile;

	MyMapFile.open("Map/TownMap.txt");
	cout << "TownMapLoading... \n";
	for (int i = 0; i < 100; ++i){
		for (int j = 0; j < 100; ++j){
			char a[1];
			MyMapFile >> a;

			TownMapData[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "TownMapLoading Complete... \n";

	MyMapFile.open("Map/LowZone1.txt");
	cout << "LowZone1MapLoading... \n";
	for (int i = 0; i < 100; ++i){
		for (int j = 0; j < 100; ++j){
			char a[1];
			MyMapFile >> a;

			LowZone1Map[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "LowZone1MapLoading Complete... \n";

	MyMapFile.open("Map/LowZone2.txt");
	cout << "LowZone2MapLoading... \n";
	for (int i = 0; i < 100; ++i){
		for (int j = 0; j < 100; ++j){
			char a[1];
			MyMapFile >> a;

			LowZone2Map[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "LowZone2MapLoading Complete... \n";

	MyMapFile.open("Map/MediumZone1.txt");
	cout << "MediumZone1 MapLoading... \n";
	for (int i = 0; i < 100; ++i){
		for (int j = 0; j < 100; ++j){
			char a[1];
			MyMapFile >> a;

			MediumZone1Map[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "MediumZone1 MapLoading Complete... \n";

	MyMapFile.open("Map/MediumZone2.txt");
	cout << "MediumZone2 MapLoading... \n";
	for (int i = 0; i < 100; ++i){
		for (int j = 0; j < 100; ++j){
			char a[1];
			MyMapFile >> a;

			MediumZone2Map[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "MediumZone2 MapLoading Complete... \n";

	MyMapFile.open("Map/HighZone1.txt");
	cout << "HighZone1 MapLoading... \n";
	for (int i = 0; i < 100; ++i){
		for (int j = 0; j < 100; ++j){
			char a[1];
			MyMapFile >> a;

			HighZone1Map[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "HighZone2 MapLoading Complete... \n";

	MyMapFile.open("Map/HighZone2.txt");
	cout << "HighZone2 MapLoading... \n";
	for (int i = 0; i < 100; ++i) {
		for (int j = 0; j < 100; ++j) {
			char a[1];
			MyMapFile >> a;

			HighZone2Map[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "HighZone2 MapLoading Complete... \n";

	MyMapFile.open("Map/HighZone3.txt");
	cout << "HighZone3 MapLoading... \n";
	for (int i = 0; i < 100; ++i) {
		for (int j = 0; j < 100; ++j) {
			char a[1];
			MyMapFile >> a;

			HighZone3Map[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "HighZone3 MapLoading Complete... \n";

	MyMapFile.open("Map/FieldRaidZone.txt");
	cout << "FieldRaidZone MapLoading... \n";
	for (int i = 0; i < 100; ++i) {
		for (int j = 0; j < 100; ++j) {
			char a[1];
			MyMapFile >> a;

			FieldRaidZoneMap[i][j] = atoi(a);
		}
	}
	MyMapFile.close();
	cout << "FieldRaidZone MapLoading Complete... \n";


}
void Init_Server()			//네트워크 이닛
{
	LoadMap();

	DB_Init();
	Initialize_NPC();

	wcout.imbue(std::locale("korean"));
	_wsetlocale(LC_ALL, L"korean");

	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	g_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, NULL, NULL);		//IOCP 생성

																				//비동기 Io로 만들어야함!->마지막인자에 WSA_flag_overlapped추가
	g_ssocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (g_ssocket == SOCKET_ERROR) { cout << "socket error \n"; }
	else if (g_ssocket == INVALID_SOCKET) { cout << "invalid socket \n"; }
	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(MY_SERVER_PORT);
	ServerAddr.sin_addr.s_addr = (INADDR_ANY);

	//bind -> 
	::bind(g_ssocket, reinterpret_cast<sockaddr*>(&ServerAddr), sizeof(ServerAddr));
	//listen
	int retval = listen(g_ssocket, 5);		//동접이 늘어나면 마지막 인자 늘려줘야한다
	if (retval == SOCKET_ERROR){
		int error_num = WSAGetLastError();
		error_display("listen socket error", error_num);
	}

	for (int i = 0; i < MAX_USER; ++i)
	{
		g_clients[i].connect = false;
	}
}

bool NPC_Object_Collision(int id, vec2 temp) {

	if (g_clients[id].zone == CharZone::LowZone1) {
		if (LowZone1Map[abs(temp.y)][temp.x] != 2)
			return false;
		else
			return true;
	}
	else if (g_clients[id].zone == CharZone::LowZone2) {
		if (LowZone2Map[abs(temp.y)][temp.x] != 2)
			return false;
		else
			return true;
	}
	else if (g_clients[id].zone == CharZone::MediumZone1) {
		if (MediumZone1Map[abs(temp.y)][temp.x] != 2)
			return false;
		else
			return true;
	}
	else if (g_clients[id].zone == CharZone::MediumZone2) {
		if (MediumZone2Map[abs(temp.y)][temp.x] != 2)
			return false;
		else
			return true;
	}
	else if (g_clients[id].zone == CharZone::HighZone1) {
		if (HighZone1Map[abs(temp.y)][temp.x] != 2)
			return false;
		else
			return true;
	}
	else if (g_clients[id].zone == CharZone::HighZone2) {
		if (HighZone2Map[abs(temp.y)][temp.x] != 2)
			return false;
		else
			return true;
	}
	else if (g_clients[id].zone == CharZone::HighZone3) {
		if (HighZone3Map[abs(temp.y)][temp.x] != 2)
			return false;
		else
			return true;
	}

}
void NPC_Random_Move(int id)
{

	if (false == g_clients[id].is_alive)
		return;
	//   for (int i = 0; i < 10000; ++i) sum = sum + i; // cpu시간을 좀 잡아먹게끔 해줌, 10만으로 늘리면 렉이걸림. 풀어줘야함. 다음시간에~
	switch (rand() % 4) 
	{
		case 0: 
		{	
			vec2 temp;
			temp.x = g_clients[id].x;	temp.y = g_clients[id].y;
			temp.y += 1;

			if (false == NPC_Object_Collision(id, temp))
				break;

			g_clients[id].y++;
			break;
		}
		case 1: 
		{
			vec2 temp;
			temp.x = g_clients[id].x;	temp.y = g_clients[id].y;
			temp.y -= 1;

			if (false == NPC_Object_Collision(id, temp))
				break;
			g_clients[id].y--;
			break;
		}
		case 2: 
		{
			vec2 temp;
			temp.x = g_clients[id].x;	temp.y = g_clients[id].y;
			temp.x -= 1;

			if (false == NPC_Object_Collision(id, temp))
				break;
			g_clients[id].x--;
			break;
		}
		case 3:
		{
			vec2 temp;
			temp.x = g_clients[id].x;	temp.y = g_clients[id].y;
			temp.x += 1;

			if (false == NPC_Object_Collision(id, temp))
				break;
			g_clients[id].x++;
			break;
		}
	}

	// 주위에 있는 플레이어들에게 npc가 움직였다고 보내줌
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (true == IsSameZone(i, id))
		{
			if (true == g_clients[i].connect)
			{
				g_clients[i].vl_lock.lock();
				if (0 != g_clients[i].ViewList.count(id))
				{
					if (true == IsClose(i, id))
					{
						g_clients[i].vl_lock.unlock();
						SendPositionPacket(i, id);
					}
					else
					{
						g_clients[i].ViewList.erase(id);
						g_clients[i].vl_lock.unlock();
						SendRemovePlayerPacket(i, id); // npc가 시야에서 멀어지면 지워줌
					}
				}
				else
				{
					if (true == IsClose(i, id))
					{
						g_clients[i].ViewList.insert(id);
						g_clients[i].vl_lock.unlock();
						SendPutPlayerPacket(i, id);
					}
					else {
						g_clients[i].vl_lock.unlock();
					}
				}
			}
		}
	}

	for (int i = 0; i < MAX_USER; ++i) {

		if ((true == g_clients[i].connect) && (IsClose(i, id))) {
			Timer_Event t = { id, high_resolution_clock::now() + 1s, E_MOVE };
			tq_lock.lock();
			timer_queue.push(t);
			tq_lock.unlock(); // 락을 걸어주고 큐에 넣어줘야 된다
			return;
		}
	}
	g_clients[id].is_active = false;
}
void ChangeScene(int currentScene, int nextScene,int id) {
	//TOWN->Other..
	if (currentScene == CharZone::TOWN) {
		if (nextScene == CharZone::LowZone1) {
			g_clients[id].x = 90; g_clients[id].y = -57;
		}
		else if (nextScene == CharZone::MediumZone1) {
			g_clients[id].x = 40; g_clients[id].y = -90;
		}
		else if (nextScene == CharZone::HighZone1) {
			g_clients[id].x = 8; g_clients[id].y = -40;
		}
	}
	//Low1->Other.
	if (currentScene == CharZone::LowZone1) {
		if (nextScene == CharZone::TOWN) {
			g_clients[id].x = 6; g_clients[id].y = -53;
		}
		if (nextScene == CharZone::LowZone2) {
			g_clients[id].x = 42; g_clients[id].y = -90;
		}
	}
	if (currentScene == CharZone::LowZone2) {
		if (nextScene == CharZone::LowZone1) {
			g_clients[id].x = 40; g_clients[id].y = -6;
		}
	}
	if (currentScene == CharZone::MediumZone1) {
		if (nextScene == CharZone::TOWN) {
			g_clients[id].x = 45; g_clients[id].y = -8;
		}
		if (nextScene == CharZone::MediumZone2) {
			g_clients[id].x = 8; g_clients[id].y = -50;
		}
	}
	if (currentScene == CharZone::MediumZone2) {
		if (nextScene == CharZone::MediumZone1) {
			g_clients[id].x = 90; g_clients[id].y = -45;
		}
	}
	if (currentScene == CharZone::HighZone1) {
		if (nextScene == CharZone::TOWN) {
			g_clients[id].x = 92; g_clients[id].y = -53;
		}
		else if (nextScene == CharZone::HighZone2) {
			g_clients[id].x = 50; g_clients[id].y = -9;
		}
	}
	if (currentScene == CharZone::HighZone2) {
		if (nextScene == CharZone::HighZone1) {
			g_clients[id].x = 50; g_clients[id].y = -90;
		}
		else {	// nextscene == CharZone::highzone3
			g_clients[id].x = 90; g_clients[id].y = -50;
		}
	}
	if (currentScene == CharZone::HighZone3) {
		if (nextScene == CharZone::HighZone2) {
			g_clients[id].x = 8; g_clients[id].y = -50;
		}
		else if (nextScene == CharZone::FRaidZone) {
			g_clients[id].x = 90; g_clients[id].y = -50;
		}
	}
	if (currentScene == CharZone::FRaidZone) {
		if (nextScene == CharZone::HighZone3) {
			g_clients[id].x = 8; g_clients[id].y = -50;
		}
	}
}

bool ObjectCollision(int ci,vec2 temp) {
	switch (g_clients[ci].zone)
	{
		case CharZone::TOWN: {
			if (TownMapData[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		case CharZone::LowZone1: {
			if (LowZone1Map[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		case CharZone::LowZone2: {
			if (LowZone2Map[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		case CharZone::MediumZone1: {
			if (MediumZone1Map[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		case CharZone::MediumZone2: {
			if (MediumZone2Map[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		case CharZone::HighZone1: {
			if (HighZone1Map[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		case CharZone::HighZone2: {
			if (HighZone2Map[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		case CharZone::HighZone3: {
			if (HighZone3Map[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		case CharZone::FRaidZone: {
			if (FieldRaidZoneMap[abs(temp.y)][temp.x] != 2)
				return false;
			else
				return true;
			break;
		}
		default:
			cout << "In objectcollision func.. unknown switch varation.. \n";
			break;
	}
}

void ProcessPacket(int ci, unsigned char *packet)
{
	switch (packet[1])
	{
	case CS_UP:
	{
		vec2 temp;
		temp.x = g_clients[ci].x;	temp.y = g_clients[ci].y+1;
		if(true==ObjectCollision(ci, temp))
			g_clients[ci].y++;
		break;
	}
	case CS_DOWN:
	{
		vec2 temp;
		temp.x = g_clients[ci].x;	temp.y = g_clients[ci].y-1;
		if (true==ObjectCollision(ci, temp))
			g_clients[ci].y--;
		break;
	}
	case CS_LEFT:
	{
		vec2 temp;
		temp.x = g_clients[ci].x-1;	temp.y = g_clients[ci].y;
		if (true == ObjectCollision(ci, temp))
			g_clients[ci].x--;
		break;
	}
	case CS_RIGHT:
	{
		vec2 temp;
		temp.x = g_clients[ci].x+1;	temp.y = g_clients[ci].y;
		if (true == ObjectCollision(ci, temp))
			g_clients[ci].x++;
		break;
	}
	case CS_SCENE:{
		cout << "Scene Change \n";
		cs_packet_scene *my_packet = reinterpret_cast<cs_packet_scene *>(packet);
		int currentZone = g_clients[ci].zone;
		g_clients[ci].zone = my_packet->scene;
		ChangeScene(currentZone,g_clients[ci].zone,ci);
		cout << g_clients[ci].zone << endl;
		break;
	}

	default: {
		cout << "Unknown Packet Type \n";
		break;
		}
	}
	SendPositionPacket(ci, ci);


	//시야처리 부분 함수처리해서 해야할듯.
#pragma region 시야처리
	//패킷을 미리 만들고 패킷의 포인터로 보내야함, 오버랩IO갯수만큼 보내고 delete를 해줘야함

	unordered_set<int> new_view_list;

	//다른 유저들과의시야처리
	for (int i = 0; i < MAX_USER; ++i) {
		if (true == g_clients[i].connect) {
			if(IsSameZone(ci,i))
				if (i != ci) 
				{
					if (true == IsClose(ci, i))
						new_view_list.insert(i);
				}
		}
	}
	for (int i = NPC_START; i < NUM_OF_NPC; ++i)
		if(true==IsSameZone(ci,i))
			if (true == IsClose(ci, i))		
				if(true==g_clients[i].is_alive)
					new_view_list.insert(i);

	////새로 추가되는 객체 - 

	unordered_set<int> vlc;
	g_clients[ci].vl_lock.lock();
	vlc = g_clients[ci].ViewList;
	g_clients[ci].vl_lock.unlock();

	for (auto target : new_view_list) {
		if (0 == vlc.count(target)){
			//1.새로 추가되는 객체
			SendPutPlayerPacket(ci, target);
			vlc.insert(target);

			if (true == Is_NPC(target)) {
				WakeUpNPC(target);
				continue;
			}

			g_clients[target].vl_lock.lock();
			//critical section 구간
			if (0 != g_clients[target].ViewList.count(ci)){
				g_clients[target].vl_lock.unlock();
				SendPositionPacket(target, ci);
			}
			else
			{
				g_clients[target].ViewList.insert(ci);
				g_clients[target].vl_lock.unlock();
				SendPutPlayerPacket(target, ci);
			}

		}
		else {
			//변동없이 존재하는 객체
			//npc는 필요가 없어서 아무것도 해주면안된다
			if (true == Is_NPC(target))continue;
			g_clients[target].vl_lock.lock();
			if (0 != g_clients[target].ViewList.count(ci))
			{
				g_clients[target].vl_lock.unlock();
				SendPositionPacket(target, ci);
			}
			else {
				g_clients[target].ViewList.insert(ci);
				g_clients[target].vl_lock.unlock();
				SendPutPlayerPacket(target, ci);
			}
		}
	}
	//3.시야에서 멀어진 객체
	unordered_set<int> removed_id_list;
	for (auto target : vlc) {
		if (0 == new_view_list.count(target)) {
			SendRemovePlayerPacket(ci, target);
			removed_id_list.insert(target);
			// 글로벌 공유자료라서 여러개가
			// 접근을 하기때문에 터진다. 그러므로 락을 걸어줘야 한다
			// 상대방에게도 내 정보를 지워줌
			// 이미 없을수도 있으니 그냥 지워주면 안되고 검사가 필요
			if (true == Is_NPC(target)) continue; // npc는 이런거 해줄필요가 없다
			g_clients[target].vl_lock.lock();
			if (0 != g_clients[target].ViewList.count(ci)){
				g_clients[target].ViewList.erase(ci);
				g_clients[target].vl_lock.unlock();
				SendRemovePlayerPacket(target, ci);
			}
			else g_clients[target].vl_lock.unlock();
		}
	}

	g_clients[ci].vl_lock.lock();
	for (auto p : vlc) 
		g_clients[ci].ViewList.insert(p);
	for (auto d : removed_id_list) 
		g_clients[ci].ViewList.erase(d);
	g_clients[ci].vl_lock.unlock();

	for (auto npc : new_view_list) {
		if (false == Is_NPC(npc))
			continue;
		OverlappedEx * over_ex = new OverlappedEx;
		over_ex->event_target = ci;
		over_ex->event_type = OP_PLAYER_MOVE_NOTIFY;
		//근처에 있느 애들한테만 이동했다고 알려주고
		PostQueuedCompletionStatus(g_hiocp, 1, npc, &over_ex->over);
	}

#pragma endregion

}
void Accept_Thread()
{
	int retval;

	while (true)
	{
		SOCKADDR_IN ClientAddr;
		ZeroMemory(&ClientAddr, sizeof(SOCKADDR_IN));
		ClientAddr.sin_family = AF_INET;
		ClientAddr.sin_port = htons(MY_SERVER_PORT);
		ClientAddr.sin_addr.s_addr = (INADDR_ANY);
		int addr_size = sizeof(ClientAddr);

		SOCKET newClientSocket = 
			WSAAccept(g_ssocket, reinterpret_cast<sockaddr*>(&ClientAddr), &addr_size, NULL, NULL);
		if (newClientSocket == INVALID_SOCKET)
		{
			int error_num = WSAGetLastError();
			error_display("invalid socket", error_num);
		}
		int new_id = -1;

		for (int i = 0; i < MAX_USER; ++i){
			if (g_clients[i].connect == false){
				new_id = i;
				break;
			}
		}
		if (-1 == new_id){
			closesocket(newClientSocket);
			cout << "Server Full \n";
		}

		char game_id[10];
		char buf2[10];

		retval = recv(newClientSocket, buf2, 10, 0);
		if (retval == SOCKET_ERROR) {
			cout << "recv Error\n";
		}
		cout << "retval : " << retval << endl;
		buf2[retval] = '\0';
		strcpy(game_id, buf2);
		cout << game_id << endl;

		//새 클라이언트의 데이터 초기화작업
		g_clients[new_id].connect = true;
		g_clients[new_id].clienet_socket = newClientSocket;
		g_clients[new_id].current_packet_sizes = 0;				//여기선 재활용하니가 전에 쓰던게 남아있을수도 0ㅇ으로 초기화
		g_clients[new_id].prev_packet_data = 0;
		ZeroMemory(&g_clients[new_id].recv_over, sizeof(g_clients[new_id].recv_over));
		g_clients[new_id].recv_over.event_type = OP_RECV;
		g_clients[new_id].recv_over.wsabuf.buf
			= reinterpret_cast<char*>(g_clients[new_id].recv_over.IOCP_buf);

		g_clients[new_id].recv_over.wsabuf.len = sizeof(g_clients[new_id].recv_over.IOCP_buf);

#ifdef USE_DB
		// Allocate statement handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);


			//EXEC 프로시저를 부르는 명령어
			sprintf(buf, "EXEC dbo.search_id %s", game_id);
			MultiByteToWideChar(CP_UTF8, 0, buf, strlen(buf), sql_data, sizeof sql_data / sizeof *sql_data);
			sql_data[strlen(buf)] = '\0';
			//Execute a SQL statement

			retcode = SQLExecDirect(hstmt, sql_data, SQL_NTS);

			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			{
				// Bind columns 1, 2, and 3  
				retcode = SQLBindCol(hstmt, 1, SQL_WCHAR, szName, NAME_LEN, &cbName);
				retcode = SQLBindCol(hstmt, 2, SQL_INTEGER, &g_DBx, 4, &cbX);		//x
				retcode = SQLBindCol(hstmt, 3, SQL_INTEGER, &g_DBy, 4, &cbY);		//y
				retcode = SQLBindCol(hstmt, 4, SQL_INTEGER, &g_DBz, 4, &cbZ);		//z

				retcode = SQLBindCol(hstmt, 5, SQL_INTEGER, &g_DBlevel, 4, &cbZ);		//level
				retcode = SQLBindCol(hstmt, 6, SQL_INTEGER, &g_DBhp, 4, &cbZ);		//hp
				retcode = SQLBindCol(hstmt, 7, SQL_INTEGER, &g_DBrexp, 4, &cbZ);		//rexp	
				retcode = SQLBindCol(hstmt, 8, SQL_INTEGER, &g_DBexp, 4, &cbZ);		//exp

				// Fetch and print each row of data. On an error, display a message and exit.  

				retcode = SQLFetch(hstmt);
				if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
					show_error();
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					cout << "ID : " << szName
						<< "  x : " << g_DBx
						<< "  y : " << g_DBy
						<< "  z : " << g_DBz
						<< "  level : " << g_DBlevel
						<< "  hp  : " << g_DBhp
						<< "  rexp : " << g_DBrexp
						<< "  exp : " << g_DBexp
						<< endl;

					cout << "\n";
				}
				else {
					cout << "No Data \n";
					SendDisconnectPacket(new_id, new_id);
					g_clients[new_id].connect = false;
					closesocket(newClientSocket);
					SQLDisconnect(hdbc);
					continue;
				}

			}

			//// Process data  
			//if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			//	SQLCancel(hstmt);
			//	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
			//}
			//SQLDisconnect(hdbc);

		}
#endif
		g_clients[new_id].x = g_DBx;
		g_clients[new_id].y = g_DBy;
		g_clients[new_id].zone = g_DBz;

		g_clients[new_id].is_alive = true;
		g_clients[new_id].id = new_id;
		g_clients[new_id].hp = g_DBhp;		//DB에 저장
	
		g_clients[new_id].exp = g_DBexp;		//DB에 저장 필수.
		g_clients[new_id].level = g_DBlevel;	//DB저장필순

		int level = g_clients[new_id].level;

		g_clients[new_id].rexp = g_DBrexp;	//DB저장해야되고	일단 100 (rexp = level*100); 할필요잇나?
		g_clients[new_id].maxhp = g_clients[new_id].level * 100;

		g_clients[new_id].attack = g_clients[new_id].level*5;

		strcpy(g_clients[new_id].cid, game_id);
		//이후 IOCP에 소켓을 등록
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(newClientSocket), g_hiocp, new_id, 0);

		cout << "Player : " << new_id << "  Connect \n";


		//방금 접속한 유저한테 정보 보내는거
	
		//다른 플레이어들에게 내 정보를 보내고 , 다른 플레이어들의 정보를 받아오는 로직
		DWORD recv_flag = 0;			//꼭 0넣어줘야함
		retval = WSARecv(newClientSocket, &g_clients[new_id].recv_over.wsabuf, 1,
			NULL, &recv_flag, &g_clients[new_id].recv_over.over, NULL);
		if (0 != retval) {
			int error_num = WSAGetLastError();
			if (WSA_IO_PENDING != error_num) {
				error_display("AcceptThread : WSARecv ", error_num);
			}
		}
		SendPutPlayerPacket(new_id, new_id);
		SendBasicDataPAcket(new_id, new_id);
		unordered_set<int> local_view_list;

		for (int i = 0; i < MAX_USER; ++i) // 이것도 범위를 바꿔줘야됨. 밑에 npc용으로 따로 만듬
		{
			if (g_clients[i].connect == true)
			{
				if (new_id != i)
				{
					if (true == IsClose(i, new_id)) { // 거리가 가까우면 보내줌
						SendPutPlayerPacket(new_id, i);
						local_view_list.insert(i);
						SendPutPlayerPacket(i, new_id);
						g_clients[new_id].ViewList.insert(i); // 시야를 추가해줌
						g_clients[i].vl_lock.lock();
						g_clients[i].ViewList.insert(new_id); // 상대방에게도 보내줌
						g_clients[i].vl_lock.unlock();
					}
				}
			}
			if (g_clients[new_id].zone == CharZone::TOWN)
			{

			}

			if (g_clients[new_id].zone == CharZone::LowZone1)
			{
				for (int i = NPC_START; i < 1000; ++i) { // npc가 범위에 있을때 정보를 보내줘야하므로 이렇게 따로 만들어줌
					if (true == IsClose(new_id, i))
					{
						WakeUpNPC(i);
						local_view_list.insert(i); // 주위에 npc가 있다면 뷰리스트에 넣어줌
						SendPutPlayerPacket(new_id, i); // 주위 npc정보를 접속한 유저에게 보내줌
					}
				}
			}

			else if (g_clients[new_id].zone == CharZone::LowZone2)
			{
				for (int i = 1000; i < 1500; ++i) { // npc가 범위에 있을때 정보를 보내줘야하므로 이렇게 따로 만들어줌
					if (true == IsClose(new_id, i))
					{
						WakeUpNPC(i);
						local_view_list.insert(i); // 주위에 npc가 있다면 뷰리스트에 넣어줌
						SendPutPlayerPacket(new_id, i); // 주위 npc정보를 접속한 유저에게 보내줌
					}
				}
			}
			else if (g_clients[new_id].zone == CharZone::MediumZone1)
			{
				for (int i = 1500; i < 2000; ++i) { // npc가 범위에 있을때 정보를 보내줘야하므로 이렇게 따로 만들어줌
					if (true == IsClose(new_id, i))
					{
						WakeUpNPC(i);
						local_view_list.insert(i); // 주위에 npc가 있다면 뷰리스트에 넣어줌
						SendPutPlayerPacket(new_id, i); // 주위 npc정보를 접속한 유저에게 보내줌
					}
				}
			}
			else if (g_clients[new_id].zone == CharZone::MediumZone2)
			{
				for (int i = 2000; i < 2500; ++i) { // npc가 범위에 있을때 정보를 보내줘야하므로 이렇게 따로 만들어줌
					if (true == IsClose(new_id, i))
					{
						WakeUpNPC(i);
						local_view_list.insert(i); // 주위에 npc가 있다면 뷰리스트에 넣어줌
						SendPutPlayerPacket(new_id, i); // 주위 npc정보를 접속한 유저에게 보내줌
					}
				}
			}
			else if (g_clients[new_id].zone == CharZone::HighZone1)
			{
				for (int i = 2500; i < 3000; ++i) { // npc가 범위에 있을때 정보를 보내줘야하므로 이렇게 따로 만들어줌
					if (true == IsClose(new_id, i))
					{
						WakeUpNPC(i);
						local_view_list.insert(i); // 주위에 npc가 있다면 뷰리스트에 넣어줌
						SendPutPlayerPacket(new_id, i); // 주위 npc정보를 접속한 유저에게 보내줌
					}
				}
			}
			else if (g_clients[new_id].zone == CharZone::HighZone2)
			{
				for (int i = 3000; i < 3500; ++i) { // npc가 범위에 있을때 정보를 보내줘야하므로 이렇게 따로 만들어줌
					if (true == IsClose(new_id, i))
					{
						WakeUpNPC(i);
						local_view_list.insert(i); // 주위에 npc가 있다면 뷰리스트에 넣어줌
						SendPutPlayerPacket(new_id, i); // 주위 npc정보를 접속한 유저에게 보내줌
					}
				}
			}
			else if (g_clients[new_id].zone == CharZone::HighZone3)
			{
				for (int i = 3500; i < 4000; ++i) { // npc가 범위에 있을때 정보를 보내줘야하므로 이렇게 따로 만들어줌
					if (true == IsClose(new_id, i))
					{
						WakeUpNPC(i);
						local_view_list.insert(i); // 주위에 npc가 있다면 뷰리스트에 넣어줌
						SendPutPlayerPacket(new_id, i); // 주위 npc정보를 접속한 유저에게 보내줌
					}
				}
			}
			

			g_clients[new_id].vl_lock.lock();
			for (auto p : local_view_list) g_clients[new_id].ViewList.insert(p);
			g_clients[new_id].vl_lock.unlock();
		}

		Timer_Event player_recoveryHP_event = { new_id, high_resolution_clock::now() + 5s ,E_RECOVERY_HP };
		timer_queue.push(player_recoveryHP_event);

	}//end while
}
void Worekr_Thread()
{
	DWORD io_size;
	unsigned long long ci;  ////ci - clents id
	OverlappedEx *over;

	while (true)
	{
		BOOL ret =
			GetQueuedCompletionStatus(g_hiocp, &io_size, &ci, reinterpret_cast<LPWSAOVERLAPPED*>(&over), INFINITE);

		if (FALSE == ret)
		{
			error_display("GQCS : ", WSAGetLastError());
			//cout << "ERROR in GQCS \n";
			//while (true);
		}
		if (0 == io_size)		//클라이언트에 접속이 끊겼을때
		{
			//주변 유저들한테 끊겼다고 보내야하~
			DisconnectClient(ci);
			continue;
		}

		if (OP_RECV == over->event_type)
		{
			unsigned char *buf = g_clients[ci].recv_over.IOCP_buf;
			unsigned psize = g_clients[ci].current_packet_sizes;
			unsigned pr_size = g_clients[ci].prev_packet_data;

			while (io_size > 0)		//io사이즈가 0ㅇ이 되면 끝났단 얘기
			{

				if (0 == psize)				//패킷이 0이면 안되므로 강제로 셋팅
					psize = buf[0];

				//그냥 빼오는게 아니라 패킷이 되나 안되나 확인하고 빼온다
				//지난번에 받은거랑 지금 받은거랑 비교해서 크거나 같으면 패킷 완성 가능
				if (io_size + pr_size >= psize)
				{	//지금 패킷 완성 가능
					//memcpy->전부다 오버헤드고 캐쉬에 악영향을 끼친다.
					unsigned char packet[MAX_PACKET_SIZE];
					memcpy(packet, g_clients[ci].packet_buf, pr_size);
					memcpy(packet + pr_size, buf, psize - pr_size);
					ProcessPacket(ci, packet);

					io_size -= psize - pr_size;
					buf += psize - pr_size;

					psize = 0;
					pr_size = 0;
				}
				else{
					memcpy(g_clients[ci].packet_buf + pr_size, buf, io_size); //겹치지 않게 뒤에다 이어 붙여줌
					pr_size += io_size;
					io_size = 0;
				}
			}

			g_clients[ci].current_packet_sizes = psize;
			g_clients[ci].prev_packet_data = pr_size;

			DWORD recv_flag = 0;			//꼭 0넣어줘야함
			int retval = WSARecv(g_clients[ci].clienet_socket, &g_clients[ci].recv_over.wsabuf, 1,
				NULL, &recv_flag, &g_clients[ci].recv_over.over, NULL);
			if (0 != retval) {
				int error_num = WSAGetLastError();
				if (WSA_IO_PENDING != error_num) {
					error_display("worekrthread : WSARecv ", error_num);
				}
			}

		}
		else if (OP_SEND == over->event_type){
			if (io_size != over->wsabuf.len)
			{
				cout << "Send incomplete error \n";
				closesocket(g_clients[ci].clienet_socket);
				g_clients[ci].connect = false;
			}
			delete over;
		}
		else if (OP_DO_AI == over->event_type)
		{
			if (true==g_clients[ci].is_alive )
			{
				lua_State* L = g_clients[ci].L;
				lua_getglobal(L, "NPC_move");
				lua_pushnumber(L, ci);
				if (0 != lua_pcall(L, 1, 0, 0)){
					printf("LUA Error : npc_move; %s\n", lua_tostring(L, -1));
					lua_close(L);
				}
			}
			delete over;
		}

		else if (OP_PLAYER_MOVE_NOTIFY == over->event_type)
		{
			lua_State *L = g_clients[ci].L;
			lua_getglobal(L, "player_move_notify");
			lua_pushnumber(L, over->event_target);
			lua_pushnumber(L, rand() % 4);
			if (0 != lua_pcall(L, 2, 1, 0))
			{
				printf("lua error : func \n", lua_tostring(L, -1));
				lua_close(L);
			}
			int retval = (int)lua_tointeger(L, -1);
			if (1==retval)
			{
				Timer_Event event = { ci, high_resolution_clock::now(),E_MOVE };
				tq_lock.lock();
				timer_queue.push(event);
				tq_lock.unlock();
			}
			lua_pop(L, -1);

			delete over;
		}
		else if (OP_PLAYER_HP_RECOVERY == over->event_type) {
			
			if (IDLE == g_clients[ci].state){
				if (g_clients[ci].hp < g_clients[ci].maxhp) {
					g_clients[ci].hp += g_clients[ci].maxhp*0.05;
					if (g_clients[ci].hp > g_clients[ci].maxhp)
						g_clients[ci].hp = g_clients[ci].maxhp;			
					SendHPRecoveryPacket(ci, ci);
				}
				Timer_Event player_recoveryHP_event = { ci, high_resolution_clock::now() + 5s ,E_RECOVERY_HP };
				timer_queue.push(player_recoveryHP_event);
			}
			delete over;
		}
		else {
			cout << "Unknown GQCS Event \n";
		}
	}
}
void Timer_Thread()
{
	for (;;) {
		Sleep(10);
		for (;;)
		{
			tq_lock.lock();
			if (0 == timer_queue.size()) {
				tq_lock.unlock();
				break;
			} // 큐가 비어있으면 꺼내면 안되니까
			Timer_Event t = timer_queue.top(); // 여러 이벤트 중에 실행시간이 제일 최근인 이벤트를 실행해야 하므로 우선순위 큐를 만듬
			if (t.exec_time > high_resolution_clock::now()) {
				tq_lock.unlock();
				break; // 현재시간보다 크다면, 기다려줌
			}
			timer_queue.pop();
			tq_lock.unlock();
			if (E_MOVE == t.event) {
				OverlappedEx *over = new OverlappedEx;
				over->event_type = OP_DO_AI;
				PostQueuedCompletionStatus(g_hiocp, 1, t.object_id, &over->over);
			}
			else if (E_REVIVENPC == t.event) {
				g_clients[t.object_id].is_alive = true;
				g_clients[t.object_id].hp = (10)*g_clients[t.object_id].zone;

				//워커스레드에서 작업해야하나? 락을 걸어야하면 워커스레드에서 하는게 옳을듯.
				
				//NPC 살려내고 다시 AI움직이게?
			}
			else if (E_RECOVERY_HP == t.event) {

				OverlappedEx *over = new OverlappedEx;
				over->event_type = OP_PLAYER_HP_RECOVERY;
				PostQueuedCompletionStatus(g_hiocp, 1, t.object_id, &over->over);
			}
			else if (E_ESCAPE == t.event) {
				OverlappedEx *over = new OverlappedEx;
				over->event_type = OP_ESCAPE;
				PostQueuedCompletionStatus(g_hiocp, 1, t.object_id, &over->over);
			}
		}
	}
}


void Logic_Thread() {

	static float frameDelta = 0;
	static float lastTime = timeGetTime();

	float currTime;
	float FPS = 30.0f;
	//무한 루프 
	while (true) {

		currTime = timeGetTime();
		frameDelta = (currTime - lastTime) * 0.001f;
		             
		if (frameDelta >= 1 / FPS){
			for (int i = 0; i < MAX_USER; ++i) {
				if (true == g_clients[i].connect) {
					if (true==g_clients[i].is_healing) {
						if (g_clients[i].last_state_time + 5s > high_resolution_clock::now())
						{
							g_clients[i].state = IDLE;
							g_clients[i].is_healing = false;
						}
					}
					
						
					//여기서 다시 타이머 돌리면 될거같은데
					
				}
			}

			lastTime = currTime;
		}
	}
	
}
void DB_Init() {
	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);



	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);
		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
			}
		}
	}

	// Connect to data source  
	retcode = SQLConnect(hdbc, (SQLWCHAR*)L"2011182004_kimkinam", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);
	//@@@@@@db에 한번만 커넥트하기'@@@@@@

}
void Shutdown_DB()
{
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);

	SQLDisconnect(hdbc);
}
void Shutdown_Server()
{
	closesocket(g_ssocket);
	CloseHandle(g_hiocp);
	WSACleanup();
}
void Save_Data(int ci) {
	
	retcode = SQLConnect(hdbc, (SQLWCHAR*)L"2011182004_kimkinam", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);
	//cout << "save _ data \n";
	// Allocate statement handle  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
		sprintf(buf2, "EXEC dbo.save_pos %s, %d, %d, %d ,%d, %d, %d, %d",
			g_clients[ci].cid, g_clients[ci].x, g_clients[ci].y,g_clients[ci].zone,
			g_clients[ci].level, g_clients[ci].hp, g_clients[ci].rexp, g_clients[ci].exp);

		MultiByteToWideChar(CP_UTF8, 0, buf2, strlen(buf2), sql_data, sizeof sql_data / sizeof *sql_data);
		sql_data[strlen(buf2)] = '\0';

		retcode = SQLExecDirect(hstmt, sql_data, SQL_NTS);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			// Bind columns 1, 2, and 3  
			retcode = SQLBindCol(hstmt, 1, SQL_WCHAR, &szID, NAME_LEN, &cb_id);
			retcode = SQLBindCol(hstmt, 2, SQL_INTEGER, &g_DBx2, PHONE_LEN, &cb_x);
			retcode = SQLBindCol(hstmt, 3, SQL_INTEGER, &g_DBy2, PHONE_LEN, &cb_y);
			retcode = SQLBindCol(hstmt, 4, SQL_INTEGER, &g_DBz2, PHONE_LEN, &cb_z);
			retcode = SQLBindCol(hstmt, 5, SQL_INTEGER, &g_DBlevel2, PHONE_LEN, &cbLevel);
			retcode = SQLBindCol(hstmt, 6, SQL_INTEGER, &g_DBhp2, PHONE_LEN, &cbHp);
			retcode = SQLBindCol(hstmt, 7, SQL_INTEGER, &g_DBrexp2, PHONE_LEN, &cbRexp);
			retcode = SQLBindCol(hstmt, 8, SQL_INTEGER, &g_DBexp2, PHONE_LEN, &cbExp);
			
			// Fetch and print each row of data. On an error, display a message and exit.  
			retcode = SQLFetch(hstmt);
			if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
				show_error();

			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				//cout << szID;

				//printf("X : %d\tY : %d\t Zone : %d\t\n", g_DBx2, g_DBy2,g_DBz2);
				//printf("level : %d\tHP : %d\trexp : %d\texp : %d\t\n", g_DBlevel2, g_DBhp2, g_DBrexp2, g_DBexp2);
			}

		}
	}
	SQLDisconnect(hdbc);
}
void Database_Thread() {
	DWORD startTime = GetTickCount();
	while (true) {
		Sleep(10);
		if (GetTickCount() - startTime > 3000){		//3초마다 저장
			for (int i = 0; i < MAX_USER; ++i)
			{
				if (false == g_clients[i].connect)
					continue;
				Save_Data(i);
			}
			startTime = GetTickCount();
		}
	}
}

int main()
{
	srand(time(NULL));
	Init_Server();

	thread AcceptThread{ Accept_Thread };
	thread TimerThread{ Timer_Thread };
	thread LogicThread{ Logic_Thread };
	thread DBThread{ Database_Thread };
	vector<thread*> worker_threads;
	for (int i = 0; i < NUM_OF_THREAD; ++i)
	{
		worker_threads.push_back(new thread{ Worekr_Thread });
	}

	//Threads Join..
	DBThread.join();
	LogicThread.join();
	TimerThread.join();
	AcceptThread.join();
	for (auto pth : worker_threads) {
		pth->join();
		delete pth;
	}
	Shutdown_DB();
	Shutdown_Server();
}