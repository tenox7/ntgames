#include <windows.h>
#include <stdlib.h>
#include <time.h>

#define GRID_SIZE 20
#define CELL_SIZE 20
#define WINDOW_SIZE (GRID_SIZE * CELL_SIZE)
#define MAX_SNAKE_LENGTH 100

typedef struct {
    int x;
    int y;
} Point;

typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

Point snake[MAX_SNAKE_LENGTH];
int snakeLength;
Point food;
Direction dir;
BOOL gameOver;
HBITMAP hbmMem;
HDC hdcMem;
HBITMAP hbmOld;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void GameInit(void);
void GameUpdate(void);
void GameRender(HDC);
void RestartGame(void);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    static TCHAR szAppName[] = TEXT("SnakeGame");
    HWND hwnd;
    MSG msg;
    WNDCLASS wndclass;

    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szAppName;

    if (!RegisterClass(&wndclass)) {
        MessageBox(NULL, TEXT("This program requires Windows NT!"), szAppName, MB_ICONERROR);
        return 0;
    }

    hwnd = CreateWindow(szAppName, TEXT("Snake Game"),
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        WINDOW_SIZE, WINDOW_SIZE + 50,  // Extra height for restart button
                        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    GameInit();

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    HDC hdc;
    PAINTSTRUCT ps;
    RECT rect;

    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd, 1, 100, NULL);
        hdc = GetDC(hwnd);
        hdcMem = CreateCompatibleDC(hdc);
        hbmMem = CreateCompatibleBitmap(hdc, WINDOW_SIZE, WINDOW_SIZE + 30);
        hbmOld = SelectObject(hdcMem, hbmMem);
        ReleaseDC(hwnd, hdc);
        return 0;

    case WM_TIMER:
        GameUpdate();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        GameRender(hdcMem);
        BitBlt(hdc, 0, 0, WINDOW_SIZE, WINDOW_SIZE + 30, hdcMem, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_UP:    if (dir != DOWN)  dir = UP;    break;
        case VK_DOWN:  if (dir != UP)    dir = DOWN;  break;
        case VK_LEFT:  if (dir != RIGHT) dir = LEFT;  break;
        case VK_RIGHT: if (dir != LEFT)  dir = RIGHT; break;
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (gameOver) {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            GetClientRect(hwnd, &rect);
            rect.top = WINDOW_SIZE;
            if (PtInRect(&rect, pt)) {
                RestartGame();
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return 0;
        
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

void GameInit(void) {
    srand((unsigned int)time(NULL));
    RestartGame();
}

void RestartGame(void) {
    snakeLength = 1;
    snake[0].x = GRID_SIZE / 2;
    snake[0].y = GRID_SIZE / 2;
    food.x = 1 + rand() % (GRID_SIZE - 2);
    food.y = 1 + rand() % (GRID_SIZE - 2);
    dir = RIGHT;
    gameOver = FALSE;
}

void GameUpdate(void) {
    int newX, newY, i;

    if (gameOver) return;

    newX = snake[0].x;
    newY = snake[0].y;

    switch (dir) {
    case UP:    newY--; break;
    case DOWN:  newY++; break;
    case LEFT:  newX--; break;
    case RIGHT: newX++; break;
    }

    if (newX < 0 || newX >= GRID_SIZE || newY < 0 || newY >= GRID_SIZE) {
        gameOver = TRUE;
        return;
    }

    for (i = 1; i < snakeLength; i++) {
        if (newX == snake[i].x && newY == snake[i].y) {
            gameOver = TRUE;
            return;
        }
    }

    for (i = snakeLength - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }

    snake[0].x = newX;
    snake[0].y = newY;

    if (snake[0].x == food.x && snake[0].y == food.y) {
        snakeLength++;
        food.x = 1 + rand() % (GRID_SIZE - 2);
        food.y = 1 + rand() % (GRID_SIZE - 2);
    }
}

void GameRender(HDC hdc) {
    HBRUSH hSnakeBrush, hFoodBrush, hBackgroundBrush, hButtonBrush;
    RECT rect;
    int i;

    hSnakeBrush = CreateSolidBrush(RGB(0, 255, 0));
    hFoodBrush = CreateSolidBrush(RGB(255, 0, 0));
    hBackgroundBrush = CreateSolidBrush(RGB(255, 255, 255));
    hButtonBrush = CreateSolidBrush(RGB(200, 200, 200));

    SetRect(&rect, 0, 0, WINDOW_SIZE, WINDOW_SIZE + 30);
    FillRect(hdc, &rect, hBackgroundBrush);

    for (i = 0; i < snakeLength; i++) {
        SetRect(&rect, snake[i].x * CELL_SIZE, snake[i].y * CELL_SIZE,
                (snake[i].x + 1) * CELL_SIZE, (snake[i].y + 1) * CELL_SIZE);
        FillRect(hdc, &rect, hSnakeBrush);
    }

    SetRect(&rect, food.x * CELL_SIZE, food.y * CELL_SIZE,
            (food.x + 1) * CELL_SIZE, (food.y + 1) * CELL_SIZE);
    FillRect(hdc, &rect, hFoodBrush);

    if (gameOver) {
        SetTextColor(hdc, RGB(255, 0, 0));
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, WINDOW_SIZE / 2 - 40, WINDOW_SIZE / 2, TEXT("Game Over"), 9);

        SetRect(&rect, 0, WINDOW_SIZE, WINDOW_SIZE, WINDOW_SIZE + 30);
        FillRect(hdc, &rect, hButtonBrush);
        SetTextColor(hdc, RGB(0, 0, 0));
        TextOut(hdc, WINDOW_SIZE / 2 - 30, WINDOW_SIZE + 5, TEXT("Restart"), 7);
    }

    DeleteObject(hSnakeBrush);
    DeleteObject(hFoodBrush);
    DeleteObject(hBackgroundBrush);
    DeleteObject(hButtonBrush);
}
