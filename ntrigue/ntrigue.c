#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h> /* For timeBeginPeriod/timeEndPeriod */

#pragma comment(lib, "winmm.lib") /* Link with winmm.lib for timer functions */

/* Define DC_PEN and DC_BRUSH for older Windows SDK */
#ifndef DC_PEN
#define DC_PEN 19
#endif
#ifndef DC_BRUSH
#define DC_BRUSH 18
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAX_ENEMIES 20
#define MAX_LASERS 30
#define MAX_EXPLOSIONS 20
#define PLAYER_SIZE 20
#define ENEMY_SIZE 20
#define MOTHERSHIP_TYPE 3  /* New enemy type for rare massive ships */
#define LASER_WIDTH 3
#define LASER_HEIGHT 15
#define ENEMY_LASER_SPEED 5
#define PLAYER_LASER_SPEED 10
#define SCROLL_SPEED 2
#define STAR_COUNT 100

typedef struct {
    int x, y;
    int width, height;
    int active;
    int type;  /* 0 = normal, 1 = shooting, 2 = boss */
    int health;
    int shootTimer;
    int scoreValue;
} Enemy;

typedef struct {
    int x, y;
    int width, height;
    int speed;
    int active;
    int isEnemy;  /* 0 = player laser, 1 = enemy laser */
    int dx, dy;   /* Direction for aimed lasers */
} Laser;

typedef struct {
    int x, y;
    int radius;
    int active;
    int maxRadius;
    int type;  /* 0 = enemy, 1 = player */
} Explosion;

typedef struct {
    int x, y;
    int brightness;
} Star;

/* Game objects */
Enemy enemies[MAX_ENEMIES];
Laser lasers[MAX_LASERS];
Explosion explosions[MAX_EXPLOSIONS];
Star stars[STAR_COUNT];

/* Player state */
int playerX, playerY;
int playerHealth = 3;
int score = 0;
int level = 1;
int gameOver = 0;
int gameStarted = 0;
int shootCooldown = 0;
int enemySpawnTimer = 0;
int frameCount = 0;

/* GDI objects */
HINSTANCE hInst;
HWND hwnd;
HDC hdcBuffer;
HBITMAP hbmBuffer;
HBITMAP hbmOldBuffer;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void GameLoop(HDC hdc);
void DrawScene(HDC hdc);
void UpdateGame(void);
void SpawnEnemy(int type);
void ShootLaser(int x, int y, int isEnemy);
void ShootAimedLaser(int x, int y, int dx, int dy, int isEnemy);
void CreateExplosion(int x, int y, int type);
void InitStars(void);
void DrawStars(HDC hdc);
void UpdateStars(void);
void RestartGame(void);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc;
    MSG msg;
    RECT rcClient;
    int windowWidth, windowHeight;
    int i;
    HANDLE hProcess;

    /* Set process to high priority for better performance */
    hProcess = GetCurrentProcess();
    SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);

    /* Register window class with optimized styles */
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = NULL;  /* Hide the cursor */
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "NTRigueClass";

    if (!RegisterClass(&wc)) return 1;

    /* Calculate window size to accommodate client area */
    rcClient.left = 0;
    rcClient.top = 0;
    rcClient.right = WINDOW_WIDTH;
    rcClient.bottom = WINDOW_HEIGHT;
    AdjustWindowRect(&rcClient, WS_OVERLAPPEDWINDOW, FALSE);
    windowWidth = rcClient.right - rcClient.left;
    windowHeight = rcClient.bottom - rcClient.top;

    /* Create optimized window - disable resize and maximize for better performance */
    hwnd = CreateWindow("NTRigueClass", "NT Rigue",
                      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 
                      CW_USEDEFAULT, CW_USEDEFAULT,
                      windowWidth, windowHeight, NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* Initialize random seed */
    srand((unsigned)time(NULL));

    /* Initialize game state */
    gameOver = 0;
    gameStarted = 0;
    
    /* Initialize player */
    playerX = WINDOW_WIDTH / 2;
    playerY = WINDOW_HEIGHT - 50;
    
    /* Initialize enemies */
    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }
    
    /* Initialize lasers */
    for (i = 0; i < MAX_LASERS; i++) {
        lasers[i].active = 0;
    }
    
    /* Initialize explosions */
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        explosions[i].active = 0;
    }
    
    /* Initialize stars background */
    InitStars();

    /* Main message loop - reverting to simpler GetMessage for reliability */
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    PAINTSTRUCT ps;
    POINT point;
    static BOOL cursorVisible = FALSE;

    switch (msg) {
        case WM_CREATE:
            /* Create optimized double-buffering */
            hdc = GetDC(hwnd);
            hdcBuffer = CreateCompatibleDC(hdc);
            hbmBuffer = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
            hbmOldBuffer = SelectObject(hdcBuffer, hbmBuffer);
            
            /* Fill buffer with black to avoid initial flicker */
            PatBlt(hdcBuffer, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BLACKNESS);
            
            /* Just ensure the buffer is properly initialized */
            
            ReleaseDC(hwnd, hdc);
            
            /* Set timer for game loop (60 FPS) with higher resolution */
            timeBeginPeriod(1); /* Request 1ms timer resolution */
            SetTimer(hwnd, 1, 16, NULL);
            
            /* Hide cursor */
            ShowCursor(FALSE);
            return 0;

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            
            /* Make sure we update the game state and draw to buffer first */
            GameLoop(hdcBuffer);
            
            /* Copy buffer to screen */
            BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcBuffer, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;

        case WM_TIMER:
            /* Simply trigger a repaint, which will call GameLoop */
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_MOUSEMOVE:
            /* Get mouse position */
            playerX = LOWORD(lParam);
            playerY = HIWORD(lParam);
            
            /* Keep player within window bounds */
            if (playerX < PLAYER_SIZE / 2) playerX = PLAYER_SIZE / 2;
            if (playerX > WINDOW_WIDTH - PLAYER_SIZE / 2) playerX = WINDOW_WIDTH - PLAYER_SIZE / 2;
            if (playerY < PLAYER_SIZE / 2) playerY = PLAYER_SIZE / 2;
            if (playerY > WINDOW_HEIGHT - PLAYER_SIZE / 2) playerY = WINDOW_HEIGHT - PLAYER_SIZE / 2;
            
            /* Keep cursor hidden when inside window */
            GetCursorPos(&point);
            ScreenToClient(hwnd, &point);
            if (point.x >= 0 && point.x < WINDOW_WIDTH && 
                point.y >= 0 && point.y < WINDOW_HEIGHT) {
                if (cursorVisible) {
                    ShowCursor(FALSE);
                    cursorVisible = FALSE;
                }
            } else {
                if (!cursorVisible) {
                    ShowCursor(TRUE);
                    cursorVisible = TRUE;
                }
            }
            return 0;

        case WM_LBUTTONDOWN:
            if (gameOver) {
                /* Restart game when clicking anywhere after game over */
                RestartGame();
            } else {
                /* Player shoots laser on left click during gameplay */
                ShootLaser(playerX, playerY - PLAYER_SIZE / 2, 0);
            }
            return 0;

        case WM_KEYDOWN:
            /* Exit game on Q or ESC key press */
            if (wParam == 'Q' || wParam == VK_ESCAPE) {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            
            if (gameOver) {
                /* Restart game on any key press after game over */
                RestartGame();
            } else if (wParam == VK_SPACE) {
                /* Player shoots laser on space key during gameplay */
                ShootLaser(playerX, playerY - PLAYER_SIZE / 2, 0);
            }
            return 0;

        case WM_DESTROY:
            /* Clean up resources */
            KillTimer(hwnd, 1);
            timeEndPeriod(1); /* End high-resolution timer */
            
            /* Clean up GDI objects */
            SelectObject(hdcBuffer, hbmOldBuffer);
            DeleteObject(hbmBuffer);
            DeleteDC(hdcBuffer);
            
            /* Restore cursor */
            ShowCursor(TRUE);
            
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void GameLoop(HDC hdc)
{
    /* Start the game on first call */
    if (!gameStarted) {
        gameStarted = 1;
    }
    
    /* Increment frame counter for animations */
    frameCount++;
    
    /* Update game state */
    UpdateGame();
    
    /* Render the scene */
    DrawScene(hdc);
}

void DrawScene(HDC hdc)
{
    RECT rect;
    HBRUSH playerBrush, enemyBrush, laserBrush;
    char windowTitle[80];
    int i;

    /* Update window title with score and level */
    if (gameStarted && gameOver) {
        wsprintf(windowTitle, "NT Rigue - GAME OVER - Final Score: %d - Press ESC to Exit", score);
    } else {
        wsprintf(windowTitle, "NT Rigue - Score: %d  Level: %d  Health: %d - ESC/Q to Exit", score, level, playerHealth);
    }
    SetWindowText(hwnd, windowTitle);

    /* Clear the screen */
    rect.left = 0;
    rect.top = 0;
    rect.right = WINDOW_WIDTH;
    rect.bottom = WINDOW_HEIGHT;
    FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    /* Draw starry background */
    DrawStars(hdc);

    /* Draw player ship if not game over */
    if (!gameOver) {
        /* Draw player ship (blue triangle) */
        POINT playerPoints[3];
        HBRUSH engineBrush;
        
        /* Ship body (triangle) */
        playerPoints[0].x = playerX;  /* Top point */
        playerPoints[0].y = playerY - PLAYER_SIZE / 2;
        playerPoints[1].x = playerX - PLAYER_SIZE / 2;  /* Bottom left */
        playerPoints[1].y = playerY + PLAYER_SIZE / 2;
        playerPoints[2].x = playerX + PLAYER_SIZE / 2;  /* Bottom right */
        playerPoints[2].y = playerY + PLAYER_SIZE / 2;
        
        playerBrush = CreateSolidBrush(RGB(30, 144, 255));  /* Dodger blue */
        SelectObject(hdc, playerBrush);
        Polygon(hdc, playerPoints, 3);
        
        /* Draw engine glow (small red rectangle at bottom) */
        engineBrush = CreateSolidBrush(RGB(255, 50, 50));
        SelectObject(hdc, engineBrush);
        rect.left = playerX - 5;
        rect.top = playerY + PLAYER_SIZE / 2;
        rect.right = playerX + 5;
        rect.bottom = playerY + PLAYER_SIZE / 2 + 5;
        FillRect(hdc, &rect, engineBrush);
        
        /* Draw cockpit (small white circle near top) */
        SelectObject(hdc, GetStockObject(WHITE_BRUSH));
        Ellipse(hdc, playerX - 3, playerY - 5, playerX + 3, playerY + 1);
        
        DeleteObject(engineBrush);
        DeleteObject(playerBrush);
    }

    /* Draw enemies */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            /* Choose color based on enemy type and add variations */
            if (enemies[i].type == 0) {
                /* Basic enemy - green with variations */
                int colorVar = enemies[i].width - ENEMY_SIZE;  /* Base variation on size */
                
                /* Create color variation based on ship size */
                if (colorVar < 0) {
                    /* Smaller ships - blueish green */
                    enemyBrush = CreateSolidBrush(RGB(0, 180 + colorVar, 120 - colorVar));
                } else if (colorVar > 0) {
                    /* Larger ships - yellowish green */
                    enemyBrush = CreateSolidBrush(RGB(colorVar * 15, 200, 0));
                } else {
                    /* Standard ships - green */
                    enemyBrush = CreateSolidBrush(RGB(0, 200, 0));
                }
            } else if (enemies[i].type == 1) {
                /* Shooting enemy - with color variations based on size */
                int colorVar = enemies[i].width - (ENEMY_SIZE + 5);
                
                /* Create color variations based on ship size */
                if (colorVar < 0) {
                    /* Smaller ships - orange */
                    enemyBrush = CreateSolidBrush(RGB(220, 120 + colorVar, 0));
                } else if (colorVar > 0) {
                    /* Larger ships - bright yellow */
                    enemyBrush = CreateSolidBrush(RGB(220 + (colorVar * 3), 220, colorVar * 10));
                } else {
                    /* Standard ships - yellow */
                    enemyBrush = CreateSolidBrush(RGB(200, 200, 0));
                }
            } else if (enemies[i].type == 2) {
                /* Boss - red with variations */
                int colorVar = enemies[i].width - (ENEMY_SIZE * 2);
                
                /* Larger bosses are more intense red/purple */
                if (colorVar > 0) {
                    enemyBrush = CreateSolidBrush(RGB(220, 0, colorVar * 10));
                } else {
                    /* Standard boss - red */
                    enemyBrush = CreateSolidBrush(RGB(200, 0, 0));
                }
            } else if (enemies[i].type == MOTHERSHIP_TYPE) {
                /* Mothership - BRIGHT purple/red with strong pulsing effect */
                int pulseEffect = (frameCount % 20) / 2;  /* 0-10 pulsing effect value */
                
                /* Create pulsing bright color that's very different from other ships */
                enemyBrush = CreateSolidBrush(RGB(
                    180 + pulseEffect * 7,  /* Strong red */
                    50 + pulseEffect * 3,   /* Low green */
                    220 - pulseEffect * 5   /* High blue with inverse pulse */
                ));
            }
            
            SelectObject(hdc, enemyBrush);
            
            /* Draw enemy shape based on type */
            if (enemies[i].type == 0) {
                /* Basic enemy - sharp aggressive fighter */
                POINT shipPoints[7];
                HBRUSH cockpitBrush, engineBrush;
                
                /* Main body - elongated pointed craft */
                shipPoints[0].x = enemies[i].x;  /* Front tip - extra pointy */
                shipPoints[0].y = enemies[i].y - enemies[i].height*2/3;
                
                shipPoints[1].x = enemies[i].x + enemies[i].width/4;  /* Upper right corner */
                shipPoints[1].y = enemies[i].y - enemies[i].height/5;
                
                shipPoints[2].x = enemies[i].x + enemies[i].width/2;  /* Right wing tip - sharp */
                shipPoints[2].y = enemies[i].y;
                
                shipPoints[3].x = enemies[i].x + enemies[i].width/4;  /* Lower right */
                shipPoints[3].y = enemies[i].y + enemies[i].height/3;
                
                shipPoints[4].x = enemies[i].x;  /* Bottom/engine */
                shipPoints[4].y = enemies[i].y + enemies[i].height/2;
                
                shipPoints[5].x = enemies[i].x - enemies[i].width/4;  /* Lower left */
                shipPoints[5].y = enemies[i].y + enemies[i].height/3;
                
                shipPoints[6].x = enemies[i].x - enemies[i].width/2;  /* Left wing tip - sharp */
                shipPoints[6].y = enemies[i].y;
                
                /* Fill the ship body */
                Polygon(hdc, shipPoints, 7);
                
                /* Add menacing cockpit - red eye-like glow */
                cockpitBrush = CreateSolidBrush(RGB(255, 50, 50));
                SelectObject(hdc, cockpitBrush);
                
                /* Narrow slit cockpit - looks like evil eye */
                rect.left = enemies[i].x - 7;
                rect.top = enemies[i].y - enemies[i].height/4;
                rect.right = enemies[i].x + 7;
                rect.bottom = enemies[i].y - enemies[i].height/4 + 3;
                FillRect(hdc, &rect, cockpitBrush);
                
                /* Add jagged engine exhaust */
                engineBrush = CreateSolidBrush(RGB(255, 120, 0));
                SelectObject(hdc, engineBrush);
                
                /* Jagged flame effect */
                {
                    POINT flamePoints[5];
                    flamePoints[0].x = enemies[i].x;
                    flamePoints[0].y = enemies[i].y + enemies[i].height/2;
                    flamePoints[1].x = enemies[i].x + 5;
                    flamePoints[1].y = enemies[i].y + enemies[i].height/2 + 8;
                    flamePoints[2].x = enemies[i].x;
                    flamePoints[2].y = enemies[i].y + enemies[i].height/2 + 4;
                    flamePoints[3].x = enemies[i].x - 5;
                    flamePoints[3].y = enemies[i].y + enemies[i].height/2 + 8;
                    flamePoints[4].x = enemies[i].x;
                    flamePoints[4].y = enemies[i].y + enemies[i].height/2;
                    Polygon(hdc, flamePoints, 5);
                }
                
                DeleteObject(engineBrush);
                DeleteObject(cockpitBrush);
            } else if (enemies[i].type == 1) {
                /* Shooting enemy - intimidating predator-like craft */
                POINT enemyPoints[9];
                HBRUSH engineBrush, weaponBrush;
                
                /* Main body - more aggressive and angular - like a predator */
                enemyPoints[0].x = enemies[i].x;  /* Front tip - very sharp */
                enemyPoints[0].y = enemies[i].y - enemies[i].height*2/3;
                
                enemyPoints[1].x = enemies[i].x + enemies[i].width/6;  /* Upper right neck */
                enemyPoints[1].y = enemies[i].y - enemies[i].height/3;
                
                enemyPoints[2].x = enemies[i].x + enemies[i].width/2;  /* Right wing - sharp */
                enemyPoints[2].y = enemies[i].y - enemies[i].height/5;
                
                enemyPoints[3].x = enemies[i].x + enemies[i].width/3;  /* Mid right */
                enemyPoints[3].y = enemies[i].y + enemies[i].height/5;
                
                enemyPoints[4].x = enemies[i].x + enemies[i].width/3;  /* Lower right wing */
                enemyPoints[4].y = enemies[i].y + enemies[i].height/2;
                
                enemyPoints[5].x = enemies[i].x;  /* Engine exhaust point */
                enemyPoints[5].y = enemies[i].y + enemies[i].height/3;
                
                enemyPoints[6].x = enemies[i].x - enemies[i].width/3;  /* Lower left wing */
                enemyPoints[6].y = enemies[i].y + enemies[i].height/2;
                
                enemyPoints[7].x = enemies[i].x - enemies[i].width/3;  /* Mid left */
                enemyPoints[7].y = enemies[i].y + enemies[i].height/5;
                
                enemyPoints[8].x = enemies[i].x - enemies[i].width/2;  /* Left wing - sharp */
                enemyPoints[8].y = enemies[i].y - enemies[i].height/5;
                
                Polygon(hdc, enemyPoints, 9);
                
                /* Add scary-looking engine glow */
                engineBrush = CreateSolidBrush(RGB(255, 50, 0));
                SelectObject(hdc, engineBrush);
                
                /* Create triangle flame effect */
                {
                    POINT flamePoints[3];
                    flamePoints[0].x = enemies[i].x;
                    flamePoints[0].y = enemies[i].y + enemies[i].height/3;
                    flamePoints[1].x = enemies[i].x - 8;
                    flamePoints[1].y = enemies[i].y + enemies[i].height/2 + 8;
                    flamePoints[2].x = enemies[i].x + 8;
                    flamePoints[2].y = enemies[i].y + enemies[i].height/2 + 8;
                    
                    Polygon(hdc, flamePoints, 3);
                }
                
                /* Add menacing weapon mounts */
                weaponBrush = CreateSolidBrush(RGB(60, 60, 60));
                SelectObject(hdc, weaponBrush);
                
                /* Gun barrels projecting forward */
                {
                    POINT leftGun[4], rightGun[4];
                    
                    /* Left gun */
                    leftGun[0].x = enemies[i].x - enemies[i].width/4;
                    leftGun[0].y = enemies[i].y - enemies[i].height/5;
                    leftGun[1].x = enemies[i].x - enemies[i].width/6;
                    leftGun[1].y = enemies[i].y - enemies[i].height/5;
                    leftGun[2].x = enemies[i].x - enemies[i].width/8;
                    leftGun[2].y = enemies[i].y - enemies[i].height/2;
                    leftGun[3].x = enemies[i].x - enemies[i].width/5;
                    leftGun[3].y = enemies[i].y - enemies[i].height/2;
                    Polygon(hdc, leftGun, 4);
                    
                    /* Right gun */
                    rightGun[0].x = enemies[i].x + enemies[i].width/4;
                    rightGun[0].y = enemies[i].y - enemies[i].height/5;
                    rightGun[1].x = enemies[i].x + enemies[i].width/6;
                    rightGun[1].y = enemies[i].y - enemies[i].height/5;
                    rightGun[2].x = enemies[i].x + enemies[i].width/8;
                    rightGun[2].y = enemies[i].y - enemies[i].height/2;
                    rightGun[3].x = enemies[i].x + enemies[i].width/5;
                    rightGun[3].y = enemies[i].y - enemies[i].height/2;
                    Polygon(hdc, rightGun, 4);
                }
                
                /* Add red tips to guns - looks like charging weapons */
                SelectObject(hdc, CreateSolidBrush(RGB(255, 0, 0)));
                Ellipse(hdc, enemies[i].x - enemies[i].width/5 - 2, 
                        enemies[i].y - enemies[i].height/2 - 5,
                        enemies[i].x - enemies[i].width/8 + 2, 
                        enemies[i].y - enemies[i].height/2);
                
                Ellipse(hdc, enemies[i].x + enemies[i].width/8 - 2, 
                        enemies[i].y - enemies[i].height/2 - 5,
                        enemies[i].x + enemies[i].width/5 + 2, 
                        enemies[i].y - enemies[i].height/2);
                
                DeleteObject(weaponBrush);
                DeleteObject(engineBrush);
            } else if (enemies[i].type == 2) {
                /* Boss - terrifying battle cruiser with sharp angles */
                POINT bossPoints[10];
                HBRUSH gunBrush, engineBrush, detailBrush;
                int pulseEffect = (frameCount % 20) / 2;  /* 0-10 pulsing effect */
                
                /* Main body - very angular, elongated battle cruiser */
                bossPoints[0].x = enemies[i].x;  /* Front tip - extra sharp */
                bossPoints[0].y = enemies[i].y - enemies[i].height*2/3;
                
                bossPoints[1].x = enemies[i].x + enemies[i].width/5;  /* Upper right hull */
                bossPoints[1].y = enemies[i].y - enemies[i].height/3;
                
                bossPoints[2].x = enemies[i].x + enemies[i].width/2;  /* Right forward wing */
                bossPoints[2].y = enemies[i].y - enemies[i].height/4;
                
                bossPoints[3].x = enemies[i].x + enemies[i].width*2/3;  /* Right mid wing */
                bossPoints[3].y = enemies[i].y;
                
                bossPoints[4].x = enemies[i].x + enemies[i].width/3;  /* Right rear */
                bossPoints[4].y = enemies[i].y + enemies[i].height/3;
                
                bossPoints[5].x = enemies[i].x;  /* Rear point */
                bossPoints[5].y = enemies[i].y + enemies[i].height/2;
                
                bossPoints[6].x = enemies[i].x - enemies[i].width/3;  /* Left rear */
                bossPoints[6].y = enemies[i].y + enemies[i].height/3;
                
                bossPoints[7].x = enemies[i].x - enemies[i].width*2/3;  /* Left mid wing */
                bossPoints[7].y = enemies[i].y;
                
                bossPoints[8].x = enemies[i].x - enemies[i].width/2;  /* Left forward wing */
                bossPoints[8].y = enemies[i].y - enemies[i].height/4;
                
                bossPoints[9].x = enemies[i].x - enemies[i].width/5;  /* Upper left hull */
                bossPoints[9].y = enemies[i].y - enemies[i].height/3;
                
                Polygon(hdc, bossPoints, 10);
                
                /* Add menacing command bridge - angular design */
                detailBrush = CreateSolidBrush(RGB(100, 100 + pulseEffect * 5, 200));
                SelectObject(hdc, detailBrush);
                
                /* Angular command deck - trapezoid shape */
                {
                    POINT bridgePoints[4];
                    bridgePoints[0].x = enemies[i].x - enemies[i].width/8;
                    bridgePoints[0].y = enemies[i].y - enemies[i].height/4;
                    bridgePoints[1].x = enemies[i].x + enemies[i].width/8;
                    bridgePoints[1].y = enemies[i].y - enemies[i].height/4;
                    bridgePoints[2].x = enemies[i].x + enemies[i].width/10;
                    bridgePoints[2].y = enemies[i].y - enemies[i].height/2;
                    bridgePoints[3].x = enemies[i].x - enemies[i].width/10;
                    bridgePoints[3].y = enemies[i].y - enemies[i].height/2;
                    
                    Polygon(hdc, bridgePoints, 4);
                }
                
                /* Add menacing looking weapon arrays */
                gunBrush = CreateSolidBrush(RGB(70, 70, 70));
                SelectObject(hdc, gunBrush);
                
                /* Left cannon - aggressive angular shape */
                {
                    POINT leftCannon[4];
                    leftCannon[0].x = enemies[i].x - enemies[i].width/2;
                    leftCannon[0].y = enemies[i].y - enemies[i].height/5;
                    leftCannon[1].x = enemies[i].x - enemies[i].width/4;
                    leftCannon[1].y = enemies[i].y - enemies[i].height/5;
                    leftCannon[2].x = enemies[i].x - enemies[i].width/3;
                    leftCannon[2].y = enemies[i].y + enemies[i].height/5;
                    leftCannon[3].x = enemies[i].x - enemies[i].width*2/3;
                    leftCannon[3].y = enemies[i].y + enemies[i].height/5;
                    Polygon(hdc, leftCannon, 4);
                }
                
                /* Right cannon - aggressive angular shape */
                {
                    POINT rightCannon[4];
                    rightCannon[0].x = enemies[i].x + enemies[i].width/2;
                    rightCannon[0].y = enemies[i].y - enemies[i].height/5;
                    rightCannon[1].x = enemies[i].x + enemies[i].width/4;
                    rightCannon[1].y = enemies[i].y - enemies[i].height/5;
                    rightCannon[2].x = enemies[i].x + enemies[i].width/3;
                    rightCannon[2].y = enemies[i].y + enemies[i].height/5;
                    rightCannon[3].x = enemies[i].x + enemies[i].width*2/3;
                    rightCannon[3].y = enemies[i].y + enemies[i].height/5;
                    Polygon(hdc, rightCannon, 4);
                }
                
                /* Add glowing red weapon tips */
                SelectObject(hdc, CreateSolidBrush(RGB(255, 0, 0)));
                Ellipse(hdc, enemies[i].x - enemies[i].width*2/3 - 4, 
                       enemies[i].y + enemies[i].height/5 - 2,
                       enemies[i].x - enemies[i].width*2/3 + 4, 
                       enemies[i].y + enemies[i].height/5 + 6);
                
                Ellipse(hdc, enemies[i].x + enemies[i].width*2/3 - 4, 
                       enemies[i].y + enemies[i].height/5 - 2,
                       enemies[i].x + enemies[i].width*2/3 + 4, 
                       enemies[i].y + enemies[i].height/5 + 6);
                
                /* Engine jets - jagged flames */
                engineBrush = CreateSolidBrush(RGB(255, 100, 0));
                SelectObject(hdc, engineBrush);
                
                /* Left engine flame - jagged design */
                {
                    POINT leftFlame[5];
                    leftFlame[0].x = enemies[i].x - enemies[i].width/5;
                    leftFlame[0].y = enemies[i].y + enemies[i].height/3;
                    leftFlame[1].x = enemies[i].x - enemies[i].width/4;
                    leftFlame[1].y = enemies[i].y + enemies[i].height/2 + 8;
                    leftFlame[2].x = enemies[i].x - enemies[i].width/6;
                    leftFlame[2].y = enemies[i].y + enemies[i].height/3 + 5;
                    leftFlame[3].x = enemies[i].x - enemies[i].width/8;
                    leftFlame[3].y = enemies[i].y + enemies[i].height/2 + 10;
                    leftFlame[4].x = enemies[i].x - enemies[i].width/10;
                    leftFlame[4].y = enemies[i].y + enemies[i].height/3;
                    
                    Polygon(hdc, leftFlame, 5);
                }
                
                /* Right engine flame - jagged design */
                {
                    POINT rightFlame[5];
                    rightFlame[0].x = enemies[i].x + enemies[i].width/5;
                    rightFlame[0].y = enemies[i].y + enemies[i].height/3;
                    rightFlame[1].x = enemies[i].x + enemies[i].width/4;
                    rightFlame[1].y = enemies[i].y + enemies[i].height/2 + 8;
                    rightFlame[2].x = enemies[i].x + enemies[i].width/6;
                    rightFlame[2].y = enemies[i].y + enemies[i].height/3 + 5;
                    rightFlame[3].x = enemies[i].x + enemies[i].width/8;
                    rightFlame[3].y = enemies[i].y + enemies[i].height/2 + 10;
                    rightFlame[4].x = enemies[i].x + enemies[i].width/10;
                    rightFlame[4].y = enemies[i].y + enemies[i].height/3;
                    
                    Polygon(hdc, rightFlame, 5);
                }
                
                DeleteObject(engineBrush);
                DeleteObject(gunBrush);
                DeleteObject(detailBrush);
            } else if (enemies[i].type == MOTHERSHIP_TYPE) {
                /* ALIEN DREADNOUGHT - massive warship with sharp, threatening angles */
                HBRUSH detailBrush, engineBrush, weaponBrush, beamBrush, glowBrush;
                HPEN outlinePen;
                int j, dx, dy;
                int pulseEffect = (frameCount % 20) / 2;  /* 0-10 pulsing effect */
                POINT outlinePoints[6];
                POINT outerPoints[6];
                
                /* Main dreadnought body - elongated, jagged star shape */
                {
                    POINT shipBody[12];
                    /* Front point - extra long and sharp */
                    shipBody[0].x = enemies[i].x;
                    shipBody[0].y = enemies[i].y - enemies[i].height*3/4;
                
                    /* Upper right angle */
                    shipBody[1].x = enemies[i].x + enemies[i].width/5;
                    shipBody[1].y = enemies[i].y - enemies[i].height/3;
                    
                    /* Upper right spike */
                    shipBody[2].x = enemies[i].x + enemies[i].width*2/3;
                    shipBody[2].y = enemies[i].y - enemies[i].height/2;
                    
                    /* Mid right */
                    shipBody[3].x = enemies[i].x + enemies[i].width/3;
                    shipBody[3].y = enemies[i].y - enemies[i].height/6;
                    
                    /* Right side spike */
                    shipBody[4].x = enemies[i].x + enemies[i].width;
                    shipBody[4].y = enemies[i].y;
                    
                    /* Lower right */
                    shipBody[5].x = enemies[i].x + enemies[i].width/2;
                    shipBody[5].y = enemies[i].y + enemies[i].height/3;
                    
                    /* Rear point */
                    shipBody[6].x = enemies[i].x;
                    shipBody[6].y = enemies[i].y + enemies[i].height/2;
                    
                    /* Lower left */
                    shipBody[7].x = enemies[i].x - enemies[i].width/2;
                    shipBody[7].y = enemies[i].y + enemies[i].height/3;
                    
                    /* Left side spike */
                    shipBody[8].x = enemies[i].x - enemies[i].width;
                    shipBody[8].y = enemies[i].y;
                    
                    /* Mid left */
                    shipBody[9].x = enemies[i].x - enemies[i].width/3;
                    shipBody[9].y = enemies[i].y - enemies[i].height/6;
                    
                    /* Upper left spike */
                    shipBody[10].x = enemies[i].x - enemies[i].width*2/3;
                    shipBody[10].y = enemies[i].y - enemies[i].height/2;
                    
                    /* Upper left angle */
                    shipBody[11].x = enemies[i].x - enemies[i].width/5;
                    shipBody[11].y = enemies[i].y - enemies[i].height/3;
                    
                    /* Fill the mothership body */
                    Polygon(hdc, shipBody, 12);
                }
                
                /* Add terrifying "glow of doom" effect - angular and pulsing */
                glowBrush = CreateSolidBrush(RGB(255, 40 + pulseEffect * 8, 100 + pulseEffect * 5));
                SelectObject(hdc, glowBrush);
                
                /* Draw angular glow shape around ship */
                {
                    POINT glowPoints[8];
                    glowPoints[0].x = enemies[i].x;  /* Top */
                    glowPoints[0].y = enemies[i].y - enemies[i].height - 20;
                    glowPoints[1].x = enemies[i].x + enemies[i].width + 30;  /* Upper right */
                    glowPoints[1].y = enemies[i].y - enemies[i].height/3;
                    glowPoints[2].x = enemies[i].x + enemies[i].width*5/4;  /* Right */
                    glowPoints[2].y = enemies[i].y;
                    glowPoints[3].x = enemies[i].x + enemies[i].width/2;  /* Lower right */
                    glowPoints[3].y = enemies[i].y + enemies[i].height*3/4;
                    glowPoints[4].x = enemies[i].x;  /* Bottom */
                    glowPoints[4].y = enemies[i].y + enemies[i].height*2/3;
                    glowPoints[5].x = enemies[i].x - enemies[i].width/2;  /* Lower left */
                    glowPoints[5].y = enemies[i].y + enemies[i].height*3/4;
                    glowPoints[6].x = enemies[i].x - enemies[i].width*5/4;  /* Left */
                    glowPoints[6].y = enemies[i].y;
                    glowPoints[7].x = enemies[i].x - enemies[i].width - 30;  /* Upper left */
                    glowPoints[7].y = enemies[i].y - enemies[i].height/3;
                
                    SelectObject(hdc, GetStockObject(NULL_BRUSH));  /* Just outline */
                    outlinePen = CreatePen(PS_SOLID, 1 + pulseEffect/2, RGB(255, 0 + pulseEffect * 10, 80));
                    SelectObject(hdc, outlinePen);
                    Polygon(hdc, glowPoints, 8);
                }
                
                /* Central command spire - angular sharp design */
                SelectObject(hdc, GetStockObject(NULL_BRUSH));  /* Reset to no fill */
                DeleteObject(outlinePen);
                
                /* Now draw the command bridge - sharp pyramid */
                detailBrush = CreateSolidBrush(RGB(200, 0, 50 + pulseEffect * 10));
                SelectObject(hdc, detailBrush);
                
                {
                    POINT bridgePoints[5];
                    bridgePoints[0].x = enemies[i].x;  /* Top spike */
                    bridgePoints[0].y = enemies[i].y - enemies[i].height/2;
                    bridgePoints[1].x = enemies[i].x + enemies[i].width/6;  /* Upper right */
                    bridgePoints[1].y = enemies[i].y - enemies[i].height/4;
                    bridgePoints[2].x = enemies[i].x + enemies[i].width/10;  /* Lower right */
                    bridgePoints[2].y = enemies[i].y;
                    bridgePoints[3].x = enemies[i].x - enemies[i].width/10;  /* Lower left */
                    bridgePoints[3].y = enemies[i].y;
                    bridgePoints[4].x = enemies[i].x - enemies[i].width/6;  /* Upper left */
                    bridgePoints[4].y = enemies[i].y - enemies[i].height/4;
                    
                    Polygon(hdc, bridgePoints, 5);
                }
                
                /* Add menacing eye-like sensor array */
                SelectObject(hdc, CreateSolidBrush(RGB(255, 255, 0)));
                rect.left = enemies[i].x - enemies[i].width/5;
                rect.top = enemies[i].y - enemies[i].height/5;
                rect.right = enemies[i].x + enemies[i].width/5;
                rect.bottom = enemies[i].y - enemies[i].height/5 + 5;
                FillRect(hdc, &rect, CreateSolidBrush(RGB(255, 50, 0)));
                
                /* Add massive weapon arrays */
                weaponBrush = CreateSolidBrush(RGB(40, 40, 40));
                SelectObject(hdc, weaponBrush);
                
                /* Draw angular wings with weapon spikes */
                /* Left wing weapons */
                {
                    POINT leftWeapon[4];
                    leftWeapon[0].x = enemies[i].x - enemies[i].width/3;
                    leftWeapon[0].y = enemies[i].y - enemies[i].height/8;
                    leftWeapon[1].x = enemies[i].x - enemies[i].width*2/3;
                    leftWeapon[1].y = enemies[i].y + enemies[i].height/8;
                    leftWeapon[2].x = enemies[i].x - enemies[i].width*4/5;
                    leftWeapon[2].y = enemies[i].y + enemies[i].height/8;
                    leftWeapon[3].x = enemies[i].x - enemies[i].width/2;
                    leftWeapon[3].y = enemies[i].y - enemies[i].height/8;
                    Polygon(hdc, leftWeapon, 4);
                }
                
                /* Right wing weapons */
                {
                    POINT rightWeapon[4];
                    rightWeapon[0].x = enemies[i].x + enemies[i].width/3;
                    rightWeapon[0].y = enemies[i].y - enemies[i].height/8;
                    rightWeapon[1].x = enemies[i].x + enemies[i].width*2/3;
                    rightWeapon[1].y = enemies[i].y + enemies[i].height/8;
                    rightWeapon[2].x = enemies[i].x + enemies[i].width*4/5;
                    rightWeapon[2].y = enemies[i].y + enemies[i].height/8;
                    rightWeapon[3].x = enemies[i].x + enemies[i].width/2;
                    rightWeapon[3].y = enemies[i].y - enemies[i].height/8;
                    Polygon(hdc, rightWeapon, 4);
                }
                
                /* Add evil-looking red weapon glow at wing tips */
                for (j = -1; j <= 1; j += 2) {  /* -1 = left, 1 = right */
                    int xPos = enemies[i].x + (j * enemies[i].width*4/5);
                    int yPos = enemies[i].y + enemies[i].height/8;
                    
                    SelectObject(hdc, CreateSolidBrush(RGB(255, 0, 0)));
                    Ellipse(hdc, xPos - 5, yPos - 3, xPos + 5, yPos + 3);
                    
                    /* Add secondary smaller red glow */
                    SelectObject(hdc, CreateSolidBrush(RGB(255, 100, 0)));
                    Ellipse(hdc, xPos - 3, yPos - 2, xPos + 3, yPos + 2);
                }
                
                /* Add massive, intimidating engine exhausts */
                engineBrush = CreateSolidBrush(RGB(255, 80 + pulseEffect * 10, 0));
                SelectObject(hdc, engineBrush);
                
                /* Draw multiple large, jagged flame arrays */
                for (j = -2; j <= 2; j++) {
                    /* Position across the back of the ship */
                    int xPos = enemies[i].x + (j * (enemies[i].width/8));
                    int engineWidth = 10 + (abs(j) == 2 ? 0 : 5); /* Center engines larger */
                    
                    /* Jagged flame shape */
                    POINT flamePoints[5];
                    flamePoints[0].x = xPos;
                    flamePoints[0].y = enemies[i].y + enemies[i].height/3;
                    flamePoints[1].x = xPos + engineWidth;
                    flamePoints[1].y = enemies[i].y + enemies[i].height/2 + 10 + (pulseEffect * 2);
                    flamePoints[2].x = xPos;
                    flamePoints[2].y = enemies[i].y + enemies[i].height/2;
                    flamePoints[3].x = xPos - engineWidth;
                    flamePoints[3].y = enemies[i].y + enemies[i].height/2 + 10 + (pulseEffect * 2);
                    flamePoints[4].x = xPos;
                    flamePoints[4].y = enemies[i].y + enemies[i].height/3;
                    
                    Polygon(hdc, flamePoints, 5);
                }
                
                /* Add energy beam weapons if actively firing */
                if ((frameCount + enemies[i].x) % 30 < 15) {
                    /* Only show beams during part of the cycle - looks like charging/firing */
                    beamBrush = CreateSolidBrush(RGB(255, 0, 170)); /* Deep magenta beam */
                    SelectObject(hdc, beamBrush);
                    
                    /* Draw multiple beam weapons */
                    for (j = -1; j <= 1; j += 2) {
                        /* Left and right beams */
                        POINT beamPoints[4];
                        int beamX = enemies[i].x + (j * enemies[i].width/3);
                        int beamWidth = enemies[i].width / 15;
                        
                        /* Angled jagged beam shape */
                        beamPoints[0].x = beamX;
                        beamPoints[0].y = enemies[i].y;
                        beamPoints[1].x = beamX + beamWidth;
                        beamPoints[1].y = enemies[i].y + enemies[i].height*2;
                        beamPoints[2].x = beamX - beamWidth;
                        beamPoints[2].y = enemies[i].y + enemies[i].height*2;
                        beamPoints[3].x = beamX;
                        beamPoints[3].y = enemies[i].y;
                        
                        Polygon(hdc, beamPoints, 4);
                    }
                    DeleteObject(beamBrush);
                }
                
                /* Now draw the outlines for the mothership */
                
                /* Add dramatic pulsing outline */
                outlinePen = CreatePen(PS_SOLID, 2 + pulseEffect/2, RGB(255, 100, 255));
                SelectObject(hdc, outlinePen);
                
                /* Draw outline around main body - using a simple hexagon instead */
                /* Simplified outline points */
                outlinePoints[0].x = enemies[i].x;
                outlinePoints[0].y = enemies[i].y - enemies[i].height*2/3;
                outlinePoints[1].x = enemies[i].x + enemies[i].width/2;
                outlinePoints[1].y = enemies[i].y - enemies[i].height/4;
                outlinePoints[2].x = enemies[i].x + enemies[i].width/2;
                outlinePoints[2].y = enemies[i].y + enemies[i].height/4;
                outlinePoints[3].x = enemies[i].x;
                outlinePoints[3].y = enemies[i].y + enemies[i].height/2;
                outlinePoints[4].x = enemies[i].x - enemies[i].width/2;
                outlinePoints[4].y = enemies[i].y + enemies[i].height/4;
                outlinePoints[5].x = enemies[i].x - enemies[i].width/2;
                outlinePoints[5].y = enemies[i].y - enemies[i].height/4;
                
                SelectObject(hdc, GetStockObject(NULL_BRUSH));  /* No fill */
                Polygon(hdc, outlinePoints, 6);
                
                /* Draw second outline effect */
                DeleteObject(outlinePen);
                outlinePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                SelectObject(hdc, outlinePen);
                
                /* Draw an additional slightly larger outline */
                for (j = 0; j < 6; j++) {
                    /* Use the same outline points we created before */
                    dx = outlinePoints[j].x - enemies[i].x;
                    dy = outlinePoints[j].y - enemies[i].y;
                    
                    /* Scale outward slightly */
                    outerPoints[j].x = enemies[i].x + (int)(dx * 1.1);
                    outerPoints[j].y = enemies[i].y + (int)(dy * 1.1);
                }
                Polygon(hdc, outerPoints, 6);
                
                /* Cleanup */
                DeleteObject(outlinePen);
                DeleteObject(engineBrush);
                DeleteObject(weaponBrush);
                DeleteObject(detailBrush);
                DeleteObject(glowBrush);
            }
            
            DeleteObject(enemyBrush);
        }
    }

    /* Draw lasers */
    for (i = 0; i < MAX_LASERS; i++) {
        if (lasers[i].active) {
            if (lasers[i].isEnemy) {
                /* Enemy lasers - red */
                laserBrush = CreateSolidBrush(RGB(255, 50, 50));
            } else {
                /* Player lasers - blue */
                laserBrush = CreateSolidBrush(RGB(50, 50, 255));
            }
            
            SelectObject(hdc, laserBrush);
            rect.left = lasers[i].x - lasers[i].width/2;
            rect.top = lasers[i].y - lasers[i].height/2;
            rect.right = lasers[i].x + lasers[i].width/2;
            rect.bottom = lasers[i].y + lasers[i].height/2;
            FillRect(hdc, &rect, laserBrush);
            DeleteObject(laserBrush);
        }
    }

    /* Draw explosions */
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].active) {
            HBRUSH explosionBrush;
            
            if (explosions[i].type == 0) {
                /* Enemy explosion - orange */
                explosionBrush = CreateSolidBrush(RGB(255, 165, 0));
            } else {
                /* Player explosion - blue */
                explosionBrush = CreateSolidBrush(RGB(30, 144, 255));
            }
            
            SelectObject(hdc, explosionBrush);
            Ellipse(hdc, explosions[i].x - explosions[i].radius, explosions[i].y - explosions[i].radius,
                   explosions[i].x + explosions[i].radius, explosions[i].y + explosions[i].radius);
            DeleteObject(explosionBrush);
        }
    }
    
    /* Display Game Over message */
    if (gameStarted && gameOver) {
        char gameOverText[20] = "GAME OVER";
        char finalScoreText[30];
        char restartText[50] = "Click anywhere or press any key to restart";
        char exitText[30] = "Press ESC or Q to quit";
        HFONT hFont, hOldFont, smallFont;
        SIZE textSize;
        int textWidth, textX;
        
        /* Create a font for Game Over message */
        hFont = CreateFont(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                         ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
        hOldFont = SelectObject(hdc, hFont);
        
        /* Display Game Over text */
        SetTextColor(hdc, RGB(255, 50, 50));
        SetBkMode(hdc, TRANSPARENT);
        
        /* Center the text */
        GetTextExtentPoint32(hdc, gameOverText, lstrlen(gameOverText), &textSize);
        textWidth = textSize.cx;
        textX = (WINDOW_WIDTH - textWidth) / 2;
        TextOut(hdc, textX, WINDOW_HEIGHT / 2 - 50, gameOverText, lstrlen(gameOverText));
        
        /* Show final score */
        wsprintf(finalScoreText, "Final Score: %d", score);
        GetTextExtentPoint32(hdc, finalScoreText, lstrlen(finalScoreText), &textSize);
        textWidth = textSize.cx;
        textX = (WINDOW_WIDTH - textWidth) / 2;
        TextOut(hdc, textX, WINDOW_HEIGHT / 2 + 10, finalScoreText, lstrlen(finalScoreText));
        
        /* Create a smaller font for instructions */
        SelectObject(hdc, hOldFont);  /* Deselect the big font */
        smallFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
        SelectObject(hdc, smallFont);
        
        /* Instructions to restart */
        GetTextExtentPoint32(hdc, restartText, lstrlen(restartText), &textSize);
        textWidth = textSize.cx;
        textX = (WINDOW_WIDTH - textWidth) / 2;
        TextOut(hdc, textX, WINDOW_HEIGHT / 2 + 60, restartText, lstrlen(restartText));
        
        /* Exit instructions */
        GetTextExtentPoint32(hdc, exitText, lstrlen(exitText), &textSize);
        textWidth = textSize.cx;
        textX = (WINDOW_WIDTH - textWidth) / 2;
        TextOut(hdc, textX, WINDOW_HEIGHT / 2 + 90, exitText, lstrlen(exitText));
        
        /* Clean up fonts */
        SelectObject(hdc, hOldFont);  /* Restore original font */
        DeleteObject(smallFont);
        DeleteObject(hFont);
    }
}

void UpdateGame(void)
{
    int i, j;
    
    frameCount++;
    
    if (gameOver) {
        return;  /* Stop updating if game is over */
    }
    
    /* Update player shoot cooldown */
    if (shootCooldown > 0) {
        shootCooldown--;
    }
    
    /* Update stars for parallax scrolling effect */
    UpdateStars();
    
    /* Update explosions */
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].active) {
            explosions[i].radius += 1;
            if (explosions[i].radius >= explosions[i].maxRadius) {
                explosions[i].active = 0;
            }
        }
    }
    
    /* Update lasers */
    for (i = 0; i < MAX_LASERS; i++) {
        if (lasers[i].active) {
            if (lasers[i].isEnemy) {
                /* Check if this is an aimed laser */
                if (lasers[i].dx != 0 || lasers[i].dy != 0) {
                    /* Move in the specified direction */
                    double len = sqrt(lasers[i].dx * lasers[i].dx + lasers[i].dy * lasers[i].dy);
                    if (len > 0) {
                        /* Normalize and apply speed */
                        lasers[i].x += (int)((lasers[i].dx / len) * lasers[i].speed);
                        lasers[i].y += (int)((lasers[i].dy / len) * lasers[i].speed);
                    } else {
                        /* Fallback if direction vector is zero */
                        lasers[i].y += lasers[i].speed;
                    }
                } else {
                    /* Regular enemy lasers move straight down */
                    lasers[i].y += lasers[i].speed;
                }
                
                /* Check if laser is off screen */
                if (lasers[i].y > WINDOW_HEIGHT || lasers[i].x < 0 || lasers[i].x > WINDOW_WIDTH) {
                    lasers[i].active = 0;
                }
                
                /* Check if enemy laser hits player */
                if (!gameOver) {
                    int dx = lasers[i].x - playerX;
                    int dy = lasers[i].y - playerY;
                    int hitDistance = PLAYER_SIZE / 2 + lasers[i].width / 2;
                    
                    if (dx*dx + dy*dy < hitDistance*hitDistance) {
                        playerHealth--;
                        lasers[i].active = 0;
                        CreateExplosion(playerX, playerY, 1);
                        
                        if (playerHealth <= 0) {
                            gameOver = 1;
                        }
                    }
                }
            } else {
                /* Player lasers move up */
                lasers[i].y -= lasers[i].speed;
                if (lasers[i].y < 0) {
                    lasers[i].active = 0;
                }
                
                /* Check if player laser hits any enemy */
                for (j = 0; j < MAX_ENEMIES; j++) {
                    if (enemies[j].active) {
                        int dx = lasers[i].x - enemies[j].x;
                        int dy = lasers[i].y - enemies[j].y;
                        int hitDistance = enemies[j].width / 2 + lasers[i].width / 2;
                        
                        if (dx*dx + dy*dy < hitDistance*hitDistance) {
                            enemies[j].health--;
                            lasers[i].active = 0;
                            
                            /* Create small explosion at hit point */
                            CreateExplosion(lasers[i].x, lasers[i].y, 0);
                            
                            if (enemies[j].health <= 0) {
                                /* Award points based on enemy type */
                                score += enemies[j].scoreValue;
                                
                                /* Create larger explosion for destroyed enemy */
                                CreateExplosion(enemies[j].x, enemies[j].y, 0);
                                enemies[j].active = 0;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    
    /* Update enemies */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            /* Move down */
            enemies[i].y += SCROLL_SPEED;
            
            /* Check if reached bottom of screen */
            if (enemies[i].y > WINDOW_HEIGHT + enemies[i].height) {
                enemies[i].active = 0;
                continue;
            }
            
            /* Check for collision with player */
            if (!gameOver) {
                int dx = enemies[i].x - playerX;
                int dy = enemies[i].y - playerY;
                int hitDistance = PLAYER_SIZE / 2 + enemies[i].width / 2;
                
                if (dx*dx + dy*dy < hitDistance*hitDistance) {
                    playerHealth--;
                    CreateExplosion(enemies[i].x, enemies[i].y, 0);
                    enemies[i].active = 0;
                    
                    if (playerHealth <= 0) {
                        CreateExplosion(playerX, playerY, 1);
                        gameOver = 1;
                    }
                    continue;
                }
            }
            
            /* Enemy shooting logic */
            if (enemies[i].shootTimer <= 0) {
                /* All enemy types can shoot, but with different probabilities and patterns */
                int shouldShoot = 0;
                
                /* Type 0 (basic): occasionally shoot straight down */
                if (enemies[i].type == 0 && rand() % 100 < 5) {
                    shouldShoot = 1;
                    enemies[i].shootTimer = 90;  /* Basic: 1.5 seconds between shots */
                }
                /* Type 1 (shooter): frequently shoot aimed at player */
                else if (enemies[i].type == 1) {
                    shouldShoot = 1;
                    enemies[i].shootTimer = 45;  /* Shooter: 0.75 seconds between shots */
                }
                /* Type 2 (boss): constant shooting with spread pattern */
                else if (enemies[i].type == 2) {
                    shouldShoot = 1;
                    enemies[i].shootTimer = 20;  /* Boss: 0.33 seconds between shots */
                }
                /* Type 3 (mothership): constant heavy barrage */
                else if (enemies[i].type == MOTHERSHIP_TYPE) {
                    shouldShoot = 1;
                    enemies[i].shootTimer = 15;  /* Mothership: 0.25 seconds between shots - rapid fire */
                }
                
                if (shouldShoot && !gameOver) {
                    /* Aim at player for type 1 and 2 enemies */
                    if (enemies[i].type >= 1 && enemies[i].type <= 2) {
                        /* Calculate direction to player */
                        int dx = playerX - enemies[i].x;
                        int dy = playerY - enemies[i].y;
                        double angle = atan2((double)dy, (double)dx);
                        
                        /* Convert to x,y offset */
                        int offsetX = (int)(sin(angle) * 10);
                        int offsetY = (int)(cos(angle) * 10);
                        
                        /* Shoot in player's direction */
                        ShootAimedLaser(enemies[i].x, enemies[i].y + enemies[i].height/2, offsetX, offsetY, 1);
                    } else if (enemies[i].type == MOTHERSHIP_TYPE) {
                        /* Mothership special attack patterns */
                        int patternType = (frameCount / 60) % 3;  /* Cycle through 3 attack patterns */
                        
                        switch (patternType) {
                            case 0:  /* Radial pattern */
                                {
                                    /* Shoot in 6 directions */
                                    int j;
                                    double angle;
                                    int offsetX, offsetY;
                                    
                                    for (j = 0; j < 6; j++) {
                                        angle = j * 3.14159 / 3;  /* 60 degree spacing */
                                        /* Slightly randomize angles for variety */
                                        angle += (rand() % 10 - 5) * 0.01;
                                        
                                        offsetX = (int)(sin(angle) * 10);
                                        offsetY = (int)(cos(angle) * 10);
                                        
                                        ShootAimedLaser(enemies[i].x, enemies[i].y + enemies[i].height/3, 
                                                      offsetX, offsetY, 1);
                                    }
                                }
                                break;
                                
                            case 1:  /* Aimed triple shot */
                                {
                                    /* Calculate direction to player */
                                    int dx, dy;
                                    double angle;
                                    int offsetX, offsetY;
                                    
                                    dx = playerX - enemies[i].x;
                                    dy = playerY - enemies[i].y;
                                    angle = atan2((double)dy, (double)dx);
                                    
                                    /* Main shot at player */
                                    offsetX = (int)(sin(angle) * 10);
                                    offsetY = (int)(cos(angle) * 10);
                                    ShootAimedLaser(enemies[i].x, enemies[i].y + enemies[i].height/3, 
                                                   offsetX, offsetY, 1);
                                    
                                    /* Side shots at slight angles */
                                    ShootAimedLaser(enemies[i].x - enemies[i].width/4, enemies[i].y + enemies[i].height/3, 
                                                   offsetX - 2, offsetY, 1);
                                    ShootAimedLaser(enemies[i].x + enemies[i].width/4, enemies[i].y + enemies[i].height/3, 
                                                   offsetX + 2, offsetY, 1);
                                }
                                break;
                                
                            case 2:  /* Sweep pattern */
                                {
                                    /* Create a sweeping laser pattern */
                                    int sweepPos;
                                    double sweepAngle;
                                    int offsetX, offsetY;
                                    
                                    sweepPos = frameCount % 40;  /* 0-39 sweep position */
                                    sweepAngle = (sweepPos - 20) * 0.05;  /* -1.0 to 1.0 radians */
                                    
                                    offsetX = (int)(sin(sweepAngle) * 10);
                                    offsetY = 10;  /* Mainly downward */
                                    
                                    /* Multiple parallel beams */
                                    ShootAimedLaser(enemies[i].x - enemies[i].width/3, enemies[i].y + enemies[i].height/3, 
                                                   offsetX, offsetY, 1);
                                    ShootAimedLaser(enemies[i].x, enemies[i].y + enemies[i].height/3, 
                                                   offsetX, offsetY, 1);
                                    ShootAimedLaser(enemies[i].x + enemies[i].width/3, enemies[i].y + enemies[i].height/3, 
                                                   offsetX, offsetY, 1);
                                }
                                break;
                        }
                    } else {
                        /* Basic enemies just shoot straight down */
                        ShootLaser(enemies[i].x, enemies[i].y + enemies[i].height/2, 1);
                    }
                    
                    /* Boss shoots additional lasers in spread pattern */
                    if (enemies[i].type == 2) {
                        ShootLaser(enemies[i].x - 15, enemies[i].y + enemies[i].height/2, 1);
                        ShootLaser(enemies[i].x + 15, enemies[i].y + enemies[i].height/2, 1);
                    }
                }
            }
            
            /* Decrease shoot timer */
            if (enemies[i].shootTimer > 0) {
                enemies[i].shootTimer--;
            }
        }
    }
    
    /* Spawn new enemies */
    enemySpawnTimer--;
    if (enemySpawnTimer <= 0) {
        /* Determine enemy type based on score and random chance */
        int type = 0;  /* Default: basic enemy */
        int r = rand() % 100;
        
        if ((score > 1000 && r < 20) || r < 5) {
            /* For testing: 20% chance after 1000 points or 5% chance always */
            type = MOTHERSHIP_TYPE;
            enemySpawnTimer = 300;  /* 5 second delay after mothership */
        } else if (score > 5000 && r < 5) {
            /* 5% chance for boss after 5000 points */
            type = 2;
            enemySpawnTimer = 300;  /* Longer delay after boss */
        } else if (score > 1000 && r < 25) {
            /* 25% chance for shooter after 1000 points */
            type = 1;
            enemySpawnTimer = 90;  /* 1.5 second delay */
        } else {
            /* Basic enemy */
            type = 0;
            enemySpawnTimer = 45;  /* 0.75 second delay */
        }
        
        SpawnEnemy(type);
    }
    
    /* Level up based on score */
    {
        int newLevel = 1 + score / 1000;
        if (newLevel > level) {
            level = newLevel;
        /* Give player extra health on level up */
        if (playerHealth < 5) {  /* Cap health at 5 */
            playerHealth++;
        }
    }
    }
}

void SpawnEnemy(int type)
{
    int i;
    int width, height, health, scoreValue;
    int sizeVariation;
    
    /* Random size variation - make some smaller, some larger */
    sizeVariation = (rand() % 11) - 5;  /* -5 to +5 size variation */
    
    /* Set properties based on enemy type */
    if (type == 0) {
        /* Basic enemy - flying saucer with size variations */
        width = height = ENEMY_SIZE + sizeVariation;
        
        /* Ensure minimum size */
        if (width < ENEMY_SIZE - 5) width = height = ENEMY_SIZE - 5;
        
        health = 1;
        scoreValue = 10;
        
        /* Larger ships are worth more points and have more health */
        if (width > ENEMY_SIZE) {
            health = 2;
            scoreValue = 15;
        }
    } else if (type == 1) {
        /* Shooting enemy - varied sizes */
        width = height = (ENEMY_SIZE + 5) + sizeVariation;
        
        /* Ensure minimum size */
        if (width < ENEMY_SIZE) width = height = ENEMY_SIZE;
        
        health = 2;
        scoreValue = 20;
        
        /* Larger ships have more health and are worth more */
        if (width > ENEMY_SIZE + 5) {
            health = 3;
            scoreValue = 25;
        }
    } else if (type == 2) {
        /* Boss - some variation but always large */
        sizeVariation = (rand() % 11) - 3;  /* Less variation for bosses, -3 to +7 */
        width = (ENEMY_SIZE * 2) + sizeVariation;
        height = (ENEMY_SIZE * 2) + sizeVariation;
        
        /* Ensure minimum boss size */
        if (width < ENEMY_SIZE * 1.5) width = height = (int)(ENEMY_SIZE * 1.5);
        
        health = 8 + (width - ENEMY_SIZE * 2) / 2;  /* Base health + bonus for size */
        scoreValue = 100 + (width - ENEMY_SIZE * 2) * 5;  /* Base score + bonus for size */
    } else if (type == MOTHERSHIP_TYPE) {
        /* Mothership - massive, rare ship with some size variation */
        sizeVariation = (rand() % 21) - 10;  /* -10 to +10 variation, but still huge */
        
        /* Mothership is 3-4x regular enemy size */
        width = (ENEMY_SIZE * 3) + sizeVariation;
        
        /* Make it wider than tall for that classic mothership look */
        height = (int)(width * 0.75);
        
        /* Very tough with lots of health */
        health = 20 + (width - ENEMY_SIZE * 3) / 2;
        
        /* Worth many points */
        scoreValue = 500 + (width - ENEMY_SIZE * 3) * 10;
    }
    
    /* Find an inactive enemy slot */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].x = rand() % (WINDOW_WIDTH - width) + width/2;
            enemies[i].y = -height;
            enemies[i].width = width;
            enemies[i].height = height;
            enemies[i].active = 1;
            enemies[i].type = type;
            enemies[i].health = health;
            enemies[i].scoreValue = scoreValue;
            enemies[i].shootTimer = rand() % 60;  /* Random initial shoot delay */
            break;
        }
    }
}

void ShootLaser(int x, int y, int isEnemy)
{
    int i;
    int width, height, speed;
    
    /* Only allow player to shoot if cooldown is zero */
    if (!isEnemy && shootCooldown > 0) {
        return;
    }
    
    /* Set properties based on laser type */
    if (isEnemy) {
        width = LASER_WIDTH;
        height = LASER_HEIGHT;
        speed = ENEMY_LASER_SPEED;
    } else {
        width = LASER_WIDTH;
        height = LASER_HEIGHT;
        speed = PLAYER_LASER_SPEED;
        shootCooldown = 10;  /* Set cooldown for player shots */
    }
    
    /* Find an inactive laser slot */
    for (i = 0; i < MAX_LASERS; i++) {
        if (!lasers[i].active) {
            lasers[i].x = x;
            lasers[i].y = y;
            lasers[i].width = width;
            lasers[i].height = height;
            lasers[i].speed = speed;
            lasers[i].active = 1;
            lasers[i].isEnemy = isEnemy;
            lasers[i].dx = 0;
            lasers[i].dy = 0;
            break;
        }
    }
}

void ShootAimedLaser(int x, int y, int dx, int dy, int isEnemy)
{
    int i;
    int width, height, speed;
    
    /* Only allow player to shoot if cooldown is zero */
    if (!isEnemy && shootCooldown > 0) {
        return;
    }
    
    /* Set properties based on laser type */
    if (isEnemy) {
        width = LASER_WIDTH;
        height = LASER_HEIGHT;
        speed = ENEMY_LASER_SPEED;
    } else {
        width = LASER_WIDTH;
        height = LASER_HEIGHT;
        speed = PLAYER_LASER_SPEED;
        shootCooldown = 10;  /* Set cooldown for player shots */
    }
    
    /* Find an inactive laser slot */
    for (i = 0; i < MAX_LASERS; i++) {
        if (!lasers[i].active) {
            lasers[i].x = x;
            lasers[i].y = y;
            lasers[i].width = width;
            lasers[i].height = height;
            lasers[i].speed = speed;
            lasers[i].active = 1;
            lasers[i].isEnemy = isEnemy;
            
            /* Store direction in dx, dy */
            lasers[i].dx = dx;
            lasers[i].dy = dy;
            break;
        }
    }
}

void CreateExplosion(int x, int y, int type)
{
    int i;
    int maxRadius;
    
    /* Set explosion size based on type */
    if (type == 0) {
        /* Enemy explosion */
        maxRadius = 20;
    } else {
        /* Player explosion */
        maxRadius = 30;
    }
    
    /* Find an inactive explosion slot */
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) {
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].radius = 5;
            explosions[i].active = 1;
            explosions[i].maxRadius = maxRadius;
            explosions[i].type = type;
            break;
        }
    }
}

void InitStars(void)
{
    int i;
    
    /* Initialize star field for parallax scrolling background */
    for (i = 0; i < STAR_COUNT; i++) {
        stars[i].x = rand() % WINDOW_WIDTH;
        stars[i].y = rand() % WINDOW_HEIGHT;
        stars[i].brightness = rand() % 3;  /* 0 = dim, 1 = medium, 2 = bright */
    }
}

void DrawStars(HDC hdc)
{
    int i;
    
    /* Draw the star field */
    for (i = 0; i < STAR_COUNT; i++) {
        int brightness;
        HBRUSH starBrush;
        
        /* Determine star color based on brightness */
        if (stars[i].brightness == 0) {
            brightness = 100;  /* Dim */
        } else if (stars[i].brightness == 1) {
            brightness = 180;  /* Medium */
        } else {
            brightness = 255;  /* Bright */
        }
        
        starBrush = CreateSolidBrush(RGB(brightness, brightness, brightness));
        SelectObject(hdc, starBrush);
        
        /* Draw stars as small rectangles, size based on brightness */
        if (stars[i].brightness == 2) {
            /* Bright stars are larger */
            Rectangle(hdc, stars[i].x - 1, stars[i].y - 1, stars[i].x + 2, stars[i].y + 2);
        } else {
            /* Dim and medium stars are smaller */
            SetPixel(hdc, stars[i].x, stars[i].y, RGB(brightness, brightness, brightness));
        }
        
        DeleteObject(starBrush);
    }
}

void UpdateStars(void)
{
    int i;
    
    /* Move stars for parallax scrolling effect */
    for (i = 0; i < STAR_COUNT; i++) {
        /* Move stars at different speeds based on brightness for parallax effect */
        if (stars[i].brightness == 0) {
            stars[i].y += 1;  /* Slowest (distant stars) */
        } else if (stars[i].brightness == 1) {
            stars[i].y += 2;  /* Medium */
        } else {
            stars[i].y += 3;  /* Fastest (closest stars) */
        }
        
        /* Wrap stars around when they reach the bottom */
        if (stars[i].y >= WINDOW_HEIGHT) {
            stars[i].y = 0;
            stars[i].x = rand() % WINDOW_WIDTH;
        }
    }
}

void RestartGame(void)
{
    int i;
    
    /* Only restart if game is over */
    if (!gameOver) {
        return;
    }
    
    /* Reset game state */
    playerHealth = 3;
    score = 0;
    level = 1;
    gameOver = 0;
    shootCooldown = 0;
    enemySpawnTimer = 0;
    
    /* Reset player position */
    playerX = WINDOW_WIDTH / 2;
    playerY = WINDOW_HEIGHT - 50;
    
    /* Clear all enemies */
    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }
    
    /* Clear all lasers */
    for (i = 0; i < MAX_LASERS; i++) {
        lasers[i].active = 0;
    }
    
    /* Clear all explosions */
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        explosions[i].active = 0;
    }
}