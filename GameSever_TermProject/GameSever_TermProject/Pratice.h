#include <WinSock2.h>
#include <winsock.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set> // ��� �̰� ���ִ°� �ξ�����. ������ �ξ� ����
// map�� ������ ������� ������ �������� ���ִ°� ����
#include <mutex> // ���� �ɾ��ֱ� ���� ���ؽ�����
#include <chrono> // npc�� ������ ������ �ð��� �־��ֱ� ���� ����
#include <queue> // �̺�Ʈť�� �����ϱ� ���� ����

#include "protocol.h"

HANDLE g_hiocp;
SOCKET g_ssocket;

enum OPTYPE { OP_SEND, OP_RECV, OP_DO_AI }; // ��Ŀ�����忡�� � �̺�Ʈ�� �����϶�� �˷������� ����
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
	std::chrono::high_resolution_clock::time_point last_move_time; // npc�� �����ӽð� ���� + ���ǵ��� ����
	bool is_active; // npc�� ���� �������� �ȿ������� �Ǵ��ϱ� ����
	bool connect;
	SOCKET client_socket;
	OverlappedEx recv_over;
	unsigned char packet_buf[MAX_PACKET_SIZE];
	int prev_packet_data;
	int curr_packet_size;
	std::unordered_set<int> view_list; // ���Ʈ�Ҷ� �������ش�
	std::mutex vl_lock;
};

struct Timer_Event {
	int object_id;
	high_resolution_clock::time_point exec_time; // �� �̺�Ʈ�� ���� ����Ǿ� �ϴ°�
	Event_Type event; // ������ Ȯ���ϸ� ����ܿ� �ٸ��͵��� �߰��� ��
};

class comparison {
	bool reverse;
public:
	comparison() {}
	bool operator() (const Timer_Event first, const Timer_Event second) const {
		return first.exec_time > second.exec_time;
	}
};

priority_queue <Timer_Event, vector<Timer_Event>, comparison> timer_queue; // �켱����ť, �׳� ���� �����ϱ� ���� �ɾ������
mutex tq_lock; // �켱���� ť ��� ���� ��

CLIENT g_clients[NUM_OF_NPC]; // npc���� ���� �ֵ��� ����� �������

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
	std::wcout << L"����" << lpMsgBuf << std::endl;
	LocalFree(lpMsgBuf);
	while (true);
}

bool Is_Close(int from, int to) // �Ÿ��� ������ �˻��ϱ� ���� �Լ�
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

void Initialize_NPC() // npc �ʱ�ȭ �Լ�
{
	for (int i = NPC_START; i < NUM_OF_NPC; ++i) {
		g_clients[i].connect = false;
		g_clients[i].x = rand() % BOARD_WIDTH; // ���ʿ� �ѷ������ �ȱ׷��� ��� npc�� ���Ĺ���
		g_clients[i].y = rand() % BOARD_HEIGHT; // ��������
												// �丮��Ʈ�� �ִ� ������ ���ɰ� ���� �����ֱ� ������. �ٵ� npc�� �ʿ䰡 ����
												// �̷��� ������ false�� ���ְ� ���� �ѷ��ָ� �ʱ�ȭ ��
		g_clients[i].last_move_time = std::chrono::high_resolution_clock::now(); // ��Ʈ ���� Ÿ�� �ʱ�ȭ
		g_clients[i].is_active = false;
	}
}

void WakeUpNPC(int id)
{
	// �̹� ������ �ָ� �� �����̸� �ȵǹǷ� ���°� �ʿ���
	if (true == g_clients[id].is_active) return;
	g_clients[id].is_active = true;
	Timer_Event event = { id, high_resolution_clock::now() + 1s ,E_MOVE };
	tq_lock.lock();
	timer_queue.push(event);
	tq_lock.unlock();
}

void Initialize_Server()
{
	// ���� �ʱ�ȭ�� �ϸ鼭 npc�� �ʱ�ȭ�Ѥ� ���ش�
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

	std::unordered_set<int> new_view_list; // �̵�ó���� ���� ���� �������
	
	for (int i = 0; i < MAX_USER; ++i)
		if (true == g_clients[i].connect)
			if (i != ci)
				if (true == Is_Close(ci, i))
					new_view_list.insert(i); // �ڱ��ڽ��� �־��ָ� �ȵȴ�

											 // npc�� �̵��Ҷ� ������ ���� �־���ߵȴ�
	for (int i = NPC_START; i < NUM_OF_NPC; ++i)
		if (true == Is_Close(ci, i)) new_view_list.insert(i);

	// ī�Ǹ� �Ҷ� ���� �ɾ������
	std::unordered_set<int> vlc;
	g_clients[ci].vl_lock.lock();
	vlc = g_clients[ci].view_list;
	g_clients[ci].vl_lock.unlock();

	for (auto target : new_view_list)
		if (0 == vlc.count(target)) {
			// 1. ���� �߰��Ǵ� ��ü
			SendPutPlayerPacket(ci, target); // ������ ���ο� Ÿ���� ������
			vlc.insert(target); // Ÿ���� ������ �丮��Ʈ�� �߰�
								// npc�� �������� �Ⱥ����൵��
			if (true == Is_NPC(target)) {
				WakeUpNPC(target);
				continue;
			}
			g_clients[target].vl_lock.lock();

			if (0 != g_clients[target].view_list.count(ci)) {
				g_clients[target].vl_lock.unlock(); // ���� �̸��̸� Ǯ�� ���ļ� Ȯ��
				SendPositionPacket(target, ci); // �������� Ÿ�Ͽ��� ������
			}
			else
			{
				g_clients[target].view_list.insert(ci);
				g_clients[target].vl_lock.unlock(); // ���� �̸��̸� Ǯ�� ���ļ� Ȯ��
				SendPutPlayerPacket(target, ci);
				// �����°ɷ� �����°� �ƴ϶� ���� �丮��Ʈ�� �� ������ �־��ش�
			}
		}
		else {
			// 2. �������� �����ϴ� ��ü
			// npc�� �ʿ䰡 ��� �ƹ��͵� ���ָ�ȵȴ�
			if (true == Is_NPC(target))continue;
			g_clients[target].vl_lock.lock();
			if (0 != g_clients[target].view_list.count(ci)) {
				g_clients[target].vl_lock.unlock();
				SendPositionPacket(target, ci); // �����ΰ� ���ϱ� �� �������� Ÿ�ٿ��� ����
			}
			else {
				g_clients[target].view_list.insert(ci);
				g_clients[target].vl_lock.unlock();
				SendPutPlayerPacket(target, ci); // ������� ǲ�÷��̾���Ŷ
			}
		}

		// 3. �þ߿��� �־��� ��ü
		std::unordered_set<int> removed_id_list;
		for (auto target : vlc) // �̰ɷ� �������ش�
			if (0 == new_view_list.count(target)) {
				// �þ߿��� �־������� �����ش�
				SendRemovePlayerPacket(ci, target);
				removed_id_list.insert(target);
				// �۷ι� �����ڷ�� ��������
				// ������ �ϱ⶧���� ������. �׷��Ƿ� ���� �ɾ���� �Ѵ�
				// ���濡�Ե� �� ������ ������
				// �̹� �������� ������ �׳� �����ָ� �ȵǰ� �˻簡 �ʿ�
				if (true == Is_NPC(target)) continue; // npc�� �̷��� �����ʿ䰡 ����
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
			g_clients[ci].view_list.insert(p); // ó���� �������� �ٽ� ������
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

	// Ŭ�� ������ �������, �� Ŭ�� �����ִ� �ٸ��ֵ鿡�� �丮��Ʈ ��������
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
	//   for (int i = 0; i < 10000; ++i) sum = sum + i; // cpu�ð��� �� ��Ƹ԰Բ� ����, 10������ �ø��� ���̰ɸ�. Ǯ�������. �����ð���~

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

	// ������ �ִ� �÷��̾�鿡�� npc�� �������ٰ� ������
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
					SendRemovePlayerPacket(i, id); // npc�� �þ߿��� �־����� ������
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
			tq_lock.unlock(); // ���� �ɾ��ְ� ť�� �־���� �ȴ�
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
				DisconnectClient(ci); // Ŭ���̾�Ʈ�� ������ ������� Ȯ��
			error_display("GQCS : ", WSAGetLastError()); // Ŭ��������� �����ְ� ���� ���⼭ ������ ����
														 //         std::cout << "Error in GQCS\n";
		}
		if (0 == io_size) {
			DisconnectClient(ci); // �Լ��� ���� �������
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
					// ���� ��Ŷ �ϼ� ����
					unsigned char packet[MAX_PACKET_SIZE];
					memcpy(packet, g_clients[ci].packet_buf, pr_size); // ���븦 �ۼ��ϱ� �������� ��� memcpy�� ����� ������带 �ҷ��´�
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
			NPC_Random_Move(ci); // ��Ʈ��Ʈ ��� ����ȣ��
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
		for (int i = 0; i < MAX_USER; ++i) // �˻������� �ٲ���ߵȴ�
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

		for (int i = 0; i < MAX_USER; ++i) // �̰͵� ������ �ٲ���ߵ�. �ؿ� npc������ ���� ����
		{
			if (g_clients[i].connect == true)
			{
				if (new_id != i)
				{
					if (true == Is_Close(i, new_id)) { // �Ÿ��� ������ ������
						SendPutPlayerPacket(new_id, i);
						local_view_list.insert(i);
						SendPutPlayerPacket(i, new_id);
						g_clients[new_id].view_list.insert(i); // �þ߸� �߰�����
						g_clients[i].vl_lock.lock();
						g_clients[i].view_list.insert(new_id); // ���濡�Ե� ������
						g_clients[i].vl_lock.unlock();
					}
				}
			}

			for (int i = NPC_START; i < NUM_OF_NPC; ++i) { // npc�� ������ ������ ������ ��������ϹǷ� �̷��� ���� �������
				if (true == Is_Close(new_id, i))
				{
					WakeUpNPC(i);
					local_view_list.insert(i); // ������ npc�� �ִٸ� �丮��Ʈ�� �־���
					SendPutPlayerPacket(new_id, i); // ���� npc������ ������ �������� ������
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
				if (Is_Close(j, i)) { // �� ������ �ɸ� ���ϰ� �پ��� ������ �ٶ����Ѱ� �ƴϴ�
					OverlappedEx *over = new OverlappedEx; // �Ҵ��� �޾Ƽ� �Ѱ���ߵȴ�
					over->event_type = OP_DO_AI;
					PostQueuedCompletionStatus(g_hiocp, 1, i, &over->over); // ����Ʈť���ø��Լ��� ȣ���� ��Ŀ�����忡�� ���� ���ѱ��
					break;
				}
			}
		}
		auto du = high_resolution_clock::now() - now;
		int du_s = duration_cast<milliseconds>(du).count(); // �� ������ �߰����ְ� �ؿ� ���ǽ��� �־���
		if (1000>du_s) Sleep(1000 - du_s); // �̷��� ���ǽ����� �ٲ��ش�
	}
}

void Timer_Thread()
{
	for (;;) {// ���ѷ��� �����ߵǼ� ���� 2��
		Sleep(10); // �ٷ� �����ϸ� ���ϰ� �������ϱ� �������� cpu���� ������
		for (;;) {
			tq_lock.lock();
			if (0 == timer_queue.size()) {
				tq_lock.unlock();
				break;
			} // ť�� ��������� ������ �ȵǴϱ�
			Timer_Event t = timer_queue.top(); // ���� �̺�Ʈ �߿� ����ð��� ���� �ֱ��� �̺�Ʈ�� �����ؾ� �ϹǷ� �켱���� ť�� ����
			if (t.exec_time > high_resolution_clock::now()) {
				tq_lock.unlock();
				break; // ����ð����� ũ�ٸ�, ��ٷ���
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