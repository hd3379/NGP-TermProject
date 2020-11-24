#pragma comment(lib, "ws2_32")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>

#define SERVER_PORT 9000

#define MAX_ENEMY_BULLET 2000
#define MAX_PLAYER_BULLET 50
#define MAX_PLAYER 2

enum class GAME_STATE
{
	TITLE,
	RUNNING,
	END,
};

typedef struct Position
{
	float x, y;
};

typedef struct Player
{
	int number;
	int hp;
	bool is_click;
	Position pos;
	Position bullets[MAX_PLAYER_BULLET];
};

struct Enemy
{
	int hp;
	Position pos;
	Position bullets[MAX_ENEMY_BULLET];
};

class CBullet {
public:

	//각속도
	float AngleRate;
	//속도
	float Speed;
	//가속도
	float SpeedRate;

	float X, Y;
	float Angle;
	bool Alive;

	int ID;

	CBullet() {
		Alive = false;
	}
	CBullet(float x, float y, float angle, float angle_rate, float speed, float speed_rate) {
		X = x; Y = y;	AngleRate = angle_rate;
		Angle = angle;	Speed = speed;
		SpeedRate = speed_rate;
		Alive = TRUE;
	};

	void Move
	(RECT rc);

	~CBullet() {}
};
void CBullet::Move(RECT rc) {
	float rad = Angle * 3.14f / 180.0f;

	//각도와 속도를 사용해서 좌표갱신
	X += Speed * cosf(rad);
	Y += Speed * sinf(rad);

	//각도에 각속도 가산
	Angle += AngleRate;

	//속도에 가속도 가산
	Speed += SpeedRate;

	//화면밖에 나가면 탄삭제
	if (X >= rc.right || X < 0 || Y >= rc.bottom || Y < 0) {
		Alive = false;
	}
}

Enemy enemy;
Player players[MAX_PLAYER];
CBullet PlayersBullet[MAX_PLAYER][MAX_PLAYER_BULLET];
CBullet EnemyBullet[MAX_ENEMY_BULLET];
GAME_STATE curr_state;
int num_player;
int round_count;
int PlayerBulletNum[MAX_PLAYER];
int EnemyBulletNum;
RECT rect;
int win;
bool Dondead;
int imsiDondead;
int EnemyXMove;
int Time;

void error_display(const char* message);
void error_quit(const char* message);
int recvn(SOCKET socket, char* buffer, int length, int flags);

void InitalizeGameData();
void Update();
void UpdateGameState();
void CollisionCheck();
void SendLogo(SOCKET socket);
void SendEnding(SOCKET socket);

unsigned WINAPI ProcessClient(LPVOID arg);

int main()
{
	int retval;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == INVALID_SOCKET)
		error_quit("socket()");

	SOCKADDR_IN server_address;
	ZeroMemory(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(SERVER_PORT);

	retval = bind(listen_socket, (SOCKADDR*)&server_address, sizeof(server_address));
	if (retval == SOCKET_ERROR)
		error_quit("bind()");

	retval = listen(listen_socket, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		error_quit("listen()");

	SOCKET client_socket;
	SOCKADDR_IN client_address;
	int client_address_length;
	HANDLE thread;

	InitalizeGameData();

	while (true)
	{
		client_address_length = sizeof(client_address);

		client_socket = accept(listen_socket, (SOCKADDR*)&client_address, &client_address_length);
		if (client_socket == INVALID_SOCKET)
		{
			error_display("accept()");
			break;
		}

		printf("[TCP 서버] 클라이언트가 접속했습니다.\n IP 주소 : %s, 포트 번호 : %d\n",
			inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

		thread = (HANDLE)_beginthreadex(NULL, 0, ProcessClient, (LPVOID)client_socket, 0, NULL);
		if (thread == NULL)
			closesocket(client_socket);
		else
			CloseHandle(thread);
	}

	closesocket(listen_socket);

	WSACleanup();
	return 0;
}

void error_display(const char* message)
{
	LPVOID message_buffer;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&message_buffer, 0, NULL);

	printf("[%s] %s", message, (char*)message_buffer);
	LocalFree(message_buffer);
}

void error_quit(const char* message)
{
	LPVOID message_buffer;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&message_buffer, 0, NULL);

	MessageBox(NULL, (LPCTSTR)message_buffer, message, MB_ICONERROR);
	LocalFree(message_buffer);
	exit(1);
}

int recvn(SOCKET socket, char* buffer, int length, int flags)
{
	int received = 0;
	char* ptr = buffer;
	int left = length;

	while (left > 0)
	{
		received = recv(socket, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;

		left -= received;
		ptr += received;
	}

	return (length - left);
}

unsigned WINAPI ProcessClient(LPVOID arg)
{
	SOCKET client_socket = (SOCKET)arg;
	SOCKADDR_IN client_address;
	int client_address_length;
	int received_data_length = 0;

	client_address_length = sizeof(client_address);
	getpeername(client_socket, (SOCKADDR*)&client_address, &client_address_length);

	while (true)
	{
		UpdateGameState();
		//SendGameState();
		switch (curr_state)
		{
		case GAME_STATE::TITLE:
			SendLogo(client_socket);
			break;
		case GAME_STATE::RUNNING:
			//RecvPlayerInfo();
			Update();
			CollisionCheck();
			//SendAllPlayerInfo();
			//SendEnemyInfo();
			break;
		case GAME_STATE::END:
			SendEnding(client_socket);
			break;
		default:
			break;
		}

		printf("\n[TCP 서버] 클라이언트 종료. IP 주소 - %s, 포트 번호 - %d\n",
			inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
		break;
	}

	return 0;
}

void SendLogo(SOCKET socket)
{
	send(socket, (char*)num_player, sizeof(int), 0);
}

void SendEnding(SOCKET socket)
{
	send(socket, (char*)win, sizeof(int), 0); // win이 1이면 승, -1이면 게임오버
}


void InitalizeGameData()
{
	curr_state = GAME_STATE::TITLE;
	num_player = 0;
	round_count = 1;
	EnemyXMove = 0;
	win = 0;
	Time = 0;
	Dondead = 0;
	imsiDondead = 0;
	enemy.hp = 250;
	EnemyXMove = 3;
	enemy.pos.x = 100;
	enemy.pos.y = 105;

	enemy.hp = 200;
	enemy.pos = { 100.0f, 105.f };

	rect.left = 0;
	rect.right = 600;
	rect.top = 0;
	rect.bottom = 800;
	for (auto bullet : enemy.bullets)
		bullet = { 10000.0f, -10000.0f };

	for (auto player : players)
	{
		player.number = -1;
		player.hp = 3;
		player.is_click = false;
		player.pos = { -10000.0f, -10000.0f };

		for (auto bullet : player.bullets)
			bullet = { 10000.0f, -10000.0f };
	}

	EnemyBulletNum = 0;
	for (int i = 0; i < MAX_PLAYER; i++)
		PlayerBulletNum[i] = 0;
}

void Update()
{

//움직임 업데이트
	if (round_count == 1) {
		enemy.pos.x += EnemyXMove;
		if (enemy.pos.x - 30 <= rect.left || enemy.pos.x + 30 >= rect.right) {
			EnemyXMove *= -1;
			enemy.pos.x += EnemyXMove;
		}
	}
	for (int i = 0; i < 2000; i++) {
		if (EnemyBullet[i].Alive == TRUE)
			EnemyBullet[i].Move(rect);
	}
	for (int i = 0; i < MAX_PLAYER; i++) {
		for (int j = 0; j < MAX_PLAYER_BULLET; i++) {
			if (PlayersBullet[i][j].Alive == TRUE)
				PlayersBullet[i][j].Move(rect);
		}
	}

	for (int i = 0; i < MAX_PLAYER; i++) {
		if (players[i].is_click == 1) {
			PlayerBulletNum[i]++;
			if (PlayerBulletNum[i] >= 20)
				PlayerBulletNum[i] = 0;
			PlayersBullet[i][PlayerBulletNum[i]].Alive = TRUE;
			PlayersBullet[i][PlayerBulletNum[i]].X = players[i].pos.x;
			PlayersBullet[i][PlayerBulletNum[i]].Y = players[i].pos.y;
			PlayersBullet[i][PlayerBulletNum[i]].Speed = 20;
			PlayersBullet[i][PlayerBulletNum[i]].SpeedRate = 0;
			PlayersBullet[i][PlayerBulletNum[i]].Angle = -90;
			PlayersBullet[i][PlayerBulletNum[i]].AngleRate = 0;
		}
	}

	static bool butterfly;
	static char pattern[10][20] = {
		"         #         ",
		"   ###  # #  ###   ",
		"  #   #     #   #  ",
		" #    #     #    # ",
		"#   # #     #  #  #",
		" #    #     #    # ",
		"  #             #  ",
		"   #############   ",
		"      #     #      ",
		"      #     #      "
	};
	//시간별탄막
	Time += 1;
	if (round_count == 1) {
		if (Time < 30) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 20;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = 90;
			EnemyBullet[EnemyBulletNum].AngleRate = 0;
		}
		else if (Time < 40) {}
		else if (Time < 70) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 20;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = 90;
			EnemyBullet[EnemyBulletNum].AngleRate = 0;
		}
		else if (Time < 80) {}
		else if (Time < 110) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 20;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = 90;
			EnemyBullet[EnemyBulletNum].AngleRate = 0;
		}
		else if (Time < 120) {}
		else if (Time < 160) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 20;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = 90;
			EnemyBullet[EnemyBulletNum].AngleRate = 0;
		}
		else if (Time < 180) {}
		else if (Time < 260) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 20;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = Time * 3 - 180;
			EnemyBullet[EnemyBulletNum].AngleRate = 0;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 20;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = Time * 3;
			EnemyBullet[EnemyBulletNum].AngleRate = 0;
		}
		else if (Time < 270) {}
		else if (Time < 400) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 6;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = Time * 8;
			EnemyBullet[EnemyBulletNum].AngleRate = 1;
		}
		else if (Time < 600) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 6;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = -Time * 8;
			EnemyBullet[EnemyBulletNum].AngleRate = -1;
		}
		else if (Time < 610) {}
		else if (Time < 800) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 6;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = Time * 8;
			EnemyBullet[EnemyBulletNum].AngleRate = 1;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 6;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = -Time * 8;
			EnemyBullet[EnemyBulletNum].AngleRate = -1;
		}
		else if (Time < 867) {
			EnemyXMove = 0;
			static int enemxplus = 0;
			static int enemyplus = 0;
			if (Time < 806) {
				enemxplus = 20;
				enemyplus = 20;
			}
			if (Time < 821) {
				enemy.pos.x += enemxplus;
				enemy.pos.y += enemyplus;
			}
			else if (Time < 836) {
				enemy.pos.x -= enemxplus;
				enemy.pos.y -= enemyplus;
			}
			else if (Time < 837) {
				enemxplus = -20;
				enemyplus = 20;
			}
			else if (Time < 854) {
				enemy.pos.x += enemxplus;
				enemy.pos.y += enemyplus;
			}
			else if (Time < 867) {
				enemy.pos.x -= enemxplus;
				enemy.pos.y -= enemyplus;
			}
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 3;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = 90;
			EnemyBullet[EnemyBulletNum].AngleRate = 0;
		}
		else if (Time < 890) {}
		else if (Time < 2000) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = rand() % 20 + 2;
			EnemyBullet[EnemyBulletNum].SpeedRate = rand() % 3;
			EnemyBullet[EnemyBulletNum].Angle = rand() % 360;
			EnemyBullet[EnemyBulletNum].AngleRate = rand() % 5;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = rand() % 20 + 2;
			EnemyBullet[EnemyBulletNum].SpeedRate = rand() % 5;
			EnemyBullet[EnemyBulletNum].Angle = rand() % 360;
			EnemyBullet[EnemyBulletNum].AngleRate = rand() % 5;
		}
	}
	else if (round_count == 2) {
		if (Time < 20) {}
		else if (Time < 160) {
			if (Time > 20) {
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = 200;
				EnemyBullet[EnemyBulletNum].Y = 200;
				EnemyBullet[EnemyBulletNum].Speed = 2;
				EnemyBullet[EnemyBulletNum].SpeedRate = 1;
				EnemyBullet[EnemyBulletNum].Angle = 90;
				EnemyBullet[EnemyBulletNum].AngleRate = 15;
			}
			if (Time > 50) {
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = 400;
				EnemyBullet[EnemyBulletNum].Y = 200;
				EnemyBullet[EnemyBulletNum].Speed = 2;
				EnemyBullet[EnemyBulletNum].SpeedRate = 1;
				EnemyBullet[EnemyBulletNum].Angle = 90;
				EnemyBullet[EnemyBulletNum].AngleRate = -15;
			}
			if (Time > 80) {
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = 200;
				EnemyBullet[EnemyBulletNum].Y = 600;
				EnemyBullet[EnemyBulletNum].Speed = 2;
				EnemyBullet[EnemyBulletNum].SpeedRate = 1;
				EnemyBullet[EnemyBulletNum].Angle = 0;
				EnemyBullet[EnemyBulletNum].AngleRate = 15;
			}
			if (Time > 120) {
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = 400;
				EnemyBullet[EnemyBulletNum].Y = 600;
				EnemyBullet[EnemyBulletNum].Speed = 2;
				EnemyBullet[EnemyBulletNum].SpeedRate = 1;
				EnemyBullet[EnemyBulletNum].Angle = 90;
				EnemyBullet[EnemyBulletNum].AngleRate = -15;
			}
		}
		else if (Time < 250) {}
		else if (Time < 500) {
			butterfly = 1;
			if (Time % 2 == 0) {
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = 50;
				EnemyBullet[EnemyBulletNum].Y = rand() % 800 + 50;
				EnemyBullet[EnemyBulletNum].Speed = 5;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = 0;
				EnemyBullet[EnemyBulletNum].AngleRate = 0;
			}
		}
		else if (Time < 600) {}
		else if (Time < 900) {
			butterfly = 0;
			if (Time % 3 == 0) {
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 5;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6;
				EnemyBullet[EnemyBulletNum].AngleRate = -1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 90;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 90;
				EnemyBullet[EnemyBulletNum].AngleRate = -1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 180;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 180;
				EnemyBullet[EnemyBulletNum].AngleRate = -1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 270;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= 2000)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 270;
				EnemyBullet[EnemyBulletNum].AngleRate = -1;
			}
		}
		else if (Time < 920) {}
		else if (Time < 930) {
			static int i = 9;
			for (int j = 19; j >= 0; j--) {
				if (pattern[i][j] == '#') {
					EnemyBulletNum++;
					if (EnemyBulletNum >= 2000)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = 100 + j * 10;
					EnemyBullet[EnemyBulletNum].Y = 100;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = 90;
					EnemyBullet[EnemyBulletNum].AngleRate = 0;
				}
			}
			i--;
		}
		else if (Time < 940) {
			static int i = 9;
			for (int j = 19; j >= 0; j--) {
				if (pattern[i][j] == '#') {
					EnemyBulletNum++;
					if (EnemyBulletNum >= 2000)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = 500 + j * 10;
					EnemyBullet[EnemyBulletNum].Y = 100;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = 90;
					EnemyBullet[EnemyBulletNum].AngleRate = 0;
				}
			}
			i--;
		}
		else if (Time < 950) {
			static int i = 9;
			for (int j = 19; j >= 0; j--) {
				if (pattern[i][j] == '#') {
					EnemyBulletNum++;
					if (EnemyBulletNum >= 2000)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = 100 + j * 10;
					EnemyBullet[EnemyBulletNum].Y = 100;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = 90;
					EnemyBullet[EnemyBulletNum].AngleRate = 0;
				}
			}
			i--;
		}
		else if (Time < 960) {
			static int i = 9;
			for (int j = 19; j >= 0; j--) {
				if (pattern[i][j] == '#') {
					EnemyBulletNum++;
					if (EnemyBulletNum >= 2000)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = 300 + j * 10;
					EnemyBullet[EnemyBulletNum].Y = 100;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = 90;
					EnemyBullet[EnemyBulletNum].AngleRate = 0;
				}
			}
			i--;
		}
		else if (Time < 970) {}
		else if (Time < 2000) {
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 10;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = Time * 6;
			EnemyBullet[EnemyBulletNum].AngleRate = 1;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 10;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = -Time * 6;
			EnemyBullet[EnemyBulletNum].AngleRate = -1;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 10;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 90;
			EnemyBullet[EnemyBulletNum].AngleRate = 1;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 10;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = -Time * 6 - 90;
			EnemyBullet[EnemyBulletNum].AngleRate = -1;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 10;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 180;
			EnemyBullet[EnemyBulletNum].AngleRate = 1;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 10;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = -Time * 6 - 180;
			EnemyBullet[EnemyBulletNum].AngleRate = -1;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 10;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 270;
			EnemyBullet[EnemyBulletNum].AngleRate = 1;
			EnemyBulletNum++;
			if (EnemyBulletNum >= 2000)
				EnemyBulletNum = 0;
			EnemyBullet[EnemyBulletNum].Alive = TRUE;
			EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
			EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
			EnemyBullet[EnemyBulletNum].Speed = 10;
			EnemyBullet[EnemyBulletNum].SpeedRate = 0;
			EnemyBullet[EnemyBulletNum].Angle = -Time * 6 - 270;
			EnemyBullet[EnemyBulletNum].AngleRate = -1;
		}
	}

//정보 넘겨주기 위한 변수에 정리
	for (int i = 0; i < MAX_PLAYER; i++)
	{
		for (int j = 0; j < PlayerBulletNum[i]; j++)
		{
			players[i].bullets[j].x = PlayersBullet[i][j].X;
			players[i].bullets[j].y = PlayersBullet[i][j].Y;
		}
	}
	for (int j = 0; j < EnemyBulletNum; j++) 
	{
		enemy.bullets[j].x = EnemyBullet[j].X;
		enemy.bullets[j].y = EnemyBullet[j].Y;
	}
}

void CollisionCheck()
{
	for (int i = 0; i < MAX_PLAYER; i++) {
		for (int j = 0; j < MAX_PLAYER_BULLET; i++) {
			if (PlayersBullet[i][j].Alive == TRUE) {
				if (round_count == 1) {
					if ((PlayersBullet[i][j].X > enemy.pos.x - 45) && (PlayersBullet[i][j].X < enemy.pos.x + 45) && 
						(PlayersBullet[i][j].Y > enemy.pos.y - 75) && (PlayersBullet[i][j].Y < enemy.pos.y + 60)) {
						enemy.hp--;
						if (enemy.hp < 0) {
							round_count = 2;
							EnemyBulletNum = 0;
							enemy.hp = 420;
							enemy.pos.x = 310;
							enemy.pos.y = 100;
							Time = 0;
							for (int i = 0; i < 2000; i++) {
								EnemyBullet[i].Alive = false;
							}
						}
						PlayersBullet[i][j].Alive = false;
					}
				}
				else if (round_count == 2) {
					if ((PlayersBullet[i][j].X > enemy.pos.x - 45) && (PlayersBullet[i][j].X < enemy.pos.x + 45) && (PlayersBullet[i][j].Y > enemy.pos.y - 75) && (PlayersBullet[i][j].Y < enemy.pos.y + 60)) {
						enemy.hp--;
						if (enemy.hp < 0) {
							win = 1;
							EnemyBulletNum = 0;
							Time = 0;
							for (int i = 0; i < 2000; i++) {
								EnemyBullet[i].Alive = false;
							}
						}
						PlayersBullet[i][j].Alive = false;
					}
				}
			}
		}
	}

	for (int i = 0; i < 2000; i++) {
		if (EnemyBullet[i].Alive == TRUE) {
			for (int j = 0; j < MAX_PLAYER; j++) {
				if (sqrt(pow(players[j].pos.x - EnemyBullet[i].X, 2) + pow(players[j].pos.y - EnemyBullet[i].Y, 2)) < 26) {
					if (Dondead == 0) {
						if (imsiDondead == 0) {
							players[j].hp--;
							if (players[j].hp <= 0)
								win = -1;
							else
							{
								imsiDondead = 1;
							}
							EnemyBullet[i].Alive = false;
						}
					}
				}
			}
		}
	}
	if (imsiDondead != 0) {
		imsiDondead++;
		if (imsiDondead >= 10)
			imsiDondead = 0;
	}
}

void UpdateGameState()
{
	if (num_player == 2)
		curr_state = GAME_STATE::RUNNING;

	if (enemy.hp <= 0)
		curr_state = GAME_STATE::END;

	for (const auto& player : players)
		if (player.hp < 0)
			curr_state = GAME_STATE::END;
}
