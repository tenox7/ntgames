#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h> /* For timeBeginPeriod/timeEndPeriod */
#include "resource.h" /* Include resource IDs */

/* Define Windows System Colors if not defined */
#ifndef COLOR_SCROLLBAR
#define COLOR_SCROLLBAR         0
#define COLOR_BACKGROUND        1
#define COLOR_ACTIVECAPTION     2
#define COLOR_INACTIVECAPTION   3
#define COLOR_MENU              4
#define COLOR_WINDOW            5
#define COLOR_WINDOWFRAME       6
#define COLOR_MENUTEXT          7
#define COLOR_WINDOWTEXT        8
#define COLOR_CAPTIONTEXT       9
#define COLOR_ACTIVEBORDER      10
#define COLOR_INACTIVEBORDER    11
#define COLOR_APPWORKSPACE      12
#define COLOR_HIGHLIGHT         13
#define COLOR_HIGHLIGHTTEXT     14
#define COLOR_BTNFACE           15
#define COLOR_BTNSHADOW         16
#define COLOR_GRAYTEXT          17
#define COLOR_BTNTEXT           18
#define COLOR_INACTIVECAPTIONTEXT 19
#define COLOR_BTNHIGHLIGHT      20

#define WHITE_BRUSH             0
#define LTGRAY_BRUSH            1
#define GRAY_BRUSH              2
#define DKGRAY_BRUSH            3
#define BLACK_BRUSH             4
#define NULL_BRUSH              5
#define WHITE_PEN               6
#define BLACK_PEN               7
#define NULL_PEN                8
#define HOLLOW_BRUSH            NULL_BRUSH
#define OEM_FIXED_FONT          10
#define ANSI_FIXED_FONT         11
#define ANSI_VAR_FONT           12
#define SYSTEM_FONT             13
#define DEVICE_DEFAULT_FONT     14
#define DEFAULT_PALETTE         15
#define SYSTEM_FIXED_FONT       16
#define DEFAULT_GUI_FONT        17
#define DC_BRUSH                18
#define DC_PEN                  19
#define STOCK_LAST              19

#endif

#pragma comment(lib, "winmm.lib") /* Link with winmm.lib for timer functions */

/* Global variables for CPU info */
char cpuArchStr[64];
char procTypeStr[64];

/* DC_PEN and DC_BRUSH already defined above */

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAX_ENEMIES 20
#define MAX_LASERS 30
#define MAX_EXPLOSIONS 20
#define PLAYER_SIZE 32
#define ENEMY_SIZE 32
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
int playerHealth = 5;
int score = 0;
int level = 1;
int gameOver = 0;
int gameStarted = 0;
int shootCooldown = 0;
int enemySpawnTimer = 0;
int frameCount = 0;
int gameOverTimer = 0;
int playerDeathSequence = 0;
int playerDeathTimer = 0;
int playerDeathExplosionCount = 0;
int enemiesKilled = 0;

/* GDI objects */
HINSTANCE hInst;
HWND hwnd;
HDC hdcBuffer;
HBITMAP hbmBuffer;
HBITMAP hbmOldBuffer;

/* Sprite bitmaps */
HBITMAP hbmShip;
HBITMAP hbmEnemy1;
HBITMAP hbmEnemy2;
HBITMAP hbmEnemy3;

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
void DrawSprite(HDC hdc, HBITMAP hbm, int x, int y, int width, int height);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc;
    MSG msg;
    RECT rcClient;
    int windowWidth, windowHeight;
    int i;
    HANDLE hProcess;

    /* Set process to highest priority for maximum performance */
    hProcess = GetCurrentProcess();
    SetPriorityClass(hProcess, REALTIME_PRIORITY_CLASS);
    
    /* Set this thread to highest priority as well */
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

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

    /* Get CPU information */
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        
        /* Determine CPU architecture */
        switch(sysInfo.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_INTEL:
                lstrcpy(cpuArchStr, "x86");
                break;
            case PROCESSOR_ARCHITECTURE_MIPS:
                lstrcpy(cpuArchStr, "MIPS");
                break;
            case PROCESSOR_ARCHITECTURE_ALPHA:
                lstrcpy(cpuArchStr, "Alpha");
                break;
            case PROCESSOR_ARCHITECTURE_PPC:
                lstrcpy(cpuArchStr, "PowerPC");
                break;
            default:
                lstrcpy(cpuArchStr, "Unknown");
                break;
        }
        
        /* Determine processor type */
        switch(sysInfo.dwProcessorType) {
            case PROCESSOR_INTEL_386:
                lstrcpy(procTypeStr, "Intel 386");
                break;
            case PROCESSOR_INTEL_486:
                lstrcpy(procTypeStr, "Intel 486");
                break;
            case PROCESSOR_INTEL_PENTIUM:
                lstrcpy(procTypeStr, "Intel Pentium");
                break;
            case PROCESSOR_MIPS_R4000:
                lstrcpy(procTypeStr, "MIPS R4000");
                break;
            case PROCESSOR_ALPHA_21064:
                lstrcpy(procTypeStr, "Alpha 21064");
                break;
            default:
                wsprintf(procTypeStr, "Type %d", sysInfo.dwProcessorType);
                break;
        }
    }

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
    
    /* Load sprite bitmaps from resources */
    hbmShip = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SHIP));
    hbmEnemy1 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_ENEMY1));
    hbmEnemy2 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_ENEMY2));
    hbmEnemy3 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_ENEMY3));

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
            
            /* Clean up bitmap resources */
            DeleteObject(hbmShip);
            DeleteObject(hbmEnemy1);
            DeleteObject(hbmEnemy2);
            DeleteObject(hbmEnemy3);
            
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

    /* Update window title */
    if (gameStarted && gameOver) {
        wsprintf(windowTitle, "SYSTEM ERROR - Memory_Management_Exception 0x0000000A");
    } else {
        wsprintf(windowTitle, "NT Rigue [%s %s] - ESC/Q to Exit", 
                cpuArchStr, procTypeStr);
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

    /* Draw UI elements at the top of the screen */
    {
        int i;
        int boxSize = 20;   /* Size of each health box */
        int boxPadding = 5; /* Space between boxes */
        int startX = 20;    /* Starting X position */
        int startY = 20;    /* Y position from top */
        int maxDisplayHealth = 5; /* Show 5 boxes for health */
        int textY = 20;     /* Y position for text */
        HBRUSH healthBrush, healthOutlineBrush;
        HPEN healthOutline;
        HFONT uiFont, oldFont;
        char statusText[80];
        
        /* Create font for status display */
        uiFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "System");
        oldFont = SelectObject(hdc, uiFont);
        
        /* Set text color for status display */
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        
        /* Create brushes for health boxes */
        healthBrush = CreateSolidBrush(RGB(0, 255, 0));        /* Bright green for active health from 16-color system palette */
        healthOutlineBrush = CreateSolidBrush(RGB(128, 128, 128)); /* Gray for empty health from 16-color system palette */
        healthOutline = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
        
        /* Draw health boxes */
        for (i = 0; i < maxDisplayHealth; i++) {
            /* Select appropriate brush based on current health */
            if (i < playerHealth) {
                SelectObject(hdc, healthBrush);
            } else {
                SelectObject(hdc, healthOutlineBrush);
            }
            
            /* Draw the health box */
            SelectObject(hdc, healthOutline);
            Rectangle(hdc, 
                     startX + (i * (boxSize + boxPadding)), 
                     startY, 
                     startX + (i * (boxSize + boxPadding)) + boxSize, 
                     startY + boxSize);
        }
        
        /* Draw level and enemies killed on screen */
        if (!gameOver && !playerDeathSequence) {
            /* Display LEVEL in center of screen */
            wsprintf(statusText, "LEVEL: %d", level);
            TextOut(hdc, WINDOW_WIDTH / 2 - 40, textY, statusText, lstrlen(statusText));
            
            /* Display KILLS on right side of screen */
            wsprintf(statusText, "KILLS: %d", enemiesKilled);
            TextOut(hdc, WINDOW_WIDTH - 120, textY, statusText, lstrlen(statusText));
            
            /* Display HEALTH indicator next to health boxes */
            TextOut(hdc, startX + (maxDisplayHealth * (boxSize + boxPadding)) + 10, 
                   textY, "HEALTH", 6);
                   
            /* Display SCORE at the top right */
            wsprintf(statusText, "SCORE: %d", score);
            TextOut(hdc, WINDOW_WIDTH - 120, textY + 25, statusText, lstrlen(statusText));
            
            /* Display CPU info at the bottom left */
            wsprintf(statusText, "Iron: %s %s", cpuArchStr, procTypeStr);
            TextOut(hdc, 20, WINDOW_HEIGHT - 25, statusText, lstrlen(statusText));
        }
        
        /* Clean up */
        SelectObject(hdc, oldFont);
        DeleteObject(uiFont);
        DeleteObject(healthBrush);
        DeleteObject(healthOutlineBrush);
        DeleteObject(healthOutline);
    }

    /* Draw player ship if not in death sequence or game over */
    if (!gameOver && !playerDeathSequence) {
        /* Draw player ship using sprite */
        DrawSprite(hdc, hbmShip, playerX - PLAYER_SIZE/2, playerY - PLAYER_SIZE/2, PLAYER_SIZE, PLAYER_SIZE);
    }

    /* Draw enemies */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            /* Choose enemy bitmap based on type */
            HBITMAP enemyBitmap;
            
            switch (enemies[i].type) {
                case 0:
                    enemyBitmap = hbmEnemy1;
                    break;
                case 1:
                    enemyBitmap = hbmEnemy2;
                    break;
                case 2:
                    enemyBitmap = hbmEnemy3;
                    break;
                default:
                    enemyBitmap = hbmEnemy1;
                    break;
            }
            
            /* Draw enemy sprite */
            DrawSprite(hdc, enemyBitmap, 
                      enemies[i].x - enemies[i].width/2, 
                      enemies[i].y - enemies[i].height/2, 
                      enemies[i].width, enemies[i].height);
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
                /* Enemy explosion - yellow from 16-color system palette */
                explosionBrush = CreateSolidBrush(RGB(255, 255, 0));
            } else {
                /* Player explosion - red from 16-color system palette */
                explosionBrush = CreateSolidBrush(RGB(255, 0, 0));
            }
            
            SelectObject(hdc, explosionBrush);
            Ellipse(hdc, explosions[i].x - explosions[i].radius, explosions[i].y - explosions[i].radius,
                   explosions[i].x + explosions[i].radius, explosions[i].y + explosions[i].radius);
            DeleteObject(explosionBrush);
        }
    }
    
    /* Display Game Over message as a Windows NT BSOD (only after death sequence is complete) */
    if (gameStarted && gameOver && !playerDeathSequence) {
        RECT rect;
        HFONT consoleFont, oldFont;
        char bsodText[30][128]; /* Array to hold multiple lines of BSOD text */
        int i, yPos;
        
        /* Create full blue background for BSOD */
        rect.left = 0;
        rect.top = 0;
        rect.right = WINDOW_WIDTH;
        rect.bottom = WINDOW_HEIGHT;
        
        /* Fill with classic BSOD blue color from Windows System Palette */
        FillRect(hdc, &rect, CreateSolidBrush(RGB(0, 0, 128)));
        
        /* Create console-like font for BSOD text */
        consoleFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, "Courier New");
        oldFont = SelectObject(hdc, consoleFont);
        
        /* White text on blue background */
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        
        /* Prepare BSOD text content */
        lstrcpy(bsodText[0], "");
        lstrcpy(bsodText[1], "*** STOP: 0x0000000A (0xDEADC0DE,0x00000000,0x00000000,0x00000000)");
        lstrcpy(bsodText[2], "GAME OVER - IRQL_NOT_LESS_OR_EQUAL");
        lstrcpy(bsodText[3], "");
        lstrcpy(bsodText[4], "The system detected an invalid or unaligned access to a memory location.");
        lstrcpy(bsodText[5], "The request could not be performed because of an I/O error.");
        
        /* Format score into technical-looking hex values */
        wsprintf(bsodText[6], "Player error at address 0x%08X, score=0x%08X, level=%d", playerX + playerY, score, level);
        
        lstrcpy(bsodText[7], "");
        lstrcpy(bsodText[8], "If this is the first time you've seen this error screen, restart your game.");
        lstrcpy(bsodText[9], "If this screen appears again, follow these steps:");
        lstrcpy(bsodText[10], "");
        lstrcpy(bsodText[11], "Check for viruses on your computer. Remove any newly installed alien hardware.");
        lstrcpy(bsodText[12], "Check that your weapons systems are properly configured and terminated.");
        lstrcpy(bsodText[13], "Run CHKDSK /F to check for hard drive corruption, then restart your game.");
        lstrcpy(bsodText[14], "");
        lstrcpy(bsodText[15], "Technical information:");
        lstrcpy(bsodText[16], "");
        
        /* Generate random hex dumps that look like memory addresses */
        wsprintf(bsodText[17], "*** BYTES_PROCESSED: 0x%08X  ENEMY_COUNT: 0x%04X  SECTOR: 0x%04X", frameCount * 1024, MAX_ENEMIES, rand() % 0xFFFF);
        wsprintf(bsodText[18], "*** MEMORY_MANAGEMENT: 0x%08X  STACK_OVERFLOW: 0x%08X", 0xBAADF00D, 0xDEADBEEF);
        wsprintf(bsodText[19], "*** PROCESS_NAME: NTRIGUE.EXE  PID: 0x%04X  START_ADDR: 0x%08X", GetCurrentProcessId(), 0x01001000);
        
        {
            char hexDigits[] = "0123456789ABCDEF";
            int j;
            int offset;
            char *dumpLine;
            
            for (i = 20; i < 25; i++) {
                /* Create random looking memory dump lines */
                dumpLine = bsodText[i];
                offset = 0;
                
                /* Memory address prefix */
                offset += wsprintf(dumpLine + offset, "%08X  ", 0x80000000 + (i * 16));
                
                /* Generate hex values */
                for (j = 0; j < 16; j++) {
                    if (j == 8) /* Extra space in middle */
                        dumpLine[offset++] = ' ';
                    
                    dumpLine[offset++] = hexDigits[rand() % 16];
                    dumpLine[offset++] = hexDigits[rand() % 16];
                    dumpLine[offset++] = ' ';
                }
                
                dumpLine[offset] = '\0';
            }
        }
        
        lstrcpy(bsodText[25], "");
        if (gameOverTimer > 0) {
            int secsLeft = (gameOverTimer + 59) / 60; /* Round up to the next second */
            wsprintf(bsodText[26], "* System will restart in %d seconds...", secsLeft);
        } else {
            lstrcpy(bsodText[26], "* Press any key or click to restart");
        }
        lstrcpy(bsodText[27], "* Press ESC or Q to quit");
        
        /* Display all the text lines */
        yPos = 30;
        for (i = 0; i < 28; i++) {
            TextOut(hdc, 30, yPos, bsodText[i], lstrlen(bsodText[i]));
            yPos += 20;
        }
        
        /* Clean up */
        SelectObject(hdc, oldFont);
        DeleteObject(consoleFont);
    }
}

void UpdateGame(void)
{
    int i, j;
    
    frameCount++;
    
    /* Handle player death sequence */
    if (playerDeathSequence) {
        playerDeathTimer--;
        
        /* Generate additional explosions during death sequence */
        if (frameCount % 8 == 0 && playerDeathExplosionCount < 15) {
            /* Create random explosions around the player position */
            int offsetX = (rand() % 80) - 40;
            int offsetY = (rand() % 80) - 40;
            
            /* Alternate between enemy (yellow) and player (red) explosions for visual variety */
            int explosionType = (playerDeathExplosionCount % 2);
            CreateExplosion(playerX + offsetX, playerY + offsetY, explosionType);
            playerDeathExplosionCount++;
            
            /* Create a second explosion for more dramatic effect */
            if (playerDeathExplosionCount < 12) {
                offsetX = (rand() % 60) - 30;
                offsetY = (rand() % 60) - 30;
                explosionType = ((playerDeathExplosionCount + 1) % 2);
                CreateExplosion(playerX + offsetX, playerY + offsetY, explosionType);
            }
        }
        
        /* When death sequence is complete, transition to game over */
        if (playerDeathTimer <= 0) {
            playerDeathSequence = 0;
            gameOver = 1;
            gameOverTimer = 300; /* 5 seconds at 60 FPS */
        }
        
        /* Continue updating explosions during death sequence */
        for (i = 0; i < MAX_EXPLOSIONS; i++) {
            if (explosions[i].active) {
                explosions[i].radius += 1;
                if (explosions[i].radius >= explosions[i].maxRadius) {
                    explosions[i].active = 0;
                }
            }
        }
        
        return; /* Only update explosions during death sequence */
    }
    
    if (gameOver) {
        /* If game is over, just update the game over timer */
        if (gameOverTimer > 0) {
            gameOverTimer--;
        }
        return;  /* Stop updating game mechanics if game is over */
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
                            /* Start death sequence instead of immediately showing game over */
                            playerDeathSequence = 1;
                            playerDeathTimer = 120; /* 2 seconds of death sequence at 60 FPS */
                            playerDeathExplosionCount = 0;
                            /* Create initial explosion */
                            CreateExplosion(playerX, playerY, 1);
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
                                enemiesKilled++;
                                
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
                        /* Start death sequence instead of immediately showing game over */
                        playerDeathSequence = 1;
                        playerDeathTimer = 120; /* 2 seconds of death sequence at 60 FPS */
                        playerDeathExplosionCount = 0;
                        /* Create initial explosion */
                        CreateExplosion(playerX, playerY, 1);
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
                /* Type 1 (double shooter): shoots two bullets side by side */
                else if (enemies[i].type == 1 && rand() % 100 < 15) {
                    shouldShoot = 1;
                    enemies[i].shootTimer = 75;  /* Double shooter: 1.25 seconds between shots */
                }
                /* Type 3 (rapid): shoots more frequently and faster bullets */
                else if (enemies[i].type == 2 && rand() % 100 < 25) {
                    shouldShoot = 1;
                    enemies[i].shootTimer = 30;  /* Rapid: 0.5 seconds between shots */
                }
                
                if (shouldShoot && !gameOver) {
                    /* All enemies shoot straight down but with different patterns */
                    if (enemies[i].type == 0) {
                        /* Basic enemy (type 0) - one regular bullet */
                        ShootLaser(enemies[i].x, enemies[i].y + enemies[i].height/2, 1);
                    } else if (enemies[i].type == 1) {
                        /* Enemy type 1 - shoots 2 bullets side by side */
                        ShootLaser(enemies[i].x - 8, enemies[i].y + enemies[i].height/2, 1);
                        ShootLaser(enemies[i].x + 8, enemies[i].y + enemies[i].height/2, 1);
                    } else if (enemies[i].type == 2) {
                        /* Enemy type 2 - shoots faster bullets */
                        ShootLaser(enemies[i].x, enemies[i].y + enemies[i].height/2, 2);
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
        /* Randomly choose enemy type with all three types available from the start */
        int type;
        int r = rand() % 100;
        
        if (r < 40) {
            /* 40% chance for type 0 enemy (uses enemy1.bmp) */
            type = 0;
            enemySpawnTimer = 45;
        } else if (r < 70) {
            /* 30% chance for type 1 enemy (uses enemy2.bmp) */
            type = 1;
            enemySpawnTimer = 60;
        } else {
            /* 30% chance for type 2 enemy (uses enemy3.bmp) */
            type = 2;
            enemySpawnTimer = 75;
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
    
    /* Set properties based on enemy type - all same size but different attributes */
    width = height = ENEMY_SIZE;
    
    if (type == 0) {
        /* Basic enemy (enemy1.bmp) */
        health = 1;
        scoreValue = 10;
    } else if (type == 1) {
        /* Shooting enemy (enemy2.bmp) */  
        health = 2;
        scoreValue = 20;
    } else if (type == 2) {
        /* Advanced enemy (enemy3.bmp) */
        health = 3;
        scoreValue = 30;
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
    if (isEnemy == 1) {
        /* Regular enemy laser */
        width = LASER_WIDTH;
        height = LASER_HEIGHT;
        speed = ENEMY_LASER_SPEED;
    } else if (isEnemy == 2) {
        /* Fast enemy laser (for enemy type 2) */
        width = LASER_WIDTH;
        height = LASER_HEIGHT;
        speed = ENEMY_LASER_SPEED * 2; /* Twice as fast */
    } else {
        /* Player laser */
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
    if (isEnemy == 1) {
        /* Regular enemy laser */
        width = LASER_WIDTH;
        height = LASER_HEIGHT;
        speed = ENEMY_LASER_SPEED;
    } else if (isEnemy == 2) {
        /* Fast enemy laser (for enemy type 2) */
        width = LASER_WIDTH;
        height = LASER_HEIGHT;
        speed = ENEMY_LASER_SPEED * 2; /* Twice as fast */
    } else {
        /* Player laser */
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
        /* Enemy explosion (yellow) */
        maxRadius = 20;
    } else {
        /* Player explosion (red) */
        maxRadius = 30;
    }
    
    /* For death sequence, create additional variation in explosion sizes */
    if (playerDeathSequence) {
        /* Add variation to explosion sizes during death sequence */
        maxRadius += rand() % 15;
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
    
    /* Only restart if game is over and timer has expired */
    if (!gameOver) {
        return;
    }
    
    /* Don't allow restart until the timer has reached 0 */
    if (gameOverTimer > 0) {
        return;
    }
    
    /* Reset game state */
    playerHealth = 5;
    score = 0;
    level = 1;
    gameOver = 0;
    shootCooldown = 0;
    enemySpawnTimer = 0;
    playerDeathSequence = 0;
    playerDeathTimer = 0;
    playerDeathExplosionCount = 0;
    enemiesKilled = 0;
    
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

/* Function removed since we now load bitmaps from resources */

void DrawSprite(HDC hdc, HBITMAP hbm, int x, int y, int width, int height)
{
    HDC hdcMem;
    BITMAP bm;
    COLORREF crTransparent = RGB(0, 0, 0); /* Black is transparent */
    
    /* Create a memory DC */
    hdcMem = CreateCompatibleDC(hdc);
    
    /* Select the bitmap into the DC */
    SelectObject(hdcMem, hbm);
    
    /* Get the bitmap dimensions */
    GetObject(hbm, sizeof(BITMAP), &bm);
    
    /* Draw the bitmap with transparency - using TransparentBlt equivalent */
    /* Since we're on older Windows, manually implement transparency */
    {
        HDC hdcMask, hdcColor;
        HBITMAP hbmMask, hbmColor, hbmOldMask, hbmOldColor;
        
        /* Create DCs for the mask and colored image */
        hdcMask = CreateCompatibleDC(hdc);
        hdcColor = CreateCompatibleDC(hdc);
        
        /* Create monochrome and color bitmaps */
        hbmMask = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, NULL);
        hbmColor = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
        
        /* Select the bitmaps into the DCs */
        hbmOldMask = SelectObject(hdcMask, hbmMask);
        hbmOldColor = SelectObject(hdcColor, hbmColor);
        
        /* Set the background color for the source image */
        SetBkColor(hdcMem, crTransparent);
        
        /* Create the mask - white is transparent area, black is the shape */
        BitBlt(hdcMask, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);
        
        /* Create the inverse mask for the colored portion */
        BitBlt(hdcColor, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);
        
        /* Combine the mask and the destination */
        StretchBlt(hdc, x, y, width, height, hdcMask, 0, 0, bm.bmWidth, bm.bmHeight, SRCAND);
        
        /* Combine the color and the destination */
        StretchBlt(hdc, x, y, width, height, hdcColor, 0, 0, bm.bmWidth, bm.bmHeight, SRCPAINT);
        
        /* Clean up */
        SelectObject(hdcMask, hbmOldMask);
        SelectObject(hdcColor, hbmOldColor);
        DeleteObject(hbmMask);
        DeleteObject(hbmColor);
        DeleteDC(hdcMask);
        DeleteDC(hdcColor);
    }
    
    /* Clean up */
    DeleteDC(hdcMem);
}