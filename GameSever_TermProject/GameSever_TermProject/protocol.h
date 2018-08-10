
#define MAX_BUFF_SIZE   4000
#define MAX_PACKET_SIZE  255

#define BOARD_WIDTH   400
#define BOARD_HEIGHT  400

#define VIEW_RADIUS   8

#define MAX_USER 500

#define NPC_START  500
#define NUM_OF_NPC  4000

#define MY_SERVER_PORT  7000

#define MAX_STR_SIZE  100

#define CS_UP		1
#define CS_DOWN		2
#define CS_LEFT		3
#define CS_RIGHT    4
#define CS_HIT		5
#define CS_SCENE	6
#define CS_ATTACK	7

#define SC_POS           1
#define SC_PUT_PLAYER    2
#define SC_REMOVE_PLAYER 3
#define SC_HIT			 4
#define SC_BASIC_DATA	 5	
#define SC_NPC_DEAD		 6
#define SC_LEVELUP		 7
#define SC_HP_RECOVRY	 8
#define SC_USER_DEAD	 9

#define SC_CONNECT_FAIL		10

enum CharZone
{TOWN, LowZone1,LowZone2, MediumZone1, MediumZone2,
	HighZone1,HighZone2,HighZone3,FRaidZone};


struct vec2 {
	int x, y;
	int GetX() { return x; }
	int GetY() { return y; }
	vec2 GetVec2() { return *this; }
};

#pragma pack (push, 1)

struct cs_packet_up {
	BYTE size;
	BYTE type;
};

struct cs_packet_down {
	BYTE size;
	BYTE type;
};

struct cs_packet_left {
	BYTE size;
	BYTE type;
};

struct cs_packet_right {
	BYTE size;
	BYTE type;
};

struct cs_packet_chat {
	BYTE size;
	BYTE type;
	WCHAR message[MAX_STR_SIZE];
};

struct cs_packet_scene {
	BYTE size;
	BYTE type;
	int scene;			//이동해야 하는씬
};

struct sc_packet_pos {
	BYTE size;
	BYTE type;
	INT id;
	INT x;			//원래 BYTE 밑에도
	INT y;
};

struct sc_packet_put_player {
	BYTE size;
	BYTE type;
	INT id;
	INT x;
	INT y;
	INT zone;
};
struct sc_packet_remove_player {
	BYTE size;
	BYTE type;
	INT id;
};

struct sc_packet_basicdata {
	BYTE size;
	BYTE type;
	INT id;
	INT hp;
	INT maxhp;
	INT attack;
	INT level;
};
struct sc_packet_levelup {
	BYTE size;
	BYTE type;
	int maxhp;
};
struct sc_packet_hp_recovery {
	BYTE size;
	BYTE type;
	int currentHP;
};
struct sc_packet_user_dead {
	BYTE size;
	BYTE type;
	int id;
	int x;
	int y;
	int hp;
};

struct sc_packet_hit {
	BYTE size;
	BYTE type;
	WORD id;
	INT hp;
	char message[MAX_STR_SIZE];
};
struct sc_packet_connectfail {
	BYTE size;
	BYTE type;
};

struct cs_packet_attack {
	BYTE size;
	BYTE type;
};


struct sc_packet_npcdead {
	BYTE size;
	BYTE type;
	INT id;
};

#pragma pack (pop)