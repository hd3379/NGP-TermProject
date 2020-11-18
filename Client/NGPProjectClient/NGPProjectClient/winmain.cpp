#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include<windows.h>
#include<math.h>
#include <atlImage.h> 
#include <mmsystem.h>
#include "resource.h"


#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    10000

// 오류 출력 함수
void err_quit(char* msg);
void err_display(char* msg);
// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags);
// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);

SOCKET sock; // 소켓
char buf[BUFSIZE + 1]; // 데이터 송수신 버퍼
HANDLE hReadEvent, hWriteEvent; // 이벤트

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
GAME_STATE curr_state;

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

	// 이벤트 생성
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (hReadEvent == NULL) return 1;
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hWriteEvent == NULL) return 1;

	// 소켓 통신 스레드 생성
	CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);


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

	// 이벤트 제거
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// closesocket()
	closesocket(sock);

	// 윈속 종료
	WSACleanup();

	return Message.wParam;
}

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

	~CBullet(){}
};
void CBullet::Move(RECT rc) {
	float rad = Angle*3.14 / 180.0;

	//각도와 속도를 사용해서 좌표갱신
	X += Speed*cosf(rad);
	Y += Speed*sinf(rad);

	//각도에 각속도 가산
	Angle += AngleRate;

	//속도에 가속도 가산
	Speed += SpeedRate;

	//화면밖에 나가면 탄삭제
	if ( X >= rc.right || X< 0 || Y >=rc.bottom || Y < 0 ) {
		Alive = false;
	}
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


// TCP 클라이언트 시작 부분
DWORD WINAPI ClientMain(LPVOID arg)
{
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
	int len;

	// 서버와 데이터 통신
	while (1) {
		//SendGameState();
		switch (curr_state)
		{
		case GAME_STATE::TITLE:
			//RecvLogo();
			break;
		case GAME_STATE::RUNNING:
			//SendPlayerInfo();
			//RecvAllPlayerInfo();
			//RecvEnemyInfo();
			break;
		case GAME_STATE::END:
			//RecvEnding();
			break;
		default:
			break;
		}
	}
	return 0;
}



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
bool gameover;
bool win;
LRESULT CALLBACK ChildProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc, memDC;
	PAINTSTRUCT ps;
	static RECT rect;
	static int reimu_x, reimu_y;
	static int cirno_x, cirno_y,cirno_xplus,cirno_hp;
	static int yuyuko_x, yuyuko_y, yuyuko_hp;
	static int mouse_x, mouse_y;
	static int Time;
	
	static bool butterfly;
	static bool shoot;
	static bool imsiDondead;
	static CBullet Rattack[20];
	static CBullet Cattack[2001];
	static int Cattack_num;
	static int Rattack_num;
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
		Time = 0;
		reimu_x = 325;
		reimu_y = 750;
		cirno_hp = 250;
		cirno_xplus = 3;
		cirno_x = 100;
		cirno_y = 105;
		yuyuko_x = 310;
		yuyuko_y = 100;
		yuyuko_hp = 420;
		Dondead = 0;
		butterfly = 0;
		imsiDondead = 0;
		win = 0;
		shoot = 0; 
		PlaySound(TEXT("cirnosong.wav"), NULL, SND_ASYNC | SND_LOOP);
		SetTimer(hWnd, 1, 200, NULL);
		SetTimer(hWnd, 2, 70, NULL);
		SetTimer(hWnd, 3, 70, NULL);
		Rattack_num = 0;
		Cattack_num = 0;
		break;

	case WM_LBUTTONDOWN:

		shoot = 1;
		
		break;
	case WM_TIMER:
		switch (wParam) {
		case 1:
			if (shoot == 1) {
				Rattack_num++;
			if (Rattack_num >= 20)
				Rattack_num = 0;
				Rattack[Rattack_num].Alive = TRUE;
				Rattack[Rattack_num].X = reimu_x;
				Rattack[Rattack_num].Y = reimu_y;
				Rattack[Rattack_num].Speed = 20;
				Rattack[Rattack_num].SpeedRate = 0;
				Rattack[Rattack_num].Angle = -90;
				Rattack[Rattack_num].AngleRate = 0;
			}
			
			break;
		case 2:
			for (int i = 0; i < 20; i++) {
				if (Rattack[i].Alive == TRUE) {
					Rattack[i].Move(rect);
					if (round_count == 1) {
						if ((Rattack[i].X > cirno_x - 45) && (Rattack[i].X < cirno_x + 45) && (Rattack[i].Y > cirno_y - 75) && (Rattack[i].Y < cirno_y + 60)) {
							cirno_hp--;
							if (cirno_hp < 0) {
								round_count = 2;
								Cattack_num = 0;
								Time = 0;
								for (int i = 0; i < 2000; i++) {
									Cattack[i].Alive = false;
								}
							}
							Rattack[i].Alive = false;
						}
					}
					else if (round_count == 2) {
						if ((Rattack[i].X > yuyuko_x - 45) && (Rattack[i].X < yuyuko_x + 45) && (Rattack[i].Y > yuyuko_y - 75) && (Rattack[i].Y < yuyuko_y + 60)) {
							yuyuko_hp--;
							if (yuyuko_hp < 0) {
								win = 1;
								Cattack_num = 0;
								Time = 0;
								for (int i = 0; i < 2000; i++) {
									Cattack[i].Alive = false;
								}
							}
							Rattack[i].Alive = false;
						}
					}
				}
			}
			
			for (int i = 0; i < 2000; i++) {
				if (Cattack[i].Alive == TRUE) {
					Cattack[i].Move(rect);
					if (sqrt(pow(reimu_x - Cattack[i].X, 2) + pow(reimu_y - Cattack[i].Y, 2)) < 26) {
						if (Dondead == 0) {
							if (imsiDondead == 0) {
								life_point--;
								if (life_point <= 0)
									gameover = 1;
								else
								{
									imsiDondead = 1;
									SetTimer(hWnd, 5, 2000, NULL);
								}
								Cattack[i].Alive = false;
							}
						}
					}
				}
			}
			if (round_count == 1) {
				cirno_x += cirno_xplus;
				if (cirno_x - 30 <= rect.left || cirno_x + 30 >= rect.right) {
					cirno_xplus *= -1;
					cirno_x += cirno_xplus;
				}
			}
			break;
		case 3:
			//시간별탄막
			Time += 1;
			if (round_count == 1) {
				if (Time < 30) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 20;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = 90;
					Cattack[Cattack_num].AngleRate = 0;
				}
				else if (Time < 40) {}
				else if (Time < 70) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 20;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = 90;
					Cattack[Cattack_num].AngleRate = 0;
				}
				else if (Time < 80) {}
				else if (Time < 110) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 20;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = 90;
					Cattack[Cattack_num].AngleRate = 0;
				}
				else if (Time < 120) {	}
				else if (Time < 160) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 20;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = 90;
					Cattack[Cattack_num].AngleRate = 0;
				}
				else if(Time < 180){}
				else if (Time < 260) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 20;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = Time*3-180;
					Cattack[Cattack_num].AngleRate = 0;
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 20;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = Time*3;
					Cattack[Cattack_num].AngleRate = 0;
				}
				else if (Time < 270) {}
				else if (Time < 400) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 6;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = Time*8;
					Cattack[Cattack_num].AngleRate = 1;
				}
				else if (Time < 600) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 6;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = -Time * 8;
					Cattack[Cattack_num].AngleRate = -1;
				}
				else if (Time < 610) {}
				else if (Time < 800) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 6;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = Time*8;
					Cattack[Cattack_num].AngleRate = 1;
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 6;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = -Time * 8;
					Cattack[Cattack_num].AngleRate = -1;
				}
				else if (Time < 867) {
					cirno_xplus = 0;
					static int cirno_yp = 0;
					static int cirno_xp = 0;
					if (Time < 806) {
						cirno_xp = 20;
						cirno_yp = 20;
						break;
					}
					if (Time < 821) {
						cirno_x += cirno_xp;
						cirno_y += cirno_yp;
					}
					else if (Time < 836) {
						cirno_x -= cirno_xp;
						cirno_y -= cirno_yp;
					}
					else if (Time < 837) {
						cirno_xp = -20;
						cirno_yp = 20;
					}
					else if (Time < 854) {
						cirno_x += cirno_xp;
						cirno_y += cirno_yp;
					}
					else if (Time < 867) {
						cirno_x -= cirno_xp;
						cirno_y -= cirno_yp;
					}
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = 3;
					Cattack[Cattack_num].SpeedRate = 0;
					Cattack[Cattack_num].Angle = 90;
					Cattack[Cattack_num].AngleRate = 0;
				}
				else if (Time < 890) {}
				else if (Time < 2000) {
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = rand()%20+2;
					Cattack[Cattack_num].SpeedRate = rand()%3;
					Cattack[Cattack_num].Angle = rand()%360;
					Cattack[Cattack_num].AngleRate = rand()%5;
					Cattack_num++;
					if (Cattack_num >= 2000)
						Cattack_num = 0;
					Cattack[Cattack_num].Alive = TRUE;
					Cattack[Cattack_num].X = cirno_x;
					Cattack[Cattack_num].Y = cirno_y;
					Cattack[Cattack_num].Speed = rand() % 20+2;
					Cattack[Cattack_num].SpeedRate = rand() % 5;
					Cattack[Cattack_num].Angle = rand() % 360;
					Cattack[Cattack_num].AngleRate = rand() % 5;
				}
			}
			else if (round_count == 2) {
				if(Time < 20){}
				else if (Time < 160) {
					if (Time > 20) {
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = 200;
						Cattack[Cattack_num].Y = 200;
						Cattack[Cattack_num].Speed = 2;
						Cattack[Cattack_num].SpeedRate = 1;
						Cattack[Cattack_num].Angle = 90;
						Cattack[Cattack_num].AngleRate = 15;
					}
					if (Time > 50) {
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = 400;
						Cattack[Cattack_num].Y = 200;
						Cattack[Cattack_num].Speed = 2;
						Cattack[Cattack_num].SpeedRate = 1;
						Cattack[Cattack_num].Angle = 90;
						Cattack[Cattack_num].AngleRate = -15;
					}
					if (Time > 80) {
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = 200;
						Cattack[Cattack_num].Y = 600;
						Cattack[Cattack_num].Speed = 2;
						Cattack[Cattack_num].SpeedRate = 1;
						Cattack[Cattack_num].Angle = 0;
						Cattack[Cattack_num].AngleRate = 15;
					}
					if (Time > 120) {
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = 400;
						Cattack[Cattack_num].Y = 600;
						Cattack[Cattack_num].Speed = 2;
						Cattack[Cattack_num].SpeedRate = 1;
						Cattack[Cattack_num].Angle = 90;
						Cattack[Cattack_num].AngleRate = -15;
					}
				}
				else if (Time < 250) {}
				else if(Time < 500){
					butterfly = 1;
					if (Time % 2 == 0) {
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = 50;
						Cattack[Cattack_num].Y = rand() % 800 + 50;
						Cattack[Cattack_num].Speed = 5;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = 0;
						Cattack[Cattack_num].AngleRate = 0;
					}
				}
				else if (Time < 600) {}
				else if (Time < 900) {
					butterfly = 0;
					if (Time % 3 == 0) {
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 5;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6;
						Cattack[Cattack_num].AngleRate = 1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6;
						Cattack[Cattack_num].AngleRate = -1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 90;
						Cattack[Cattack_num].AngleRate = 1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 90;
						Cattack[Cattack_num].AngleRate = -1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 180;
						Cattack[Cattack_num].AngleRate = 1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 180;
						Cattack[Cattack_num].AngleRate = -1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 270;
						Cattack[Cattack_num].AngleRate = 1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 270;
						Cattack[Cattack_num].AngleRate = -1;
					}
				}
				else if (Time < 920) {}
				else if(Time <930){
					static int i = 9;
					for (int j = 19; j >= 0; j--) {
						if (pattern[i][j] == '#') {
							Cattack_num++;
							if (Cattack_num >= 2000)
								Cattack_num = 0;
							Cattack[Cattack_num].Alive = TRUE;
							Cattack[Cattack_num].X = 100 + j*10;
							Cattack[Cattack_num].Y = 100;
							Cattack[Cattack_num].Speed = 10;
							Cattack[Cattack_num].SpeedRate = 0;
							Cattack[Cattack_num].Angle = 90;
							Cattack[Cattack_num].AngleRate = 0;
						}
					}
					i--;
				}
				else if(Time < 940){
					static int i = 9;
					for (int j = 19; j >= 0; j--) {
						if (pattern[i][j] == '#') {
							Cattack_num++;
							if (Cattack_num >= 2000)
								Cattack_num = 0;
							Cattack[Cattack_num].Alive = TRUE;
							Cattack[Cattack_num].X = 500 + j * 10;
							Cattack[Cattack_num].Y = 100;
							Cattack[Cattack_num].Speed = 10;
							Cattack[Cattack_num].SpeedRate = 0;
							Cattack[Cattack_num].Angle = 90;
							Cattack[Cattack_num].AngleRate = 0;
						}
					}
					i--;
				}
				else if (Time < 950) {
					static int i = 9;
					for (int j = 19; j >= 0; j--) {
						if (pattern[i][j] == '#') {
							Cattack_num++;
							if (Cattack_num >= 2000)
								Cattack_num = 0;
							Cattack[Cattack_num].Alive = TRUE;
							Cattack[Cattack_num].X = 100 + j * 10;
							Cattack[Cattack_num].Y = 100;
							Cattack[Cattack_num].Speed = 10;
							Cattack[Cattack_num].SpeedRate = 0;
							Cattack[Cattack_num].Angle = 90;
							Cattack[Cattack_num].AngleRate = 0;
						}
					}
					i--;
				}
				else if (Time < 960) {
					static int i = 9;
					for (int j = 19; j >= 0; j--) {
						if (pattern[i][j] == '#') {
							Cattack_num++;
							if (Cattack_num >= 2000)
								Cattack_num = 0;
							Cattack[Cattack_num].Alive = TRUE;
							Cattack[Cattack_num].X = 300 + j * 10;
							Cattack[Cattack_num].Y = 100;
							Cattack[Cattack_num].Speed = 10;
							Cattack[Cattack_num].SpeedRate = 0;
							Cattack[Cattack_num].Angle = 90;
							Cattack[Cattack_num].AngleRate = 0;
						}
					}
					i--;
				}
				else if (Time < 970) {}
				else if (Time < 2000) {
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6;
						Cattack[Cattack_num].AngleRate = 1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = -Time * 6;
						Cattack[Cattack_num].AngleRate = -1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 90;
						Cattack[Cattack_num].AngleRate = 1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = -Time * 6 - 90;
						Cattack[Cattack_num].AngleRate = -1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 180;
						Cattack[Cattack_num].AngleRate = 1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = -Time * 6 - 180;
						Cattack[Cattack_num].AngleRate = -1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = Time * 6 - 270;
						Cattack[Cattack_num].AngleRate = 1;
						Cattack_num++;
						if (Cattack_num >= 2000)
							Cattack_num = 0;
						Cattack[Cattack_num].Alive = TRUE;
						Cattack[Cattack_num].X = yuyuko_x;
						Cattack[Cattack_num].Y = yuyuko_y;
						Cattack[Cattack_num].Speed = 10;
						Cattack[Cattack_num].SpeedRate = 0;
						Cattack[Cattack_num].Angle = -Time * 6 - 270;
						Cattack[Cattack_num].AngleRate = -1;
				}
			}
			//시간별탄막
			break;
		case 5:
			imsiDondead = 0;
			KillTimer(hWnd, 5);
			break;
		}
		InvalidateRect(hWnd, NULL, false);
		break;
	case WM_LBUTTONUP:
		shoot = 0;
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
		else if (gameover == 1) {
			GameOver.TransparentBlt(memDC, 0, 0, 600, 800,
				0, 0, 544, 725,RGB(8,8,8));
		}
		else if (round_count == 1) {
			cirno_back.TransparentBlt(memDC, 0, 0, 600, 800,
				0, 0, 600, 442, RGB(255, 255, 255));
			cirno.TransparentBlt(memDC, cirno_x - 30, cirno_y - 50, 60, 100,
				0, 0, 300, 500, RGB(255, 0, 0, ));
			for (int i = 0; i < 2000; i++) {
				if (Cattack[i].Alive == TRUE) {
					skill1.TransparentBlt(memDC, Cattack[i].X - 10, Cattack[i].Y - 10, 20, 20, 0, 0, 40, 40, RGB(250, 250, 250));
				}
			}
		}
		else {
			flower.TransparentBlt(memDC, 0, 0, 600, 800,
				0, 0, 600, 644, RGB(255, 255, 255));
			yuyuko.TransparentBlt(memDC, yuyuko_x - 30, yuyuko_y - 50, 60, 100,
				0 , 0 ,99,143,RGB(47,95,115));
			for (int i = 0; i < 2000; i++) {
				if (Cattack[i].Alive == TRUE) {
					if (butterfly == 0)
						skill2.TransparentBlt(memDC, Cattack[i].X - 10, Cattack[i].Y - 10, 20, 20, 0, 0, 40, 40, RGB(250, 250, 250));
					else
						Nabi.TransparentBlt(memDC, Cattack[i].X - 20, Cattack[i].Y - 20, 40, 40, 0, 0, 169, 172, RGB(0, 0, 0));
				}
			}
		}


		reimu.TransparentBlt(memDC, reimu_x - 19, reimu_y - 35, 41, 85,
			0, 0, 100, 216, RGB(255, 255, 255));

		

		core.TransparentBlt(memDC, reimu_x - 8, reimu_y -8, 16, 16,
			0, 0, 320, 320, RGB(255, 255, 255));

		//레이무탄
		for (int i = 0; i < 20; i++) {
			if (Rattack[i].Alive == TRUE) {
				Bujuck.TransparentBlt(memDC, Rattack[i].X-7, Rattack[i].Y-10, 15, 20, 0, 0, 228, 490, RGB(255, 255, 255));
			}
		}
		//레이무탄

		BackBuff.Draw(hdc, 0, 0);
		BackBuff.ReleaseDC();

		ReleaseDC(hWnd, memdc);
		EndPaint(hWnd, &ps);
		break;

	case WM_MOUSEMOVE:
		mouse_x = LOWORD(lParam);
		mouse_y = HIWORD(lParam);

		if (mouse_x <= 590 && mouse_x >= 10)
			reimu_x = mouse_x;

		if (mouse_y <= 790 && mouse_y >= 10)
			reimu_y = mouse_y;

		break;
	case WM_DESTROY:
		KillTimer(hWnd, 1);
		KillTimer(hWnd, 2);
		KillTimer(hWnd, 3);
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

	switch (uMsg) {

	case WM_CREATE:
		GetClientRect(hWnd, &rect);

		SetTimer(hWnd, 1, 70, NULL);

		break;

	case WM_TIMER:
		switch (wParam) {
		case 1:
			if (win == 1 || gameover == 1) {}
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

		for (int a = 0; a < life_point; a++) {
			life.TransparentBlt(memDC, 100 + (40 * a), 195, 33, 40,
				0, 0, 33, 40, RGB(250, 250, 250));
		}


		TextOut(memDC, 20, 250, TEXT("skill1"), 7);

		for (int a = 0; a < skill_1_point; a++) {
			skill1.TransparentBlt(memDC, 100 + (40 * a), 245, 40, 40,
				0, 0, 40, 40, RGB(250, 250, 250));
		}


		TextOut(memDC, 20, 300, TEXT("skill2"), 7);

		for (int a = 0; a < skill_2_point; a++) {
			skill2.TransparentBlt(memDC, 100 + (40 * a), 295, 41, 40,
				0, 0, 41, 40, RGB(250, 250, 250));
		}

		SelectObject(memDC, oldFont);
		DeleteObject(hFont);


		////////


		///////// score

		SetBkMode(memDC, TRANSPARENT);

		hFont = CreateFont(30, 0, 0, 0, 1000, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0,
			VARIABLE_PITCH | FF_ROMAN, TEXT("굴림"));
		oldFont = (HFONT)SelectObject(memDC, hFont);

		TextOut(memDC, 20, 100, TEXT("Score : "), 9);

		wsprintf(str, TEXT("%d"), score);
		TextOut(memDC, 150, 100, str, lstrlen(str));

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