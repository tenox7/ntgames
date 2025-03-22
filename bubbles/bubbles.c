#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define WINDOW_CLASS_NAME "BubbleBreaker3D"
#define WINDOW_TITLE "Bubble Breaker"

#define BOARD_SIZE 11
#define CELL_PADDING 2
#define CELL_SIZE 40
#define COLOR_COUNT 5
#define SCREEN_WIDTH (BOARD_SIZE * CELL_SIZE)
#define SCREEN_HEIGHT (BOARD_SIZE * CELL_SIZE)

/* Game state */
int board[BOARD_SIZE][BOARD_SIZE];
int score = 0;
BOOL gameOver = FALSE;

/* Animation state */
#define ANIM_FRAMES 2
#define ANIM_SPEED 10 /* milliseconds per frame */
BOOL popping = FALSE;
BOOL popVisited[BOARD_SIZE][BOARD_SIZE] = {FALSE};
int popFrame = 0;

/* Persistent offscreen buffer */
HDC offscreenDC = NULL;
HBITMAP offscreenBitmap = NULL;
HBITMAP oldOffscreenBitmap = NULL;
BOOL needFullRedraw = TRUE;

/* Colors */
COLORREF colors[] = {
    RGB(255, 0, 0),    /* Red */
    RGB(0, 128, 0),    /* Green */
    RGB(0, 0, 255),    /* Blue */
    RGB(255, 128, 0),  /* Orange */
    RGB(255, 0, 255)   /* Magenta */
};

/* Function prototypes */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitBoard(void);
void DrawBoard(HDC hdc);
void UpdateWindowTitle(HWND hwnd);
void HandleClick(int x, int y);
int FindCluster(int x, int y, int color, BOOL visited[BOARD_SIZE][BOARD_SIZE]);
void ApplyGravity(void);
void ShiftColumns(void);
BOOL IsGameOver(void);
void RestartGame(HWND hwnd);
void StartPopAnimation(void);
void Draw3DBubble(HDC hdc, int x, int y, COLORREF color, int size);
void InitOffscreenBuffer(HWND hwnd);
void CleanupOffscreenBuffer(void);

/* Main function */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc;
    HWND hwnd;
    MSG msg;

    wc.style = 0;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hwnd = CreateWindow(
        WINDOW_CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, SCREEN_WIDTH+16, SCREEN_HEIGHT+38,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    /* Initialize persistent offscreen buffer */
    InitOffscreenBuffer(hwnd);

    srand((unsigned int)time(NULL));
    InitBoard();
    UpdateWindowTitle(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

#define TIMER_ID 1

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawBoard(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_SIZE: {
            /* Recreate offscreen buffer when window size changes */
            InitOffscreenBuffer(hwnd);
            needFullRedraw = TRUE;
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (gameOver) {
                RestartGame(hwnd);
            } else if (!popping) { /* Only allow clicks when not animating */
                int x = LOWORD(lParam) / CELL_SIZE;
                int y = HIWORD(lParam) / CELL_SIZE;
                HandleClick(x, y);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_TIMER: {
            int x, y;
            RECT updateRect;
            
            if (wParam == TIMER_ID && popping) {
                popFrame++;
                if (popFrame >= ANIM_FRAMES) {
                    /* Animation finished - remove popping bubbles and apply gravity */
                    KillTimer(hwnd, TIMER_ID);
                    popping = FALSE;
                    
                    /* Actually remove the bubbles */
                    for (y = 0; y < BOARD_SIZE; y++) {
                        for (x = 0; x < BOARD_SIZE; x++) {
                            if (popVisited[y][x]) {
                                board[y][x] = -1;
                            }
                        }
                    }
                    
                    /* Apply gravity and shift columns */
                    ApplyGravity();
                    ShiftColumns();
                    
                    /* Check if game is over */
                    if (IsGameOver()) {
                        gameOver = TRUE;
                    }
                    
                    /* Mark for full redraw */
                    needFullRedraw = TRUE;
                    
                    /* Full redraw after animation is complete */
                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    /* During animation, only redraw the affected bubbles */
                    for (y = 0; y < BOARD_SIZE; y++) {
                        for (x = 0; x < BOARD_SIZE; x++) {
                            if (popVisited[y][x]) {
                                /* Calculate the rectangle for this bubble */
                                updateRect.left = x * CELL_SIZE;
                                updateRect.top = y * CELL_SIZE;
                                updateRect.right = (x + 1) * CELL_SIZE;
                                updateRect.bottom = (y + 1) * CELL_SIZE;
                                
                                /* Only invalidate the bubble area, don't erase background */
                                InvalidateRect(hwnd, &updateRect, FALSE);
                            }
                        }
                    }
                }
            }
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            CleanupOffscreenBuffer();
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void InitBoard(void) {
    int x, y;
    for (y = 0; y < BOARD_SIZE; y++) {
        for (x = 0; x < BOARD_SIZE; x++) {
            board[y][x] = rand() % COLOR_COUNT;
        }
    }
}

#define CELL_PADDING 2

/* 3D helper function to generate lighter and darker shades */
COLORREF LightenColor(COLORREF color, float factor) {
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    
    r = min(255, (int)(r + (255 - r) * factor));
    g = min(255, (int)(g + (255 - g) * factor));
    b = min(255, (int)(b + (255 - b) * factor));
    
    return RGB(r, g, b);
}

COLORREF DarkenColor(COLORREF color, float factor) {
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    
    r = max(0, (int)(r * (1.0f - factor)));
    g = max(0, (int)(g * (1.0f - factor)));
    b = max(0, (int)(b * (1.0f - factor)));
    
    return RGB(r, g, b);
}

/* Function to draw a 3D bubble */
void Draw3DBubble(HDC hdc, int x, int y, COLORREF color, int size) {
    int centerX = x + size / 2;
    int centerY = y + size / 2;
    int radius = size / 2;
    int highlightRadius = radius / 2;
    HBRUSH brush, highlightBrush, shadowBrush;
    HPEN pen, oldPen;
    
    /* Create brushes and pen */
    brush = CreateSolidBrush(color);
    highlightBrush = CreateSolidBrush(LightenColor(color, 0.5f));
    shadowBrush = CreateSolidBrush(DarkenColor(color, 0.3f));
    pen = GetStockObject(NULL_PEN);
    
    /* Select null pen for smooth edges */
    oldPen = SelectObject(hdc, pen);
    
    /* Draw the main bubble */
    SelectObject(hdc, brush);
    Ellipse(hdc, x, y, x + size, y + size);
    
    /* Draw highlight (upper-left quadrant) */
    SelectObject(hdc, highlightBrush);
    Ellipse(hdc, 
            centerX - highlightRadius - radius/4, 
            centerY - highlightRadius - radius/4,
            centerX - radius/4, 
            centerY - radius/4);
    
    /* Create a subtle shadow on the bottom-right */
    SelectObject(hdc, shadowBrush);
    Ellipse(hdc, 
            centerX + radius/4, 
            centerY + radius/4,
            centerX + radius/4 + radius/3, 
            centerY + radius/4 + radius/3);
    
    /* Clean up */
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(highlightBrush);
    DeleteObject(shadowBrush);
}

/* Initialize the persistent offscreen buffer */
void InitOffscreenBuffer(HWND hwnd) {
    RECT clientRect;
    HDC hdc;
    
    /* Clean up previous buffer if it exists */
    if (offscreenDC != NULL) {
        SelectObject(offscreenDC, oldOffscreenBitmap);
        DeleteObject(offscreenBitmap);
        DeleteDC(offscreenDC);
    }
    
    /* Get client area size */
    GetClientRect(hwnd, &clientRect);
    hdc = GetDC(hwnd);
    
    /* Create persistent offscreen buffer */
    offscreenDC = CreateCompatibleDC(hdc);
    offscreenBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    oldOffscreenBitmap = (HBITMAP)SelectObject(offscreenDC, offscreenBitmap);
    
    /* Set drawing properties */
    SetGraphicsMode(offscreenDC, GM_ADVANCED);
    SetBkMode(offscreenDC, TRANSPARENT);
    
    ReleaseDC(hwnd, hdc);
    needFullRedraw = TRUE;
}

/* Clean up offscreen buffer resources */
void CleanupOffscreenBuffer(void) {
    if (offscreenDC != NULL) {
        SelectObject(offscreenDC, oldOffscreenBitmap);
        DeleteObject(offscreenBitmap);
        DeleteDC(offscreenDC);
        offscreenDC = NULL;
        offscreenBitmap = NULL;
    }
}

void DrawBoard(HDC hdc) {
    int x, y;
    HPEN pen;
    HPEN oldPen;
    RECT rect;
    RECT clientRect;
    int circleSize = CELL_SIZE - (2 * CELL_PADDING);
    float shrinkFactor = 1.0f;
    int offset;
    HWND hwnd = WindowFromDC(hdc);

    /* Get client area size */
    GetClientRect(hwnd, &clientRect);
    
    /* Create or validate offscreen buffer */
    if (offscreenDC == NULL) {
        InitOffscreenBuffer(hwnd);
    }

    /* Only redraw the entire board when needed */
    if (needFullRedraw) {
        /* Fill background with white */
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(offscreenDC, &clientRect, whiteBrush);
        DeleteObject(whiteBrush);

        /* Create null pen for smooth circles without borders */
        pen = GetStockObject(NULL_PEN);
        oldPen = SelectObject(offscreenDC, pen);

        /* Draw all non-animating bubbles */
        for (y = 0; y < BOARD_SIZE; y++) {
            for (x = 0; x < BOARD_SIZE; x++) {
                if (board[y][x] != -1 && (!popping || !popVisited[y][x])) {
                    COLORREF bubbleColor = colors[board[y][x]];
                    
                    /* Draw 3D bubble */
                    Draw3DBubble(offscreenDC, 
                        x * CELL_SIZE + CELL_PADDING, 
                        y * CELL_SIZE + CELL_PADDING, 
                        bubbleColor, 
                        circleSize
                    );
                } else if (board[y][x] == -1) {
                    /* Clear area where bubble used to be */
                    RECT clearRect;
                    HBRUSH whiteBrush;
                    
                    clearRect.left = x * CELL_SIZE;
                    clearRect.top = y * CELL_SIZE;
                    clearRect.right = (x + 1) * CELL_SIZE;
                    clearRect.bottom = (y + 1) * CELL_SIZE;
                    
                    whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
                    FillRect(offscreenDC, &clearRect, whiteBrush);
                    DeleteObject(whiteBrush);
                }
            }
        }
        
        /* Reset the flag */
        needFullRedraw = FALSE;
        
        /* Restore original pen */
        SelectObject(offscreenDC, oldPen);
    }
    
    /* For popping animation, we need to redraw just those bubbles */
    if (popping) {
        HDC animDC;
        HBITMAP animBitmap;
        HBITMAP oldAnimBitmap;
        
        /* Create a temporary DC for animation */
        animDC = CreateCompatibleDC(hdc);
        animBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        oldAnimBitmap = (HBITMAP)SelectObject(animDC, animBitmap);
        
        /* Copy the current offscreen buffer to animation buffer */
        BitBlt(animDC, 0, 0, clientRect.right, clientRect.bottom, offscreenDC, 0, 0, SRCCOPY);

        /* Calculate shrink factor for popping animation */
        shrinkFactor = 1.0f - ((float)popFrame / ANIM_FRAMES);
        
        /* Set up for drawing */
        SetGraphicsMode(animDC, GM_ADVANCED);
        SetBkMode(animDC, TRANSPARENT);
        pen = GetStockObject(NULL_PEN);
        oldPen = SelectObject(animDC, pen);
        
        /* Draw only the popping bubbles */
        for (y = 0; y < BOARD_SIZE; y++) {
            for (x = 0; x < BOARD_SIZE; x++) {
                if (board[y][x] != -1 && popVisited[y][x]) {
                    /* Get circle size based on animation state */
                    int currentSize = (int)(circleSize * shrinkFactor);
                    COLORREF bubbleColor = colors[board[y][x]];
                    
                    /* Make bubble brighter as it pops */
                    bubbleColor = LightenColor(bubbleColor, (float)popFrame / ANIM_FRAMES);
                    
                    /* Clear bubble area first */
                    {
                        RECT clearRect;
                        HBRUSH whiteBrush;
                        
                        clearRect.left = x * CELL_SIZE;
                        clearRect.top = y * CELL_SIZE;
                        clearRect.right = (x + 1) * CELL_SIZE;
                        clearRect.bottom = (y + 1) * CELL_SIZE;
                        
                        whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
                        FillRect(animDC, &clearRect, whiteBrush);
                        DeleteObject(whiteBrush);
                    }
                    
                    /* Center the shrinking bubble */
                    offset = (circleSize - currentSize) / 2;
                    
                    /* Draw 3D bubble */
                    Draw3DBubble(animDC, 
                        x * CELL_SIZE + CELL_PADDING + offset, 
                        y * CELL_SIZE + CELL_PADDING + offset, 
                        bubbleColor, 
                        currentSize
                    );
                }
            }
        }
        
        SelectObject(animDC, oldPen);
        
        /* If animation is done, update the offscreen buffer for next frame */
        if (popFrame >= ANIM_FRAMES - 1) {
            needFullRedraw = TRUE;
        }
        
        /* Copy the animation buffer to screen */
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, animDC, 0, 0, SRCCOPY);
        
        /* Clean up animation DC */
        SelectObject(animDC, oldAnimBitmap);
        DeleteObject(animBitmap);
        DeleteDC(animDC);
    } else {
        /* For non-animation frames, just copy the offscreen buffer */
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, offscreenDC, 0, 0, SRCCOPY);
    }

    /* Game over text */
    if (gameOver) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 0, 0)); /* Red text */
        rect.left = 0;
        rect.top = SCREEN_HEIGHT / 2 - 20;
        rect.right = SCREEN_WIDTH;
        rect.bottom = SCREEN_HEIGHT / 2 + 20;
        DrawText(hdc, "Game Over! Click to restart", -1, &rect, 
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

/* Start pop animation for bubbles marked in the visited array */
void StartPopAnimation(void) {
    HWND hwnd;
    /* Reset animation state */
    popFrame = 0;
    popping = TRUE;
    
    /* Mark for animation update */
    needFullRedraw = TRUE;
    
    /* Start the animation timer */
    hwnd = GetActiveWindow();
    SetTimer(hwnd, TIMER_ID, ANIM_SPEED, NULL);
}

/* Function to update window title with current score */
void UpdateWindowTitle(HWND hwnd) {
    char windowTitle[64];
    wsprintf(windowTitle, "%s - Score: %d", WINDOW_TITLE, score);
    SetWindowText(hwnd, windowTitle);
}

void HandleClick(int x, int y) {
    int color = board[y][x];
    BOOL visited[BOARD_SIZE][BOARD_SIZE] = {FALSE};
    int clusterSize;
    HWND hwnd;

    if (color == -1) return;

    clusterSize = FindCluster(x, y, color, visited);

    if (clusterSize < 2) return;

    score += clusterSize * clusterSize;
    
    /* Get window handle and update title */
    hwnd = GetActiveWindow();
    UpdateWindowTitle(hwnd);

    /* Copy the visited array to the popVisited array for animation */
    {
        int xx, yy;
        for (yy = 0; yy < BOARD_SIZE; yy++) {
            for (xx = 0; xx < BOARD_SIZE; xx++) {
                popVisited[yy][xx] = visited[yy][xx];
            }
        }
    }
    
    /* Start pop animation */
    StartPopAnimation();
    
    /* Note: The actual removal of bubbles and gravity application happens 
       after the animation completes, in the WM_TIMER handler */
}

int FindCluster(int x, int y, int color, BOOL visited[BOARD_SIZE][BOARD_SIZE]) {
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE || board[y][x] != color || visited[y][x]) {
        return 0;
    }

    visited[y][x] = TRUE;

    return 1 + FindCluster(x - 1, y, color, visited) +
               FindCluster(x + 1, y, color, visited) +
               FindCluster(x, y - 1, color, visited) +
               FindCluster(x, y + 1, color, visited);
}

void ApplyGravity(void) {
    int x, y, empty;
    for (x = 0; x < BOARD_SIZE; x++) {
        empty = BOARD_SIZE - 1;
        for (y = BOARD_SIZE - 1; y >= 0; y--) {
            if (board[y][x] != -1) {
                board[empty][x] = board[y][x];
                empty--;
            }
        }
        for (y = empty; y >= 0; y--) {
            board[y][x] = -1;
        }
    }
}

void ShiftColumns(void) {
    int x, xx, y;
    for (x = 0; x < BOARD_SIZE - 1; x++) {
        if (board[BOARD_SIZE - 1][x] == -1) {
            for (xx = x + 1; xx < BOARD_SIZE; xx++) {
                if (board[BOARD_SIZE - 1][xx] != -1) {
                    for (y = 0; y < BOARD_SIZE; y++) {
                        board[y][x] = board[y][xx];
                        board[y][xx] = -1;
                    }
                    break;
                }
            }
        }
    }
}

BOOL IsGameOver(void) {
    int x, y;
    BOOL visited[BOARD_SIZE][BOARD_SIZE];

    for (y = 0; y < BOARD_SIZE; y++) {
        for (x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] != -1) {
                ZeroMemory(visited, sizeof(visited));
                if (FindCluster(x, y, board[y][x], visited) >= 2) {
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}

void RestartGame(HWND hwnd) {
    /* Stop any active animation */
    if (popping) {
        KillTimer(hwnd, TIMER_ID);
        popping = FALSE;
    }
    
    /* Reset game state */
    InitBoard();
    score = 0;
    gameOver = FALSE;
    
    /* Clear pop visited array */
    {
        int x, y;
        for (y = 0; y < BOARD_SIZE; y++) {
            for (x = 0; x < BOARD_SIZE; x++) {
                popVisited[y][x] = FALSE;
            }
        }
    }
    
    /* Force full redraw of the board */
    needFullRedraw = TRUE;
    
    UpdateWindowTitle(hwnd);
}