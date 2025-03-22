#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAX_EXPLOSIONS 20
#define NUM_CITIES 6
#define CURSOR_SIZE 10
#define MAX_MISSILES 20
#define MIN(a,b) (((a)<(b))?(a):(b))

typedef struct {
    int startX, startY;
    int x, y;
    int dx, dy;
    int active;
} Missile;

typedef struct {
    int x, y;
    int radius;
    int active;
    int maxRadius;
    int type;  /* 0 = normal, 1 = city destruction */
} Explosion;

typedef struct {
    int x, y;
    int alive;
} City;

Missile missiles[MAX_MISSILES];
Explosion explosions[MAX_EXPLOSIONS];
City cities[NUM_CITIES];
int cursorX, cursorY;
int score = 0;
int level = 1;
int gameOver = 0;
int gameStarted = 0; /* Flag to indicate game has properly started */

HINSTANCE hInst;
HWND hwnd;
HDC hdcBuffer;
HBITMAP hbmBuffer;
HBITMAP hbmOldBuffer;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void GameLoop(HDC hdc);
void DrawScene(HDC hdc);
void UpdateGame(void);
void LaunchMissile(int x, int y);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc;
    MSG msg;
    int i;

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "MissileCommandClass";

    if (!RegisterClass(&wc)) return 1;

    hwnd = CreateWindow("MissileCommandClass", "Missile Command",
                        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                        WINDOW_WIDTH, WINDOW_HEIGHT+25, NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    srand((unsigned)time(NULL));

    /* Initialize game state */
    gameOver = 0;  /* Explicitly ensure game is not in game over state */
    gameStarted = 0; /* Game hasn't properly started yet */
    
    /* Initialize missiles */
    for (i = 0; i < MAX_MISSILES; i++) {
        missiles[i].active = 0;
    }
    
    /* Initialize explosions */
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        explosions[i].active = 0;
    }
    
    /* Initialize cities */
    for (i = 0; i < NUM_CITIES; i++) {
        cities[i].x = (i + 1) * WINDOW_WIDTH / (NUM_CITIES + 1);
        cities[i].y = WINDOW_HEIGHT - 20;
        cities[i].alive = 1;
    }

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

    switch (msg) {
        case WM_CREATE:
            hdc = GetDC(hwnd);
            hdcBuffer = CreateCompatibleDC(hdc);
            hbmBuffer = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
            hbmOldBuffer = SelectObject(hdcBuffer, hbmBuffer);
            ReleaseDC(hwnd, hdc);
            SetTimer(hwnd, 1, 16, NULL);  /* ~60 FPS */
            return 0;

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            GameLoop(hdcBuffer);
            BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcBuffer, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;

        case WM_TIMER:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_MOUSEMOVE:
            cursorX = LOWORD(lParam);
            cursorY = HIWORD(lParam);
            return 0;

        case WM_LBUTTONDOWN:
            LaunchMissile(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            SelectObject(hdcBuffer, hbmOldBuffer);
            DeleteObject(hbmBuffer);
            DeleteDC(hdcBuffer);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void GameLoop(HDC hdc)
{
    UpdateGame();
    DrawScene(hdc);
}

void DrawScene(HDC hdc)
{
    int i;
    RECT rect;
    HBRUSH cityBrush, explosionBrush, cursorBrush;
    HPEN missilePen;
    char windowTitle[50];

    /* Update window title with score and level */
    if (gameStarted && gameOver) {
        wsprintf(windowTitle, "Missile Command - GAME OVER - Final Score: %d", score);
    } else {
        wsprintf(windowTitle, "Missile Command - Score: %d  Level: %d", score, level);
    }
    SetWindowText(hwnd, windowTitle);

    /* Clear the buffer */
    rect.left = 0;
    rect.top = 0;
    rect.right = WINDOW_WIDTH;
    rect.bottom = WINDOW_HEIGHT;
    FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    /* Draw cities (trapezoid shapes with more detail) */
    for (i = 0; i < NUM_CITIES; i++) {
        if (cities[i].alive) {
            /* Create trapezoid shape */
            POINT cityPoints[4];
            HPEN cityOutline;
            HBRUSH detailBrush;
            
            /* Main trapezoid body */
            cityPoints[0].x = cities[i].x - 25;  /* Bottom left */
            cityPoints[0].y = cities[i].y + 20;
            cityPoints[1].x = cities[i].x - 15;  /* Top left */
            cityPoints[1].y = cities[i].y - 15;
            cityPoints[2].x = cities[i].x + 15;  /* Top right */
            cityPoints[2].y = cities[i].y - 15;
            cityPoints[3].x = cities[i].x + 25;  /* Bottom right */
            cityPoints[3].y = cities[i].y + 20;
            
            /* Fill the trapezoid */
            cityBrush = CreateSolidBrush(RGB(0, 200, 0));
            SelectObject(hdc, cityBrush);
            Polygon(hdc, cityPoints, 4);
            
            /* Add details - windows/doors */
            detailBrush = CreateSolidBrush(RGB(0, 100, 0));
            SelectObject(hdc, detailBrush);
            
            /* Draw a small rectangle for a door */
            rect.left = cities[i].x - 5;
            rect.top = cities[i].y + 5;
            rect.right = cities[i].x + 5;
            rect.bottom = cities[i].y + 20;
            FillRect(hdc, &rect, detailBrush);
            
            /* Add windows */
            rect.left = cities[i].x - 12;
            rect.top = cities[i].y - 5;
            rect.right = cities[i].x - 5;
            rect.bottom = cities[i].y + 2;
            FillRect(hdc, &rect, detailBrush);
            
            rect.left = cities[i].x + 5;
            rect.top = cities[i].y - 5;
            rect.right = cities[i].x + 12;
            rect.bottom = cities[i].y + 2;
            FillRect(hdc, &rect, detailBrush);
            
            /* Add outline */
            cityOutline = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
            SelectObject(hdc, cityOutline);
            Polygon(hdc, cityPoints, 4);
            
            /* Cleanup */
            DeleteObject(detailBrush);
            DeleteObject(cityOutline);
            DeleteObject(cityBrush);
        }
    }

    /* Draw missiles */
    for (i = 0; i < MAX_MISSILES; i++) {
        if (missiles[i].active) {
            missilePen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            SelectObject(hdc, missilePen);
            MoveToEx(hdc, missiles[i].startX, missiles[i].startY, NULL);
            LineTo(hdc, missiles[i].x, missiles[i].y);
            DeleteObject(missilePen);
        }
    }

    /* Draw explosions */
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].active) {
            if (explosions[i].type == 1) {
                /* City destruction explosion - red */
                explosionBrush = CreateSolidBrush(RGB(255, 50, 50));
            } else {
                /* Normal explosion - yellow */
                explosionBrush = CreateSolidBrush(RGB(255, 255, 0));
            }
            
            SelectObject(hdc, explosionBrush);
            Ellipse(hdc, explosions[i].x - explosions[i].radius, explosions[i].y - explosions[i].radius,
                    explosions[i].x + explosions[i].radius, explosions[i].y + explosions[i].radius);
            DeleteObject(explosionBrush);
        }
    }

    /* Draw cursor */
    cursorBrush = CreateSolidBrush(RGB(0, 255, 255));
    SelectObject(hdc, cursorBrush);
    Ellipse(hdc, cursorX - CURSOR_SIZE/2, cursorY - CURSOR_SIZE/2,
            cursorX + CURSOR_SIZE/2, cursorY + CURSOR_SIZE/2);
    DeleteObject(cursorBrush);
    
    /* Display Game Over message only if game was started and is now over */
    if (gameStarted && gameOver) {
        char gameOverText[20] = "GAME OVER";
        char finalScoreText[30];
        HFONT hFont, hOldFont;
        SIZE textSize;
        int textWidth, textX;
        
        /* Create a larger font for Game Over message */
        hFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                           ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
        hOldFont = SelectObject(hdc, hFont);
        
        /* Display Game Over text */
        SetTextColor(hdc, RGB(255, 50, 50));
        SetBkMode(hdc, TRANSPARENT);
        
        /* Center the text - get width first */
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
        
        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
    }
}

void UpdateGame(void)
{
    int i, j;
    int dx, dy;
    int targetX;
    int speed;
    int maxMissiles;
    int spawnChance;
    int cityCount = 0;
    static int frameCount = 0;

    /* Only check for game over condition if game is already running */
    if (!gameOver) {
        /* Check if all cities are destroyed */
        for (i = 0; i < NUM_CITIES; i++) {
            if (cities[i].alive) {
                cityCount++;
            }
        }
        
        if (cityCount == 0) {
            gameOver = 1;
        }
    }

    if (gameOver) {
        return;  /* Stop updating if game is over */
    }

    /* Set gameStarted flag once we get to the first frame update */
    if (!gameStarted) {
        gameStarted = 1;
    }
    
    frameCount++;

    /* Calculate speed based on level */
    speed = 200 + (level * 50);  /* Start at 200, increase by 50 each level */

    /* Calculate maximum number of missiles based on level */
    maxMissiles = MIN(MAX_MISSILES, 5 + level);  /* Start with 5, increase by 1 each level, cap at MAX_MISSILES */

    /* Calculate spawn chance */
    spawnChance = 120 - MIN(60, level * 5);
    if (spawnChance < 10) spawnChance = 10;  /* Ensure spawn chance doesn't go below 10 */

    /* Update missiles */
    for (i = 0; i < maxMissiles; i++) {
        if (missiles[i].active) {
            missiles[i].x += missiles[i].dx;
            missiles[i].y += missiles[i].dy;
            if (missiles[i].y >= WINDOW_HEIGHT) {
                /* Missile hit ground, create explosion */
                for (j = 0; j < MAX_EXPLOSIONS; j++) {
                    if (!explosions[j].active) {
                        explosions[j].x = missiles[i].x;
                        explosions[j].y = WINDOW_HEIGHT;
                        explosions[j].radius = 0;
                        explosions[j].active = 1;
                        explosions[j].maxRadius = 30;
                        explosions[j].type = 0;  /* Normal explosion */
                        break;
                    }
                }
                missiles[i].active = 0;
            }
        }
    }

    /* Update explosions */
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].active) {
            explosions[i].radius += 1;
            if (explosions[i].radius >= explosions[i].maxRadius) {
                explosions[i].active = 0;
            }

            /* Check if explosion hits any missiles */
            for (j = 0; j < maxMissiles; j++) {
                if (missiles[j].active) {
                    dx = missiles[j].x - explosions[i].x;
                    dy = missiles[j].y - explosions[i].y;
                    if (dx*dx + dy*dy <= explosions[i].radius * explosions[i].radius) {
                        missiles[j].active = 0;
                        score += 10;
                    }
                }
            }
        }
    }

    /* Spawn new missiles */
    if (frameCount < 60 || rand() % spawnChance == 0) {  /* Guarantee spawn in first second */
        for (i = 0; i < maxMissiles; i++) {
            if (!missiles[i].active) {
                int j;
                int targetCity;
                int cityCount = 0;
                int livingCities[NUM_CITIES];
                int cityIndex = 0;
                
                missiles[i].startX = missiles[i].x = rand() % WINDOW_WIDTH;
                missiles[i].startY = missiles[i].y = 0;
                
                /* Make sure we're targeting a live city, if any exist */
                for (j = 0; j < NUM_CITIES; j++) {
                    if (cities[j].alive) {
                        cityCount++;
                    }
                }
                
                if (cityCount > 0) {
                    /* Choose a random living city to target */
                    for (j = 0; j < NUM_CITIES; j++) {
                        if (cities[j].alive) {
                            livingCities[cityIndex++] = j;
                        }
                    }
                    
                    targetCity = livingCities[rand() % cityCount];
                    targetX = cities[targetCity].x;
                } else {
                    /* If no cities alive, just choose random target */
                    targetX = rand() % WINDOW_WIDTH;
                }
                
                missiles[i].dx = ((targetX - missiles[i].x) * speed) / (WINDOW_HEIGHT * 100);
                missiles[i].dy = speed / 100;
                missiles[i].active = 1;
                break;
            }
        }
    }

    /* Check if missiles hit cities */
    for (i = 0; i < maxMissiles; i++) {
        if (missiles[i].active && missiles[i].y >= WINDOW_HEIGHT - 20) {
            for (j = 0; j < NUM_CITIES; j++) {
                if (cities[j].alive && abs(missiles[i].x - cities[j].x) < 20) {
                    /* Create large explosion when city is destroyed */
                    int k;
                    for (k = 0; k < MAX_EXPLOSIONS; k++) {
                        if (!explosions[k].active) {
                            explosions[k].x = cities[j].x;
                            explosions[k].y = cities[j].y;
                            explosions[k].radius = 0;
                            explosions[k].active = 1;
                            explosions[k].maxRadius = 60;  /* Larger explosion for city destruction */
                            explosions[k].type = 1;  /* Mark as city destruction explosion */
                            break;
                        }
                    }
                    
                    cities[j].alive = 0;
                    missiles[i].active = 0;
                    break;
                }
            }
        }
    }

    /* Check for level up */
    if (score >= level * 100) {
        level++;
        frameCount = 0;  /* Reset frame count for quick spawn on new level */
    }
}

void LaunchMissile(int x, int y)
{
    int i;
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) {
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].radius = 0;
            explosions[i].active = 1;
            explosions[i].maxRadius = 30;
            explosions[i].type = 0;  /* Normal explosion */
            break;
        }
    }
}
