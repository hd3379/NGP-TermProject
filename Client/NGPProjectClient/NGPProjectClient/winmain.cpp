#pragma comment(lib, "ws2_32")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <stdlib.h>
#include<windows.h>
#include<math.h>
#include <atlImage.h> 
#include <mmsystem.h>
#include "resource.h"

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000

#define MAX_ENEMY_BULLET 200
#define MAX_PLAYER_BULLET 50
#define MAX_PLAYER 1

// 오류 출력 함수
void err_quit(char* msg);
void err_display(char* msg);
// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags);

SOCKET sock; // 소켓

HINSTANCE g_hInst;
LPCTSTR lpszClass = TEXT("Window Class Name");
LPCTSTR Child = TEXT("Child");
LPCTSTR Child2 = TEXT("Child2");

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ChildProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ChildProc2(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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

typedef struct Enemy
{
	int hp;
	Position pos;
	Position bullets[MAX_ENEMY_BULLET];
};

template<class T>
bool SendData(SOCKET sock, T* data, int len)
{
	int retval;

	retval = send(sock, (char*)&len, sizeof(int), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("고정 길이 send()");
		return false;
	}

	retval = send(sock, (char*)&(*data), len, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("가변 길이 send()");
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
		err_display("고정 길이 recv()");
		return false;
	}
	else if (retval == 0)
		return false;

	retval = recvn(sock, (char*)&(*data), len, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("가변 길이 recv()");
		return false;
	}
	else if (retval == 0)
		return false;

	return true;
}

void RecvPlayerNumber(SOCKET sock);
void RecvGameState(SOCKET sock);
void SendPlayerInfo(SOCKET sock);
void RecvAllPlayerInfo(SOCKET sock);
void RecvEnemyInfo(SOCKET sock);
void RecvLogo(SOCKET socket);
void RecvEnding(SOCKET socket);
bool IsAlive(Position bullet);
float Clamp(float min, float value, float max);

Enemy enemy;
Player players[MAX_PLAYER];
GAME_STATE curr_state;
int my_number;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpszCmdParam, int nCmdShow)
{
	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hInst = hInstance;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = lpszClass;
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&WndClass);

	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.lpszClassName = Child;
	WndClass.lpfnWndProc = ChildProc;
	RegisterClassEx(&WndClass);

	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.lpszClassName = Child2;
	WndClass.lpfnWndProc = ChildProc2;
	RegisterClassEx(&WndClass);

	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	//네이글 알고리즘 중지
	bool optval = TRUE;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&optval, sizeof(optval));

	RecvPlayerNumber(sock);

	hWnd = CreateWindow(lpszClass,
		TEXT("window program"),
		WS_OVERLAPPEDWINDOW,
		0, 0, 1000, 900, NULL,
		(HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	
	closesocket(sock);
	// 윈속 종료
	WSACleanup();

	return Message.wParam;
}

CImage background;
CImage BackBuff;
CImage cirno_back;
CImage reimu;
CImage cirno;
CImage core;
CImage flower;
CImage cirno_name;
CImage yuyuko_name;
CImage life;
CImage skill1;
CImage skill2;
CImage Bujuck;
CImage Nabi;
CImage yuyuko;
CImage Win;
CImage GameOver;

HBITMAP CirnoSpin;	// 0 ,123,140
HBITMAP ReimuSpin;	//98,124,76
HBITMAP Background;
HBITMAP Button;
HBITMAP TITLE;

int win;
int num_player;

void LOGO(HDC hdc,RECT rc) {
	HDC memdc;
	HDC alpa;
	HBITMAP alpaBoard;
	static int Cspin[14] = { 789,863,0,60,130,196,290,385,468,552,634,716,789,863}; //11개
	static int Rspin[17] = { 0,173,308,388,510,617,743,849,1037,1228,1412,1592,1772,1964,2073,2194 };//16개
	static BLENDFUNCTION bf;
	static int buttonb=0;
	static int bb = 2;
	static int Cb = 9;
	static int Rb = 0;
	static int rest = 0;
	//투명정도
	buttonb+=bb;
	if (buttonb >= 23)
		bb *= -1;
	if (buttonb <= 0)
		bb *= -1;
	//투명정도

	// 애니메이션
	if (Cb >= 12) {
		if (rest < 10)
			rest++;
		else
		{
			Rb++;
		}
		if (Rb >= 14) {
			rest = 0;
			Cb = 0;
		}
	}
	else
	{
		if (rest < 10)
			rest++;
		else if (Cb == 0)
			Cb += 2;
		else
			Cb++;

		if (Cb >= 12)
			rest = 0;
		Rb = 0;
	}
	// 애니메이션
	
	memset(&bf, 0, sizeof(bf));

	memdc = CreateCompatibleDC(hdc);
	alpa = CreateCompatibleDC(hdc);
	alpaBoard = CreateCompatibleBitmap(hdc,rc.right,rc.bottom);
	SelectObject(alpa, alpaBoard);
	      
	SelectObject(memdc, Background);

	StretchBlt(hdc,0,0,rc.right,rc.bottom,memdc,0,0,824,550,SRCCOPY);

	//치르노회전
	SelectObject(memdc, CirnoSpin);
	TransparentBlt(hdc, 500,500,70,100 , memdc, Cspin[Cb], 0, Cspin[Cb + 1]-Cspin[Cb], 85, RGB(0, 123, 140));
	//치르노회전

	//레이무회전

	SelectObject(memdc, ReimuSpin);
	TransparentBlt(hdc, 250,500,100,100, memdc, Rspin[Rb], 0, Rspin[Rb + 1]- Rspin[Rb], 171, RGB(98, 124, 76));
	//레이무회전

	//Title
	SelectObject(memdc, TITLE);
	TransparentBlt(hdc, 650, 50, 300, 418, memdc, 0, 0, 300, 418, RGB(255, 255, 255));
	//Title


	//press button
	SelectObject(memdc, Button);

	BitBlt(alpa, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
	TransparentBlt(alpa,300, 650,350,80, memdc , 0, 0, 570, 120, RGB(255, 255, 255));

	bf.SourceConstantAlpha = 10*buttonb;// 연하기의 정도는 이 값을 바꾼다.
	AlphaBlend(hdc, 0, 0, rc.right, rc.bottom, alpa, 0, 0, rc.right, rc.bottom, bf);
	//press button

	DeleteObject(alpa);
	DeleteDC(memdc);
	DeleteDC(alpa);
}
static bool Dondead;
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	HDC memdc;
	HDC memDC;
	static HWND child_hWnd;
	static HWND child_hWnd2;
	static RECT rc;
	static bool start;

	RecvGameState(sock);
	switch (curr_state)
	{
	case GAME_STATE::TITLE:
		RecvLogo(sock);
		break;
	case GAME_STATE::RUNNING:
		SendPlayerInfo(sock);
		RecvAllPlayerInfo(sock);
		RecvEnemyInfo(sock);
		break;
	case GAME_STATE::END:
		RecvEnding(sock);
		break;
	default:
		break;
	}

	switch (iMessage) {
	case WM_CREATE:
		GetClientRect(hWnd, &rc);
		start = 0;
		
		background.Load(TEXT("background.PNG"));
		cirno_back.Load(TEXT("cirno_back.PNG"));
		reimu.Load(TEXT("reimu.PNG"));
		core.Load(TEXT("core.PNG"));
		flower.Load(TEXT("flower.PNG"));
		cirno_name.Load(TEXT("cirno_name.PNG"));
		yuyuko_name.Load(TEXT("yuyuko_name.PNG"));
		life.Load(TEXT("life.PNG"));
		skill1.Load(TEXT("skill1.PNG"));
		skill2.Load(TEXT("skill2.PNG"));
		Bujuck.Load(TEXT("부적.bmp"));
		cirno.Load(TEXT("cirno.bmp"));
		yuyuko.Load(TEXT("yuyuko.bmp"));
		Nabi.Load(TEXT("butterfly.bmp"));
		GameOver.Load(TEXT("gameover.bmp"));
		Win.Load(TEXT("Reimuwin.bmp"));

		BackBuff.Create(1000, 900, 24);

		Background = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_UBackground));
		Button = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_Button));
		CirnoSpin = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_CirnoSpin));
		ReimuSpin = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_ReimuSpin));
		TITLE = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_TITLE));

		SetTimer(hWnd, 1, 120, NULL);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		HBITMAP memBoard;
		memdc = CreateCompatibleDC(hdc);
		memBoard = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
		SelectObject(memdc, memBoard);

		if (start == 0) {
			LOGO(memdc, rc);
		}
		else {
			memDC = BackBuff.GetDC();

			FillRect(memDC, &rc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

			background.Draw(memDC, 0, 0, 1000, 900);


			BackBuff.Draw(hdc, 0, 0);
			BackBuff.ReleaseDC();

		}
		BitBlt(hdc, 0, 0, rc.right, rc.bottom, memdc, 0, 0, SRCCOPY);

		DeleteDC(memdc);
		EndPaint(hWnd, &ps);
		DeleteObject(memBoard);
		break;
	case WM_KEYDOWN:
		if (start == 0) {
			start = 1;
			child_hWnd = CreateWindow(TEXT("Child"), NULL, WS_CHILD | WS_VISIBLE, 50, 30, 600, 800, hWnd, NULL, g_hInst, NULL);

			// score
			child_hWnd2 = CreateWindow(TEXT("Child2"), NULL, WS_CHILD | WS_VISIBLE, 700, 30, 250, 800, hWnd, NULL, g_hInst, NULL);

			InvalidateRect(hWnd, NULL, TRUE);
		}
		else if (wParam == 'z' || wParam == 'Z') {
			if (Dondead == 1) {
				Dondead = 0;
			}
			else if (Dondead == 0) {
				Dondead = 1;
			}
		}
		break;
	case WM_TIMER:
		if (start == 0) {
			InvalidateRect(hWnd, NULL, false);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return(DefWindowProc(hWnd, iMessage, wParam, lParam));
}

HFONT hFont, oldFont;
static int score = 0;
static int round_count = 1;
static int life_point = 3;
static int skill_1_point = 3;
static int skill_2_point = 3;
LRESULT CALLBACK ChildProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc, memDC;
	PAINTSTRUCT ps;
	static RECT rect;
	static int cirno_x, cirno_y,cirno_xplus,cirno_hp;
	static int yuyuko_x, yuyuko_y, yuyuko_hp;
	static int Time;
	
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

	switch (uMsg) {

	case WM_CREATE:
		GetClientRect(hWnd, &rect);
		SetTimer(hWnd, 2, 70, NULL);
		break;
	case WM_LBUTTONDOWN:
		players[my_number].is_click = true;
		break;
	case WM_LBUTTONUP:
		players[my_number].is_click = false;
		break;
	case WM_TIMER:
		InvalidateRect(hWnd, NULL, FALSE);

		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		memDC = BackBuff.GetDC();
		HDC memdc;
		memdc = CreateCompatibleDC(memDC);

		FillRect(memDC, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

		if (win == 1) {
			Win.TransparentBlt(memDC, 0,200,600,400,
			0,0,258,194,RGB(8,8,8));
		}
		else if (win == -1) {
			GameOver.TransparentBlt(memDC, 0, 0, 600, 800,
				0, 0, 544, 725,RGB(8,8,8));
		}
		else if (round_count == 1)
		{
			cirno_back.TransparentBlt(memDC, 0, 0, 600, 800,
				0, 0, 600, 442, RGB(255, 255, 255));
			cirno.TransparentBlt(memDC, enemy.pos.x - 30, enemy.pos.y - 50, 60, 100,
				0, 0, 300, 500, RGB(255, 0, 0, ));
			for (int i = 0; i < MAX_ENEMY_BULLET; i++) 
			{
				if (IsAlive(enemy.bullets[i])) {
					skill1.TransparentBlt(memDC, enemy.bullets[i].x - 10, enemy.bullets[i].y - 10,
						20, 20, 0, 0, 40, 40, RGB(250, 250, 250));
				}
			}
		}
		else
		{
			flower.TransparentBlt(memDC, 0, 0, 600, 800,
				0, 0, 600, 644, RGB(255, 255, 255));
			yuyuko.TransparentBlt(memDC, enemy.pos.x - 30, enemy.pos.y - 50, 60, 100,
				0 , 0 ,99,143,RGB(47,95,115));
			for (int i = 0; i < MAX_ENEMY_BULLET; i++) 
			{
				if (IsAlive(enemy.bullets[i])) {
					if (butterfly == 0)
						skill2.TransparentBlt(memDC, enemy.bullets[i].x - 10, enemy.bullets[i].y - 10, 
							20, 20, 0, 0, 40, 40, RGB(250, 250, 250));
					else
						Nabi.TransparentBlt(memDC, enemy.bullets[i].x - 20, enemy.bullets[i].y - 20,
							40, 40, 0, 0, 169, 172, RGB(0, 0, 0));
				}
			}
		}

		for (int i = 0; i < MAX_PLAYER; ++i)
		{
			reimu.TransparentBlt(memDC, (int)players[i].pos.x - 19, (int)players[i].pos.y - 35, 41, 85,
				0, 0, 100, 216, RGB(255, 255, 255));

			core.TransparentBlt(memDC, (int)players[i].pos.x - 8, (int)players[i].pos.y - 8, 16, 16,
				0, 0, 320, 320, RGB(255, 255, 255));

			for (int j = 0; j < MAX_PLAYER_BULLET; j++)
			{
				if (IsAlive(players[i].bullets[j]))
				{
					Bujuck.TransparentBlt(memDC, players[i].bullets[j].x - 7, players[i].bullets[j].y - 10,
						15, 20, 0, 0, 228, 490, RGB(255, 255, 255));
				}
			}
		}

		BackBuff.Draw(hdc, 0, 0);
		BackBuff.ReleaseDC();

		ReleaseDC(hWnd, memdc);
		EndPaint(hWnd, &ps);
		break;

	case WM_MOUSEMOVE:
		players[my_number].pos.x = LOWORD(lParam);
		players[my_number].pos.y = HIWORD(lParam);
		
		players[my_number].pos.x = Clamp(10.0f, players[my_number].pos.x, 590.0f);
		players[my_number].pos.y = Clamp(10.0f, players[my_number].pos.y, 790.0f);

		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ChildProc2(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc, memDC;
	PAINTSTRUCT ps;
	static RECT rect;
	TCHAR str[10];
	TCHAR debugstringA[40], debugstringB[40], debugstringC[40];

	switch (uMsg) {

	case WM_CREATE:
		GetClientRect(hWnd, &rect);

		SetTimer(hWnd, 1, 70, NULL);

		break;

	case WM_TIMER:
		switch (wParam) {
		case 1:
			if (win == 1 || win == -1) {}
			else
			score += 3;

			break;
		}

		InvalidateRect(hWnd, NULL, FALSE);

		break;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		memDC = BackBuff.GetDC();

		FillRect(memDC, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

		if (round_count == 1) {
			SetTextColor(memDC, RGB(100, 100, 250));
			cirno_back.TransparentBlt(memDC, 0, 0, 250, 800,
				600, 0, 200, 442, RGB(255, 255, 255));

			cirno_name.TransparentBlt(memDC, 0, 400, 250, 395,
				0, 0, 300, 418, RGB(255, 255, 255));
		}
		else {
			flower.TransparentBlt(memDC, 0, 0, 250, 800,
				600, 0, 200, 644, RGB(255, 255, 255));
			SetTextColor(memDC, RGB(250, 100, 100));

			yuyuko_name.TransparentBlt(memDC, 0, 400, 265, 410,
				0, 0, 300, 418, RGB(255, 255, 255));

		}

		////////// life

		hFont = CreateFont(30, 0, 0, 0, 1000, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0,
			VARIABLE_PITCH | FF_ROMAN, TEXT("굴림"));
		oldFont = (HFONT)SelectObject(memDC, hFont);

		TextOut(memDC, 20, 200, TEXT("Life"), 5);

		for (int a = 0; a < players[my_number].hp; a++) {
			life.TransparentBlt(memDC, 100 + (40 * a), 195, 33, 40,
				0, 0, 33, 40, RGB(250, 250, 250));
		}

		SelectObject(memDC, oldFont);
		DeleteObject(hFont);

		///////// score

		SetBkMode(memDC, TRANSPARENT);
		 
		hFont = CreateFont(20, 0, 0, 0, 1000, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0,
			VARIABLE_PITCH | FF_ROMAN, TEXT("굴림"));
		oldFont = (HFONT)SelectObject(memDC, hFont);

		TextOut(memDC, 20, 100, TEXT("Score : "), 9);

		wsprintf(str, TEXT("%d"), score);
		TextOut(memDC, 150, 100, str, lstrlen(str));

		for (int i = 0; i < MAX_PLAYER; ++i)
		{
			_stprintf(debugstringA, TEXT("nump %d mynum %d"), num_player, my_number);
			TextOut(memDC, 10, 300 + i*20, debugstringA, lstrlen(debugstringA));

			_stprintf(debugstringB, TEXT("%dP {%d, %d} c %d"), 
				i + 1, (int)players[i].pos.x, (int)players[i].pos.y, players[i].is_click);
			TextOut(memDC, 10, 360 + i*20, debugstringB, lstrlen(debugstringB));

			printf("%dP pos {%.1f, %.1f} c %d\n",
				i + 1, players[i].pos.x, players[i].pos.y, players[i].is_click);
		}

		_stprintf(debugstringC, TEXT("e pos {%d, %d}"), (int)enemy.pos.x, (int)enemy.pos.y);
		TextOut(memDC, 10, 400, debugstringC, lstrlen(debugstringC));

		SelectObject(memDC, oldFont);
		DeleteObject(hFont);

		////////////////

		BackBuff.Draw(hdc, 0, 0);
		BackBuff.ReleaseDC();


		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// 소켓 함수 오류 출력 후 종료
void err_quit(char* msg)
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

// 소켓 함수 오류 출력
void err_display(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
}

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

void RecvPlayerNumber(SOCKET sock)
{
	if (!RecvData(sock, &my_number))
		return;
}

void RecvGameState(SOCKET sock)
{
	if (!RecvData(sock, &curr_state))
		return;
}

void SendPlayerInfo(SOCKET sock)
{
	if (!SendData(sock, &players[my_number].number, sizeof(players[my_number].number)))
		return;

	if (!SendData(sock, &players[my_number].pos, sizeof(players[my_number].pos)))
		return;

	if (!SendData(sock, &players[my_number].is_click, sizeof(players[my_number].is_click)))
		return;
}

void RecvAllPlayerInfo(SOCKET sock)
{
	int number;

	for (int i = 0; i < MAX_PLAYER; ++i)
	{
		if (!RecvData(sock, &number))
			return;

		if (!RecvData(sock, &players[number].hp))
			return;

		if (!RecvData(sock, &players[number].pos))
			return;

		if (!RecvData(sock, &players[number].bullets))
			return;
	}
}

void RecvLogo(SOCKET socket)
{
	if (!RecvData(sock, &num_player))
		return;
}

void RecvEnding(SOCKET socket)
{
	if (!RecvData(sock, &win))
		return;
}

void RecvEnemyInfo(SOCKET sock)
{
	if (!RecvData(sock, &enemy.pos))
		return;

	if (!RecvData(sock, &enemy.bullets))
		return;
}

bool IsAlive(Position bullet)
{
	if ((-5000.0f < bullet.x && bullet.x < 5000.0f) && (-5000.0f < bullet.y && bullet.y < 5000.0f))
		return true;
	else
		return false;
}

float Clamp(float min, float value, float max)
{
	if (value < min)
		return min;

	if (value > max)
		return max;

	return value;
}