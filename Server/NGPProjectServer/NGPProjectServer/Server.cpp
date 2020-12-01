#pragma comment(lib, "ws2_32")
#pragma comment(lib,"winmm.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <Windows.h>
#include <time.h>
#include <math.h>

#define SERVER_PORT 9000

#define MAX_ENEMY_BULLET 2000
#define MAX_PLAYER_BULLET 30
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
	bool is_ready;
	Position pos;
	Position bullets[MAX_PLAYER_BULLET];
};

typedef struct Enemy
{
	int hp;
	bool Shootbutterfly;
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

	void Move();

	~CBullet() {}
};
void CBullet::Move() {
	float rad = Angle * 3.14f / 180.0f;

	//각도와 속도를 사용해서 좌표갱신
	X += Speed * cosf(rad);
	Y += Speed * sinf(rad);

	//각도에 각속도 가산
	Angle += AngleRate;

	//속도에 가속도 가산
	Speed += SpeedRate;

	//화면밖에 나가면 탄삭제
	if (X >= 600 || X < 0 || Y >= 800 || Y < 0) {
		Alive = false;
		X = 10000.0f;
		Y = -10000.0f;
	}
}

CRITICAL_SECTION cs;
HANDLE client_thread, recv_event;

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
bool is_all_ready = false;
bool Dondead;
int imsiDondead;
int EnemyXMove;
int Time;
DWORD frameDelta;
DWORD lastTime;

void error_display(const char* message);
void error_quit(const char* message);
int recvn(SOCKET socket, char* buffer, int length, int flags);

template<class T>
bool SendData(SOCKET sock, T* data, int len)
{
	int retval;

	retval = send(sock, (char*)&len, sizeof(int), 0);
	if (retval == SOCKET_ERROR)
	{
		error_display("고정 길이 send()");
		return false;
	}

	retval = send(sock, (char*)&(*data), len, 0);
	if (retval == SOCKET_ERROR)
	{
		error_display("가변 길이 send()");
		return false;
	}

	return true;
}

template<class T>
bool RecvData(SOCKET sock, T* data)
{
	int retval, len;

	retval = recvn(sock, (char*)&len, sizeof(int), 0);
	if (retval == SOCKET_ERROR)
	{
		error_display("고정 길이 recv()");
		return false;
	}
	else if (retval == 0)
		return false;

	retval = recvn(sock, (char*)&(*data), len, 0);
	if (retval == SOCKET_ERROR)
	{
		error_display("가변 길이 recv()");
		return false;
	}
	else if (retval == 0)
		return false;

	return true;
}

void InitalizeGameData();
void UpdateGameState();
void SendPlayerNumber(SOCKET sock);
void SendGameState(SOCKET sock);
void RecvPlayerInfo(SOCKET sock);
void SendAllPlayerInfo(SOCKET sock);
void SendEnemyInfo(SOCKET sock);
void CollisionCheck();
void Update();
void SendLogo(SOCKET socket);
void SendEnding(SOCKET socket);
void RecvReady(SOCKET socket);

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

	InitializeCriticalSection(&cs);
	
	recv_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (recv_event == NULL)
		return 1;

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

		client_thread = (HANDLE)_beginthreadex(NULL, 0, ProcessClient, (LPVOID)client_socket, 0, NULL);
		if (client_thread == NULL) 
			closesocket(client_socket);

		else
			CloseHandle(client_thread);
	}

	closesocket(listen_socket);
	DeleteCriticalSection(&cs);
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

void InitalizeGameData()
{
	curr_state = GAME_STATE::TITLE;
	num_player = 0;
	round_count = 0;
	frameDelta = 0;
	lastTime = timeGetTime();
	EnemyXMove = 3;
	win = 0;
	enemy.hp = 200;
	enemy.pos = { 100.0f, 105.f };
	PlayerBulletNum[0] = -1;
	PlayerBulletNum[1] = -1;
	EnemyBulletNum = 0;
	rect.left = 0;
	rect.right = 600;
	rect.top = 0;
	rect.bottom = 800;
	enemy.Shootbutterfly = 0;
	for (auto bullet : enemy.bullets)
		bullet = { 10000.0f, -10000.0f };

	for (int i = 0; i < MAX_PLAYER ; ++i)
	{
		players[i].number = i;
		players[i].hp = 3;
		players[i].is_click = false;
		players[i].is_ready = false;
		players[i].pos = { -10000.0f, -10000.0f };

		for (auto bullet : players[i].bullets)
			bullet = { 10000.0f, -10000.0f };
	}
}

unsigned WINAPI ProcessClient(LPVOID arg)
{
	SOCKET client_socket = (SOCKET)arg;
	SOCKADDR_IN client_address;
	int client_address_length;
	int received_data_length = 0;

	client_address_length = sizeof(client_address);
	getpeername(client_socket, (SOCKADDR*)&client_address, &client_address_length);

	SendPlayerNumber(client_socket);
	
	clock_t start = clock();
	DWORD currTime;
	DWORD FPS = 30;
	while (true) 
	{
		clock_t end = clock();

		//디버그용 출력문
		if ((end - start) / CLOCKS_PER_SEC >= 2)
		{
			start = clock();

			switch (curr_state)
			{
			case GAME_STATE::TITLE:
				printf("TITLE\n");
				break;
			case GAME_STATE::RUNNING:
				printf("RUNNING\n");
				break;
			case GAME_STATE::END:
				printf("END\n");
				break;
			default:
				printf("DEFAULT\n");
				break;
			}

			for (int i = 0; i < num_player; ++i)
			{
				printf("num_player: %d - ", num_player);
				printf("%dP pos {%.2f, %.2f}, click %d\n", i+1, players[i].pos.x, players[i].pos.y, players[i].is_click);
			}

			printf("enemy pos {%.2f, %.2f}\n", enemy.pos.x, enemy.pos.y);

		}
		currTime = timeGetTime();
		frameDelta = (currTime - lastTime) * 000.1f;
		if (frameDelta >= float(1) / float(FPS))
		{
			SendGameState(client_socket);
			switch (curr_state)
			{
			case GAME_STATE::TITLE:
				SendLogo(client_socket);
				RecvReady(client_socket);
				break;
			case GAME_STATE::RUNNING:

				RecvPlayerInfo(client_socket);
				//WaitForMultipleObjects(2, &client_thread, TRUE, INFINITE);
				Update();
				CollisionCheck();

				SendAllPlayerInfo(client_socket);
				SendEnemyInfo(client_socket);
				break;
			case GAME_STATE::END:
				SendEnding(client_socket);
				break;
			default:
				break;

			}
			UpdateGameState();
			lastTime = currTime;
		}
		if (curr_state == GAME_STATE::RUNNING) {
		}
	}

	EnterCriticalSection(&cs);
	num_player--;
	LeaveCriticalSection(&cs);

	return 0;
}

void UpdateGameState()
{
	if (players[0].is_ready && players[1].is_ready) {
		curr_state = GAME_STATE::RUNNING;
		if(round_count == 0)
			round_count = 1;
	}
	if (win == -1 || win == 1)
		curr_state = GAME_STATE::END;
}

void SendPlayerNumber(SOCKET sock)
{
	if (!SendData(sock, &num_player, sizeof(num_player)))
		return;

	EnterCriticalSection(&cs);
	num_player++;
	LeaveCriticalSection(&cs);
}

void SendGameState(SOCKET sock)
{
	//curr_state = GAME_STATE::RUNNING;	//테스트를 위해 죽지않게 RUNNING으로 일단 설정

	if (!SendData(sock, &curr_state, sizeof(curr_state)))
		return;
}

void RecvPlayerInfo(SOCKET sock)
{
	int number;
	if (!RecvData(sock, &number))
		return;

	if (!RecvData(sock, &players[number].pos))
		return;

	if (!RecvData(sock, &players[number].is_click))
		return;

	SetEvent(recv_event);
}

void SendAllPlayerInfo(SOCKET sock)
{
	for (int i = 0; i < MAX_PLAYER; ++i)
	{
		if (!SendData(sock, &players[i].number, sizeof(players[i].number)))
			return;

		if (!SendData(sock, &players[i].hp, sizeof(players[i].hp)))
			return;

		if (!SendData(sock, &players[i].pos, sizeof(players[i].pos)))
			return;

		if (!SendData(sock, &players[i].bullets, sizeof(players[i].bullets)))
			return;
	}
}

void SendEnemyInfo(SOCKET sock)
{
	if (!SendData(sock, &enemy.pos, sizeof(enemy.pos)))
		return;

	if (!SendData(sock, &enemy.bullets, sizeof(enemy.bullets)))
		return;

	if (!SendData(sock, &enemy.Shootbutterfly, sizeof(enemy.Shootbutterfly)))
		return;
}

void SendLogo(SOCKET socket)
{
	if (!SendData(socket, &num_player, sizeof(num_player)))
		return;
}

void SendEnding(SOCKET socket)
{
	// win이 1이면 승, -1이면 게임오버
	if (!SendData(socket, &win, sizeof(win)))
		return;
}

void RecvReady(SOCKET socket)
{
	int number;

	if (!RecvData(socket, &number))
		return;

	if (!RecvData(socket, &players[number].is_ready))
		return;
}


void Update()
{
	static float count = 0;
	//움직임 업데이트
	if (round_count == 1) {
		enemy.pos.x += EnemyXMove;
		if (enemy.pos.x - 30 <= rect.left || enemy.pos.x + 30 >= rect.right) {
			EnemyXMove *= -1;
			enemy.pos.x += EnemyXMove;
		}
	}
	for (int i = 0; i < MAX_ENEMY_BULLET; i++) {
		if (EnemyBullet[i].Alive == TRUE)
			EnemyBullet[i].Move();
	}
	for (int i = 0; i < MAX_PLAYER; i++) {
		for (int j = 0; j < MAX_PLAYER_BULLET; j++) {
			if (PlayersBullet[i][j].Alive == TRUE)
				PlayersBullet[i][j].Move();
		}
	}
	count++;
	if (count >= 1.85) {
		count -= 1.85;
		//시간별탄막
		Time += 1;

		for (int i = 0; i < MAX_PLAYER; i++) {
			if (players[i].is_click) {
				PlayerBulletNum[i]++;
				if (PlayerBulletNum[i] >= MAX_PLAYER_BULLET)
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

		if (round_count == 1) {
			if (Time < 30) {
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 20;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 3 - 180;
				EnemyBullet[EnemyBulletNum].AngleRate = 0;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 6;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 8;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = rand() % 20 + 2;
				EnemyBullet[EnemyBulletNum].SpeedRate = rand() % 3;
				EnemyBullet[EnemyBulletNum].Angle = rand() % 360;
				EnemyBullet[EnemyBulletNum].AngleRate = rand() % 5;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				enemy.Shootbutterfly = 1;
				if (Time % 2 == 0) {
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				enemy.Shootbutterfly = 0;
				if (Time % 3 == 0) {
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
					EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
					EnemyBullet[EnemyBulletNum].Speed = 5;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = Time * 6;
					EnemyBullet[EnemyBulletNum].AngleRate = 1;
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
					EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = Time * 6;
					EnemyBullet[EnemyBulletNum].AngleRate = -1;
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
					EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 90;
					EnemyBullet[EnemyBulletNum].AngleRate = 1;
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
					EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 90;
					EnemyBullet[EnemyBulletNum].AngleRate = -1;
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
					EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 180;
					EnemyBullet[EnemyBulletNum].AngleRate = 1;
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
					EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 180;
					EnemyBullet[EnemyBulletNum].AngleRate = -1;
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
						EnemyBulletNum = 0;
					EnemyBullet[EnemyBulletNum].Alive = TRUE;
					EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
					EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
					EnemyBullet[EnemyBulletNum].Speed = 10;
					EnemyBullet[EnemyBulletNum].SpeedRate = 0;
					EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 270;
					EnemyBullet[EnemyBulletNum].AngleRate = 1;
					EnemyBulletNum++;
					if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
						if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
						if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
						if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
						if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = -Time * 6;
				EnemyBullet[EnemyBulletNum].AngleRate = -1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 90;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = -Time * 6 - 90;
				EnemyBullet[EnemyBulletNum].AngleRate = -1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 180;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = -Time * 6 - 180;
				EnemyBullet[EnemyBulletNum].AngleRate = -1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
					EnemyBulletNum = 0;
				EnemyBullet[EnemyBulletNum].Alive = TRUE;
				EnemyBullet[EnemyBulletNum].X = enemy.pos.x;
				EnemyBullet[EnemyBulletNum].Y = enemy.pos.y;
				EnemyBullet[EnemyBulletNum].Speed = 10;
				EnemyBullet[EnemyBulletNum].SpeedRate = 0;
				EnemyBullet[EnemyBulletNum].Angle = Time * 6 - 270;
				EnemyBullet[EnemyBulletNum].AngleRate = 1;
				EnemyBulletNum++;
				if (EnemyBulletNum >= MAX_ENEMY_BULLET)
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
	}


	//정보 넘겨주기 위한 변수에 정리
	for (int i = 0; i < MAX_PLAYER; i++)
	{
		for (int j = 0; j < MAX_PLAYER_BULLET; j++)
		{
			players[i].bullets[j].x = PlayersBullet[i][j].X;
			players[i].bullets[j].y = PlayersBullet[i][j].Y;
		}
	}
	for (int j = 0; j < MAX_ENEMY_BULLET; j++)
	{
		enemy.bullets[j].x = EnemyBullet[j].X;
		enemy.bullets[j].y = EnemyBullet[j].Y;
	}
}

void CollisionCheck()
{
	for (int i = 0; i < MAX_PLAYER; i++) {
		for (int j = 0; j < MAX_PLAYER_BULLET; j++) {
			if (PlayersBullet[i][j].Alive == TRUE) {
				if (round_count == 1) {
					if ((PlayersBullet[i][j].X > enemy.pos.x - 45) && (PlayersBullet[i][j].X < enemy.pos.x + 45) &&
						(PlayersBullet[i][j].Y > enemy.pos.y - 75) && (PlayersBullet[i][j].Y < enemy.pos.y + 75)) {
						enemy.hp--;
						if (enemy.hp <= 0) {
							round_count = 2;
							EnemyBulletNum = 0;
							enemy.hp = 420;
							enemy.pos.x = 310;
							enemy.pos.y = 100;
							Time = 0;
							for (int i = 0; i < MAX_ENEMY_BULLET; i++) {
								EnemyBullet[i].Alive = false;
								EnemyBullet[i].X = 10000.0f;
								EnemyBullet[i].Y = -10000.0f;
							}
						}
						PlayersBullet[i][j].Alive = false;
						PlayersBullet[i][j].X = 10000.0f;
						PlayersBullet[i][j].Y = -10000.0f;
					}
				}
				else if (round_count == 2) {
					if ((PlayersBullet[i][j].X > enemy.pos.x - 45) && (PlayersBullet[i][j].X < enemy.pos.x + 45) && (PlayersBullet[i][j].Y > enemy.pos.y - 75) && (PlayersBullet[i][j].Y < enemy.pos.y + 75)) {
						enemy.hp--;
						if (enemy.hp < 0) {
							win = 1;
							EnemyBulletNum = 0;
							Time = 0;
							for (int i = 0; i < MAX_ENEMY_BULLET; i++) {
								EnemyBullet[i].Alive = false;
								EnemyBullet[i].X = 10000.0f;
								EnemyBullet[i].Y = -10000.0f;
							}
						}
						PlayersBullet[i][j].Alive = false;
						PlayersBullet[i][j].X = 10000.0f;
						PlayersBullet[i][j].Y = -10000.0f;
					}
				}
			}
		}
	}

	for (int i = 0; i < EnemyBulletNum; i++) {
		if (EnemyBullet[i].Alive == TRUE) {
			for (int j = 0; j < MAX_PLAYER; j++) {
				if (sqrt(pow(players[j].pos.x - EnemyBullet[i].X, 2.0f) + pow(players[j].pos.y - EnemyBullet[i].Y, 2.0f)) < 26) {
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
							EnemyBullet[i].X = 10000.0f;
							EnemyBullet[i].Y = -10000.0f;
						}
					}
				}
			}
		}
	}
	if (imsiDondead != 0) {
		imsiDondead++;
		if (imsiDondead >= 60)
			imsiDondead = 0;
	}
}