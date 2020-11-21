#pragma comment(lib, "ws2_32")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>

#define SERVER_PORT 9000

#define MAX_ENEMY_BULLET 200
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
	/*CBullet(float x, float y, float angle, float angle_rate, float speed, float speed_rate) {
		X = x; Y = y;	AngleRate = angle_rate;
		Angle = angle;	Speed = speed;
		SpeedRate = speed_rate;
		Alive = TRUE;
	};*/

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
GAME_STATE curr_state;
int num_player;

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
void SendGameState(SOCKET sock);
void RecvPlayerInfo(SOCKET sock);
void SendAllPlayerInfo(SOCKET sock);


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

void InitalizeGameData()
{
	GAME_STATE curr_state = GAME_STATE::TITLE;
	int num_player = 0;

	Enemy enemy;
	enemy.hp = 200;
	enemy.pos = { 100.0f, 105.f };
	for (auto bullet : enemy.bullets)
		bullet = { 10000.0f, -10000.0f };

	Player players[MAX_PLAYER];
	for (auto player : players)
	{
		player.number = -1;
		player.hp = 3;
		player.is_click = false;
		player.pos = { -10000.0f, -10000.0f };

		for (auto bullet : player.bullets)
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

	while (true)
	{
		UpdateGameState();
		SendGameState(client_socket);
		switch (curr_state)
		{
		case GAME_STATE::TITLE:
			//SendLogo();
			break;
		case GAME_STATE::RUNNING:
			RecvPlayerInfo(client_socket);
			//Update();
			//CollisionCheck();
			SendAllPlayerInfo(client_socket);
			//SendEnemyInfo();
			break;
		case GAME_STATE::END:
			//SendEnding();
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

void SendGameState(SOCKET sock)
{
	curr_state = GAME_STATE::RUNNING;

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