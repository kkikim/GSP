#include <WinSock2.h>
#include <winsock.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set> // 대신 이걸 써주는게 훨씬좋다. 성능이 훨씬 빠름
// map을 쓸때도 순서상관 없으면 저런맵을 써주는게 좋음
#include <mutex> // 락을 걸어주기 위한 뮤텍스선언
#include <chrono> // npc의 마지막 움직인 시간을 넣어주기 위해 선언
#include <queue> // 이벤트큐를 생성하기 위한 선언

#include "protocol.h"

HANDLE g_hiocp;
SOCKET g_ssocket;

enum OPTYPE { OP_SEND, OP_RECV, OP_DO_AI }; // 워커스레드에게 어떤 이벤트를 실행하라고 알려줄지의 정보
enum Event_Type { E_MOVE };
using namespace std;
using namespace chrono;

struct OverlappedEx {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	unsigned char IOCP_buf[MAX_BUFF_SIZE];
	OPTYPE event_type;
};

struct CLIENT {
	int x;
	int y;
	int id;
	std::chrono::high_resolution_clock::time_point last_move_time; // npc의 움직임시간 제어 + 스피드핵 방지
	bool is_active; // npc가 현재 움직였나 안움직였나 판단하기 위함
	bool connect;
	SOCKET client_socket;
	OverlappedEx recv_over;
	unsigned char packet_buf[MAX_PACKET_SIZE];
	int prev_packet_data;
	int curr_packet_size;
	std::unordered_set<int> view_list; // 억셉트할때 생성해준다
	std::mutex vl_lock;
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

priority_queue <Timer_Event, vector<Timer_Event>, comparison> timer_queue; // 우선순위큐, 그냥 쓰면 죽으니까 락을 걸어줘야함
mutex tq_lock; // 우선순위 큐 제어를 위한 락

CLIENT g_clients[NUM_OF_NPC]; // npc까지 들어갈수 있도록 충분히 만들어줌

void error_display(char *msg, int err_no)
{
	WCHAR *lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L"에러" << lpMsgBuf << std::endl;
	LocalFree(lpMsgBuf);
	while (true);
}

bool Is_Close(int from, int to) // 거리가 가깝나 검사하기 위한 함수
{
	return (g_clients[from].x - g_clients[to].x)
		* (g_clients[from].x - g_clients[to].x)
		+ (g_clients[from].y - g_clients[to].y)
		* (g_clients[from].y - g_clients[to].y) <= VIEW_RADIUS *VIEW_RADIUS;
}

bool Is_NPC(int id)
{
	return (id >= NPC_START) && (id < NUM_OF_NPC);
}

void Initialize_NPC() // npc 초기화 함수
{
	for (int i = NPC_START; i < NUM_OF_NPC; ++i) {
		g_clients[i].connect = false;
		g_clients[i].x = rand() % BOARD_WIDTH; // 모든맵에 뿌려줘야지 안그러면 모든 npc가 겹쳐버림
		g_clients[i].y = rand() % BOARD_HEIGHT; // 마찬가지
												// 뷰리스트가 있는 이유는 유령과 좀비를 없애주기 위함임. 근데 npc는 필요가 음슴
												// 이렇게 접속을 false로 해주고 골고루 뿌려주면 초기화 끝
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // 라스트 무브 타임 초기화
		g_clients[i].is_active = false;
	}
}

void WakeUpNPC(int id)
{
	// 이미 움직인 애를 또 움직이면 안되므로 상태가 필요함
	if (true == g_clients[id].is_active) return;
	g_clients[id].is_active = true;
	Timer_Event event = { id, high_resolution_clock::now() + 1s ,E_MOVE };
	tq_lock.lock();
	timer_queue.push(event);
	tq_lock.unlock();
}

void Initialize_Server()
{
	// 서버 초기화를 하면서 npc도 초기화ㅡㄹ 해준다
	Initialize_NPC();

	std::wcout.imbue(std::locale("korean"));
	WSADATA   wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	g_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, NULL, 0);

	g_ssocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(MY_SERVER_PORT);
	ServerAddr.sin_addr.s_addr = INADDR_ANY;

	::bind(g_ssocket, reinterpret_cast<sockaddr *>(&ServerAddr), sizeof(ServerAddr));
	listen(g_ssocket, 5);

	for (int i = 0; i < MAX_USER; ++i) {
		g_clients[i].connect = false;
	}
}

void SendPacket(int cl, void *packet)
{
	int psize = reinterpret_cast<unsigned char *>(packet)[0];
	int ptype = reinterpret_cast<unsigned char *>(packet)[1];
	OverlappedEx *over = new OverlappedEx;
	over->event_type = OP_SEND;
	memcpy(over->IOCP_buf, packet, psize);
	ZeroMemory(&over->over, sizeof(over->over));
	over->wsabuf.buf = reinterpret_cast<CHAR *>(over->IOCP_buf);
	over->wsabuf.len = psize;

	int ret = WSASend(g_clients[cl].client_socket, &over->wsabuf, 1, NULL, 0,
		&over->over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			error_display("Error in SendPacket:", err_no);
	}
	std::cout << "Send Packet [" << ptype << "] To Client : " << cl << std::endl;
}

void SendPutPlayerPacket(int client, int object)
{
	sc_packet_put_player packet;
	packet.id = object;
	packet.size = sizeof(packet);
	packet.type = SC_PUT_PLAYER;
	packet.x = g_clients[object].x;
	packet.y = g_clients[object].y;

	SendPacket(client, &packet);
}

void SendPositionPacket(int client, int object)
{
	sc_packet_pos packet;
	packet.id = object;
	packet.size = sizeof(packet);
	packet.type = SC_POS;
	packet.x = g_clients[object].x;
	packet.y = g_clients[object].y;

	SendPacket(client, &packet);
}

void SendRemovePlayerPacket(int client, int object)
{
	sc_packet_pos packet;
	packet.id = object;
	packet.size = sizeof(packet);
	packet.type = SC_REMOVE_PLAYER;
	SendPacket(client, &packet);
}

void ProcessPacket(int ci, unsigned char packet[])
{
	switch (packet[1]) {
	case CS_UP: if (g_clients[ci].y > 0) g_clients[ci].y--; break;
	case CS_DOWN: if (g_clients[ci].y < BOARD_HEIGHT - 1) g_clients[ci].y++; break;
	case CS_LEFT: if (g_clients[ci].x > 0) g_clients[ci].x--; break;
	case CS_RIGHT: if (g_clients[ci].x < BOARD_WIDTH - 1) g_clients[ci].x++; break;
	default: std::cout << "Unknown Packet Type from Client : " << ci << std::endl;
		while (true);
	}

	SendPositionPacket(ci, ci);

	std::unordered_set<int> new_view_list; // 이동처리를 위해 새로 만들어줌
	
	for (int i = 0; i < MAX_USER; ++i)
		if (true == g_clients[i].connect)
			if (i != ci)
				if (true == Is_Close(ci, i))
					new_view_list.insert(i); // 자기자신은 넣어주면 안된다

											 // npc도 이동할때 정보를 같이 넣어줘야된다
	for (int i = NPC_START; i < NUM_OF_NPC; ++i)
		if (true == Is_Close(ci, i)) new_view_list.insert(i);

	// 카피를 할때 락을 걸어줘야함
	std::unordered_set<int> vlc;
	g_clients[ci].vl_lock.lock();
	vlc = g_clients[ci].view_list;
	g_clients[ci].vl_lock.unlock();

	for (auto target : new_view_list)
		if (0 == vlc.count(target)) {
			// 1. 새로 추가되는 객체
			SendPutPlayerPacket(ci, target); // 없으면 새로운 타겟을 보내줌
			vlc.insert(target); // 타켓의 정보를 뷰리스트에 추가
								// npc는 내정보를 안보내줘도됨
			if (true == Is_NPC(target)) {
				WakeUpNPC(target);
				continue;
			}
			g_clients[target].vl_lock.lock();

			if (0 != g_clients[target].view_list.count(ci)) {
				g_clients[target].vl_lock.unlock(); // 락을 미리미리 풀어 병렬성 확보
				SendPositionPacket(target, ci); // 내정보를 타켓에게 보내줌
			}
			else
			{
				g_clients[target].view_list.insert(ci);
				g_clients[target].vl_lock.unlock(); // 락을 미리미리 풀어 병렬성 확보
				SendPutPlayerPacket(target, ci);
				// 보내는걸로 끝나는게 아니라 상대방 뷰리스트에 내 정보를 넣어준다
			}
		}
		else {
			// 2. 변동없이 존재하는 객체
			// npc는 필요가 없어서 아무것도 해주면안된다
			if (true == Is_NPC(target))continue;
			g_clients[target].vl_lock.lock();
			if (0 != g_clients[target].view_list.count(ci)) {
				g_clients[target].vl_lock.unlock();
				SendPositionPacket(target, ci); // 움직인게 나니까 내 움직임을 타겟에게 보냄
			}
			else {
				g_clients[target].view_list.insert(ci);
				g_clients[target].vl_lock.unlock();
				SendPutPlayerPacket(target, ci); // 없을경우 풋플레이어패킷
			}
		}

		// 3. 시야에서 멀어진 객체
		std::unordered_set<int> removed_id_list;
		for (auto target : vlc) // 이걸로 변경해준다
			if (0 == new_view_list.count(target)) {
				// 시야에서 멀어졌으니 지워준다
				SendRemovePlayerPacket(ci, target);
				removed_id_list.insert(target);
				// 글로벌 공유자료라서 여러개가
				// 접근을 하기때문에 터진다. 그러므로 락을 걸어줘야 한다
				// 상대방에게도 내 정보를 지워줌
				// 이미 없을수도 있으니 그냥 지워주면 안되고 검사가 필요
				if (true == Is_NPC(target)) continue; // npc는 이런거 해줄필요가 없다
				g_clients[target].vl_lock.lock();
				if (0 != g_clients[target].view_list.count(ci)) {
					g_clients[target].view_list.erase(ci);
					g_clients[target].vl_lock.unlock();
					SendRemovePlayerPacket(target, ci);
				}
				else g_clients[target].vl_lock.unlock();
			}

		g_clients[ci].vl_lock.lock();
		for (auto p : vlc)
			g_clients[ci].view_list.insert(p); // 처리가 끝났으니 다시 돌려줌
		for (auto p : removed_id_list)
			g_clients[ci].view_list.erase(p);
		g_clients[ci].vl_lock.unlock();
}

void DisconnectClient(int ci)
{
	closesocket(g_clients[ci].client_socket);
	g_clients[ci].connect = false;
	std::unordered_set<int> lvl;

	g_clients[ci].vl_lock.lock();
	lvl = g_clients[ci].view_list;
	g_clients[ci].vl_lock.unlock();

	// 클라가 접속을 끊은경우, 이 클라를 보고있던 다른애들에게 뷰리스트 정보업뎃
	for (auto target : lvl)
	{
		g_clients[target].vl_lock.lock();
		if (0 != g_clients[target].view_list.count(ci)) 
		{
			g_clients[target].vl_lock.unlock();
			SendRemovePlayerPacket(target, ci);
		}
		else g_clients[target].vl_lock.unlock();
	}
	g_clients[ci].vl_lock.lock();
	g_clients[ci].view_list.clear();
	g_clients[ci].vl_lock.unlock();
}

//volatile int sum;

void NPC_Random_Move(int id)
{
	//   for (int i = 0; i < 10000; ++i) sum = sum + i; // cpu시간을 좀 잡아먹게끔 해줌, 10만으로 늘리면 렉이걸림. 풀어줘야함. 다음시간에~

	int x = g_clients[id].x;
	int y = g_clients[id].y;

	switch (rand() % 4) {
	case 0: if (x > 0)x--; break;
	case 1: if (y > 0) y--; break;
	case 2: if (x < BOARD_WIDTH - 2) x++; break;
	case 3: if (y < BOARD_HEIGHT - 2) y++; break;
	}

	g_clients[id].x = x;
	g_clients[id].y = y;

	// 주위에 있는 플레이어들에게 npc가 움직였다고 보내줌
	for (int i = 0; i < MAX_USER; ++i) {
		if (true == g_clients[i].connect) {
			g_clients[i].vl_lock.lock();
			if (0 != g_clients[i].view_list.count(id)) {
				if (true == Is_Close(i, id)) {
					g_clients[i].vl_lock.unlock();
					SendPositionPacket(i, id);
				}
				else {
					g_clients[i].view_list.erase(id);
					g_clients[i].vl_lock.unlock();
					SendRemovePlayerPacket(i, id); // npc가 시야에서 멀어지면 지워줌
				}
			}
			else {
				if (true == Is_Close(i, id)) {
					g_clients[i].view_list.insert(id);
					g_clients[i].vl_lock.unlock();
					SendPutPlayerPacket(i, id);
				}
				else {
					g_clients[i].vl_lock.unlock();
				}
			}
		}
	}

	for (int i = 0; i < MAX_USER; ++i) {
		if ((true == g_clients[i].connect) && (Is_Close(i, id))) {
			Timer_Event t = { id, high_resolution_clock::now() + 1s, E_MOVE };
			tq_lock.lock();
			timer_queue.push(t);
			tq_lock.unlock(); // 락을 걸어주고 큐에 넣어줘야 된다
			return;
		}
	}
	g_clients[id].is_active = false;
}

//void Heart_Beat(int id)
//{
//   NPC_Random_Move(id);
//}

void Worker_Thread()
{
	while (true) {
		DWORD io_size;
		unsigned long ci;
		OverlappedEx *over;
		BOOL ret = GetQueuedCompletionStatus(g_hiocp, &io_size, &ci,
			reinterpret_cast<LPWSAOVERLAPPED *>(&over), INFINITE);
		//      std::cout << "GQCS :";
		if (FALSE == ret) {
			int err_no = WSAGetLastError();
			if (64 == err_no)
				DisconnectClient(ci); // 클라이언트의 접속이 종료됨을 확인
			error_display("GQCS : ", WSAGetLastError()); // 클로즈소켓을 안해주고 끄면 여기서 에러가 난다
														 //         std::cout << "Error in GQCS\n";
		}
		if (0 == io_size) {
			DisconnectClient(ci); // 함수로 새로 만들어줌
			continue;
		}
		if (OP_RECV == over->event_type) {
			std::cout << "RECV from Client :" << ci;
			std::cout << "  IO_SIZE : " << io_size << std::endl;
			unsigned char *buf = g_clients[ci].recv_over.IOCP_buf;
			unsigned psize = g_clients[ci].curr_packet_size;
			unsigned pr_size = g_clients[ci].prev_packet_data;
			while (io_size > 0) {
				if (0 == psize) psize = buf[0];
				if (io_size + pr_size >= psize) {
					// 지금 패킷 완성 가능
					unsigned char packet[MAX_PACKET_SIZE];
					memcpy(packet, g_clients[ci].packet_buf, pr_size); // 뼈대를 작성하기 위함이지 사실 memcpy는 상당한 오버헤드를 불러온다
					memcpy(packet + pr_size, buf, psize - pr_size);
					ProcessPacket(static_cast<int>(ci), packet);
					io_size -= psize - pr_size;
					buf += psize - pr_size;
					psize = 0; pr_size = 0;
				}
				else {
					memcpy(g_clients[ci].packet_buf + pr_size, buf, io_size);
					pr_size += io_size;
					io_size = 0;
				}
			}
			g_clients[ci].curr_packet_size = psize;
			g_clients[ci].prev_packet_data = pr_size;
			DWORD recv_flag = 0;
			WSARecv(g_clients[ci].client_socket,
				&g_clients[ci].recv_over.wsabuf, 1,
				NULL, &recv_flag, &g_clients[ci].recv_over.over, NULL);
		}
		else if (OP_SEND == over->event_type) {
			if (io_size != over->wsabuf.len) {
				std::cout << "Send Incomplete Error!\n";
				closesocket(g_clients[ci].client_socket);
				g_clients[ci].connect = false;
			}
			delete over;
		}
		else if (OP_DO_AI == over->event_type) {
			NPC_Random_Move(ci); // 하트비트 대신 직접호출
			delete over;
		}
		else {
			std::cout << "Unknown GQCS event!\n";
			while (true);
		}
	}
}

void Accept_Thread()
{
	while (true) {
		SOCKADDR_IN ClientAddr;
		ZeroMemory(&ClientAddr, sizeof(SOCKADDR_IN));
		ClientAddr.sin_family = AF_INET;
		ClientAddr.sin_port = htons(MY_SERVER_PORT);
		ClientAddr.sin_addr.s_addr = INADDR_ANY;
		int addr_size = sizeof(ClientAddr);
		SOCKET new_client = WSAAccept(g_ssocket,
			reinterpret_cast<sockaddr *>(&ClientAddr), &addr_size, NULL, NULL);

		int new_id = -1;
		for (int i = 0; i < MAX_USER; ++i) // 검색범위를 바꿔줘야된다
			if (g_clients[i].connect == false) { new_id = i; break; }
		if (-1 == new_id) { std::cout << "MAX USER OVERFLOW!\n"; closesocket(new_client);  continue; }

		g_clients[new_id].connect = true;
		g_clients[new_id].client_socket = new_client;
		g_clients[new_id].curr_packet_size = 0;
		g_clients[new_id].prev_packet_data = 0;
		ZeroMemory(&g_clients[new_id].recv_over, sizeof(g_clients[new_id].recv_over));
		g_clients[new_id].recv_over.event_type = OP_RECV;
		g_clients[new_id].recv_over.wsabuf.buf =
			reinterpret_cast<CHAR *>(g_clients[new_id].recv_over.IOCP_buf);
		g_clients[new_id].recv_over.wsabuf.len = sizeof(g_clients[new_id].recv_over.IOCP_buf);
		g_clients[new_id].x = 4;
		g_clients[new_id].y = 4;

		DWORD recv_flag = 0;
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(new_client), g_hiocp, new_id, 0);
		WSARecv(new_client, &g_clients[new_id].recv_over.wsabuf, 1,
			NULL, &recv_flag, &g_clients[new_id].recv_over.over, NULL);
		SendPutPlayerPacket(new_id, new_id);

		std::unordered_set<int> local_view_list;

		for (int i = 0; i < MAX_USER; ++i) // 이것도 범위를 바꿔줘야됨. 밑에 npc용으로 따로 만듬
		{
			if (g_clients[i].connect == true)
			{
				if (new_id != i)
				{
					if (true == Is_Close(i, new_id)) { // 거리가 가까우면 보내줌
						SendPutPlayerPacket(new_id, i);
						local_view_list.insert(i);
						SendPutPlayerPacket(i, new_id);
						g_clients[new_id].view_list.insert(i); // 시야를 추가해줌
						g_clients[i].vl_lock.lock();
						g_clients[i].view_list.insert(new_id); // 상대방에게도 보내줌
						g_clients[i].vl_lock.unlock();
					}
				}
			}

			for (int i = NPC_START; i < NUM_OF_NPC; ++i) { // npc가 범위에 있을때 정보를 보내줘야하므로 이렇게 따로 만들어줌
				if (true == Is_Close(new_id, i))
				{
					WakeUpNPC(i);
					local_view_list.insert(i); // 주위에 npc가 있다면 뷰리스트에 넣어줌
					SendPutPlayerPacket(new_id, i); // 주위 npc정보를 접속한 유저에게 보내줌
				}
			}

			g_clients[new_id].vl_lock.lock();
			for (auto p : local_view_list) g_clients[new_id].view_list.insert(p);
			g_clients[new_id].vl_lock.unlock();
		}
	}
}

void NPC_AI_Thread()
{
	for (;;) {
		auto now = high_resolution_clock::now();
		for (int i = NPC_START; i < NUM_OF_NPC; ++i) {
			for (int j = 0; j < MAX_USER; ++j) {
				if (Is_Close(j, i)) { // 이 조건을 걸면 부하가 줄어들긴 하지만 바람직한건 아니다
					OverlappedEx *over = new OverlappedEx; // 할당을 받아서 넘겨줘야된다
					over->event_type = OP_DO_AI;
					PostQueuedCompletionStatus(g_hiocp, 1, i, &over->over); // 포스트큐컴플릿함수를 호출해 워커스레드에게 일을 떠넘긴다
					break;
				}
			}
		}
		auto du = high_resolution_clock::now() - now;
		int du_s = duration_cast<milliseconds>(du).count(); // 이 정보를 추가해주고 밑에 조건식을 넣어줌
		if (1000>du_s) Sleep(1000 - du_s); // 이렇게 조건식으로 바꿔준다
	}
}

void Timer_Thread()
{
	for (;;) {// 무한루프 돌려야되서 포문 2개
		Sleep(10); // 바로 시작하면 부하가 심해지니까 슬립으로 cpu낭비를 막아줌
		for (;;) {
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
			OverlappedEx *over = new OverlappedEx;
			if (E_MOVE == t.event) over->event_type = OP_DO_AI;
			PostQueuedCompletionStatus(g_hiocp, 1, t.object_id, &over->over);
		}
	}
}

void Shutdown_Server()
{
	closesocket(g_ssocket);
	CloseHandle(g_hiocp);
	WSACleanup();
}

int main()
{
	std::vector <std::thread *> worker_threads;

	Initialize_Server();
	for (int i = 0; i < 6; ++i)
		worker_threads.push_back(new std::thread{ Worker_Thread });

	//std::thread ai_thread{ NPC_AI_Thread };

	std::thread timer_thread{ Timer_Thread };

	std::thread accept_thread{ Accept_Thread };

	accept_thread.join();

	timer_thread.join();
	for (auto pth : worker_threads) {
		pth->join();
		delete pth;
	}
	Shutdown_Server();
}