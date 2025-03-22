#include <windows.h>
#include <stdlib.h>
#include <time.h>

/* Constants */
#define WINDOW_WIDTH     640
#define WINDOW_HEIGHT    480
#define PADDLE_WIDTH     80
#define PADDLE_HEIGHT    15
#define PADDLE_Y         (WINDOW_HEIGHT - 30)
#define BALL_SIZE        10
#define BRICK_ROWS       5
#define BRICK_COLS       10
#define BRICK_WIDTH      ((WINDOW_WIDTH - 20) / BRICK_COLS)
#define BRICK_HEIGHT     20
#define BRICK_GAP        2
#define BALL_SPEED_INITIAL 5       /* Starting ball speed */
#define BALL_SPEED_MAX     12      /* Maximum ball speed */
#define BALL_SPEED_INCREMENT 0.15  /* Speed increase per brick hit */

/* Global Variables */
HWND    g_hWnd = NULL;
HDC     g_hDC = NULL;
HDC     g_hdcMemory = NULL;   /* Memory DC for double buffering */
HBITMAP g_hbmMemory = NULL;   /* Memory bitmap for double buffering */
HBITMAP g_hbmOld = NULL;      /* For storing the old bitmap */
HBRUSH  g_hBrushBall = NULL;
HBRUSH  g_hBrushPaddle = NULL;
HBRUSH  g_hBrushBrick = NULL;
HBRUSH  g_hBrushBackground = NULL;
HCURSOR g_hEmptyCursor = NULL; /* Empty cursor to hide mouse pointer */
int     g_PaddleX = (WINDOW_WIDTH - PADDLE_WIDTH) / 2;
int     g_BallX = WINDOW_WIDTH / 2;
int     g_BallY = WINDOW_HEIGHT / 2;
int     g_BallVX = BALL_SPEED_INITIAL;
int     g_BallVY = -BALL_SPEED_INITIAL;
float   g_CurrentBallSpeed = BALL_SPEED_INITIAL; /* Current ball speed (increases over time) */
int     g_Bricks[BRICK_ROWS][BRICK_COLS];
int     g_Score = 0;
int     g_Lives = 3;
int     g_BricksDestroyed = 0; /* Count of destroyed bricks for speed calculation */
BOOL    g_GameOver = FALSE;
BOOL    g_GamePaused = FALSE;

/* Function Prototypes */
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void GameInit(void);
void GameUpdate(void);
void GameRender(void);
void ResetBall(void);
void HideMouseCursor(void);
void ShowMouseCursor(void);

/* WinMain - Entry Point */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc;
    MSG msg;
    RECT rect;
    HANDLE hProcess;
    
    /* Set process priority to HIGH_PRIORITY_CLASS (one below REALTIME_PRIORITY_CLASS) */
    hProcess = GetCurrentProcess();
    SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);
    
    /* Register Window Class */
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = NULL;  /* Set to NULL to handle cursor ourselves */
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "BreakoutClass";
    
    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    /* Create Window */
    rect.left = 0;
    rect.top = 0;
    rect.right = WINDOW_WIDTH;
    rect.bottom = WINDOW_HEIGHT;
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    
    g_hWnd = CreateWindow(
        "BreakoutClass",
        "Win32 Breakout",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, hInstance, NULL
    );
    
    if (g_hWnd == NULL)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    /* Create Resources */
    g_hDC = GetDC(g_hWnd);
    
    /* Initialize Double Buffering */
    g_hdcMemory = CreateCompatibleDC(g_hDC);
    g_hbmMemory = CreateCompatibleBitmap(g_hDC, WINDOW_WIDTH, WINDOW_HEIGHT);
    g_hbmOld = SelectObject(g_hdcMemory, g_hbmMemory);
    
    /* Create fully transparent empty cursor (absolutely invisible) */
    {
        ICONINFO iconInfo;
        HBITMAP hBitmap;
        
        /* Create 1x1 transparent bitmap for cursor */
        hBitmap = CreateCompatibleBitmap(GetDC(NULL), 1, 1);
        
        /* Setup icon information structure */
        iconInfo.fIcon = FALSE;  /* Not an icon, a cursor */
        iconInfo.xHotspot = 0;   /* Hotspot at corner */
        iconInfo.yHotspot = 0;
        iconInfo.hbmMask = hBitmap;  /* Both mask and color are the same bitmap */
        iconInfo.hbmColor = hBitmap; /* for complete transparency */
        
        /* Create the cursor from our transparent bitmap */
        g_hEmptyCursor = CreateIconIndirect(&iconInfo);
        
        /* Clean up the bitmap, it's no longer needed */
        DeleteObject(hBitmap);
    }
    
    g_hBrushBall = CreateSolidBrush(RGB(255, 255, 255));
    g_hBrushPaddle = CreateSolidBrush(RGB(0, 255, 0));
    g_hBrushBrick = CreateSolidBrush(RGB(255, 0, 0));
    g_hBrushBackground = CreateSolidBrush(RGB(0, 0, 0));
    
    /* Initialize Game */
    GameInit();
    
    /* Show Window */
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    
    /* Message Loop */
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    /* Cleanup */
    DeleteObject(g_hBrushBall);
    DeleteObject(g_hBrushPaddle);
    DeleteObject(g_hBrushBrick);
    DeleteObject(g_hBrushBackground);
    
    /* Clean up double buffering */
    SelectObject(g_hdcMemory, g_hbmOld);
    DeleteObject(g_hbmMemory);
    DeleteDC(g_hdcMemory);
    
    /* Clean up cursor */
    DestroyCursor(g_hEmptyCursor);
    
    ReleaseDC(g_hWnd, g_hDC);
    
    return (int)msg.wParam;
}

/* Window Procedure */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static DWORD lastTime = 0;
    DWORD currentTime;
    POINT pt;
    static BOOL needsRedraw = TRUE;  /* Flag to control rendering */
    
    switch (message)
    {
        case WM_CREATE:
            /* Set Timer for Game Loop - 30 FPS is sufficient for this game */
            SetTimer(hWnd, 1, 33, NULL);  /* ~30 FPS */
            lastTime = GetTickCount();
            /* Hide cursor initially */
            HideMouseCursor();
            return 0;
            
        case WM_TIMER:
            if (!g_GameOver && !g_GamePaused)
            {
                /* Get current time for frame rate control */
                currentTime = GetTickCount();
                
                /* Update Game State */
                GameUpdate();
                
                /* Set redraw flag */
                needsRedraw = TRUE;
                
                /* Only render if at least 33ms (30fps) have passed since last render */
                if ((currentTime - lastTime) >= 33 && needsRedraw)
                {
                    /* Render Game */
                    GameRender();
                    
                    /* Reset redraw flag */
                    needsRedraw = FALSE;
                    
                    /* Update lastTime */
                    lastTime = currentTime;
                }
                
                /* Keep cursor hidden during gameplay */
                HideMouseCursor();
            }
            else
            {
                /* Show cursor when game is paused or over */
                ShowMouseCursor();
                
                /* Only render paused/game over screen once */
                if (needsRedraw)
                {
                    GameRender();
                    needsRedraw = FALSE;
                }
            }
            return 0;
            
        case WM_MOUSEMOVE:
            /* Always hide cursor during gameplay */
            if (!g_GameOver && !g_GamePaused)
            {
                HideMouseCursor();
                
                /* Get mouse x position in client coordinates */
                pt.x = LOWORD(lParam);
                pt.y = HIWORD(lParam);
                
                /* Center paddle on mouse x position */
                g_PaddleX = pt.x - (PADDLE_WIDTH / 2);
                
                /* Keep paddle within screen bounds */
                if (g_PaddleX < 0)
                    g_PaddleX = 0;
                if (g_PaddleX > WINDOW_WIDTH - PADDLE_WIDTH)
                    g_PaddleX = WINDOW_WIDTH - PADDLE_WIDTH;
                
                /* Set redraw flag - will be processed in timer event */
                needsRedraw = TRUE;
            }
            else
            {
                /* Show cursor when game is paused or over */
                ShowMouseCursor();
            }
            return 0;
            
        case WM_KEYDOWN:
            switch (wParam)
            {
                case VK_SPACE:
                    /* Pause/Resume Game */
                    g_GamePaused = !g_GamePaused;
                    
                    /* Toggle cursor visibility based on game state */
                    if (g_GamePaused)
                        ShowMouseCursor();
                    else
                        HideMouseCursor();
                    
                    /* Force a redraw when pausing/unpausing */
                    needsRedraw = TRUE;
                    GameRender();
                    needsRedraw = FALSE;
                    break;
                    
                case 'R':
                case 'r':
                    /* Restart Game */
                    if (g_GameOver)
                    {
                        GameInit();
                        g_GameOver = FALSE;
                        HideMouseCursor();
                        /* Force a redraw on restart */
                        needsRedraw = TRUE;
                        GameRender();
                        needsRedraw = FALSE;
                    }
                    break;
            }
            return 0;
            
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                BeginPaint(hWnd, &ps);
                
                /* WM_PAINT is triggered when window needs repainting */
                /* We should render even if we weren't planning to */
                GameRender();
                
                /* Reset redraw flag since we just painted */
                needsRedraw = FALSE;
                
                EndPaint(hWnd, &ps);
            }
            return 0;
            
        case WM_SETCURSOR:
            /* Handle cursor display based on game state */
            if (LOWORD(lParam) == HTCLIENT)  /* In client area */
            {
                if (!g_GameOver && !g_GamePaused)
                {
                    /* Keep cursor hidden during gameplay */
                    HideMouseCursor();
                    return TRUE;  /* We've handled the cursor */
                }
                else
                {
                    /* Show cursor during pause/game over */
                    ShowMouseCursor();
                    return TRUE;
                }
            }
            break;  /* Let Windows handle cursor for non-client areas */
            
        case WM_ACTIVATE:
            /* Show cursor when window is deactivated, hide when activated during gameplay */
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                ShowMouseCursor();
            }
            else if (!g_GameOver && !g_GamePaused)
            {
                HideMouseCursor();
            }
            return 0;
            
        case WM_DESTROY:
            KillTimer(hWnd, 1);
            /* Make sure to restore cursor before quitting */
            ShowMouseCursor();
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}

/* Initialize Game */
void GameInit(void)
{
    int i, j;
    
    /* Initialize Random Seed */
    srand((unsigned int)time(NULL));
    
    /* Reset Ball */
    g_BallX = WINDOW_WIDTH / 2;
    g_BallY = WINDOW_HEIGHT / 2;
    g_CurrentBallSpeed = BALL_SPEED_INITIAL;  /* Reset ball speed to initial value */
    g_BallVX = (rand() % 2 == 0) ? g_CurrentBallSpeed : -g_CurrentBallSpeed;
    g_BallVY = -g_CurrentBallSpeed;
    
    /* Reset Paddle */
    g_PaddleX = (WINDOW_WIDTH - PADDLE_WIDTH) / 2;
    
    /* Initialize Bricks */
    for (i = 0; i < BRICK_ROWS; i++)
    {
        for (j = 0; j < BRICK_COLS; j++)
        {
            g_Bricks[i][j] = 1;  /* 1 = Active, 0 = Destroyed */
        }
    }
    
    /* Reset Score and Lives */
    g_Score = 0;
    g_Lives = 3;
    g_BricksDestroyed = 0;  /* Reset brick counter */
    g_GameOver = FALSE;
    g_GamePaused = FALSE;
}

/* Simple Ball-Paddle Collision Check */
BOOL CheckPaddleCollision(int ballX, int ballY, int ballSize, int paddleX, int paddleY, int paddleWidth, int paddleHeight)
{
    return (ballX + ballSize > paddleX &&
            ballX < paddleX + paddleWidth &&
            ballY + ballSize > paddleY &&
            ballY < paddleY + paddleHeight);
}

/* Update Game State */
void GameUpdate(void)
{
    int i, j, hitPos, k, l;
    int brickX, brickY, brickWidth, brickHeight;
    int ballRight, ballBottom;
    int startRow, endRow, startCol, endCol;
    int bricksLeft;
    int leftDist, rightDist, topDist, bottomDist;
    int minDist;
    int oldBallX, oldBallY;  /* Previous position for tunneling prevention */
    int crossingX, crossX, crossY;  /* For tunneling detection calculations */
    float crossingRatio, tunnelCheckT;  /* For trajectory calculations */
    BOOL hitBrick;
    BOOL allDestroyed;
    BOOL collision;
    
    /* Initialize values */
    hitBrick = FALSE;
    
    /* Cache brick dimensions to avoid recalculating */
    brickWidth = BRICK_WIDTH - BRICK_GAP*2;
    brickHeight = BRICK_HEIGHT - BRICK_GAP*2;
    
    /* Store previous position for tunneling prevention */
    oldBallX = g_BallX;
    oldBallY = g_BallY;
    
    /* Move Ball */
    g_BallX += g_BallVX;
    g_BallY += g_BallVY;
    
    /* Cache ball bounds */
    ballRight = g_BallX + BALL_SIZE;
    ballBottom = g_BallY + BALL_SIZE;
    
    /* Wall Collision - faster direct checks */
    if (g_BallX <= 0)
    {
        g_BallX = 0;
        g_BallVX = -g_BallVX;
    }
    else if (ballRight >= WINDOW_WIDTH)
    {
        g_BallX = WINDOW_WIDTH - BALL_SIZE;
        g_BallVX = -g_BallVX;
    }
    
    if (g_BallY <= 0)
    {
        g_BallY = 0;
        g_BallVY = -g_BallVY;
    }
    
    /* Bottom Collision = Life Lost */
    if (g_BallY >= WINDOW_HEIGHT)
    {
        g_Lives--;
        if (g_Lives <= 0)
        {
            g_GameOver = TRUE;
        }
        else
        {
            ResetBall();
        }
        
        /* Early return after life lost */
        return;
    }
    
    /* Paddle Collision - Regular collision first */
    if (CheckPaddleCollision(g_BallX, g_BallY, BALL_SIZE, g_PaddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT))
    {
        /* Top of paddle collision only */
        if (g_BallY + BALL_SIZE - PADDLE_Y < 8)
        {
            g_BallVY = -abs(g_BallVY);
            g_BallY = PADDLE_Y - BALL_SIZE; /* Prevent sticking */
            
            /* Angle ball based on hit position */
            hitPos = (g_BallX + BALL_SIZE/2) - g_PaddleX;
            if (hitPos < PADDLE_WIDTH/3)
                g_BallVX = -g_CurrentBallSpeed;
            else if (hitPos > (PADDLE_WIDTH*2)/3)
                g_BallVX = g_CurrentBallSpeed;
            else
                g_BallVX = (g_BallVX > 0) ? g_CurrentBallSpeed/2 : -g_CurrentBallSpeed/2;
        }
    }
    /* Tunneling prevention for paddle */
    else if (g_BallVY > 0) /* Only check when ball is moving down */
    {
        /* Was ball above paddle in previous frame and below it now? */
        if (oldBallY + BALL_SIZE <= PADDLE_Y && g_BallY + BALL_SIZE >= PADDLE_Y)
        {
            /* Find X position at crossing point */
            crossingRatio = (float)(PADDLE_Y - (oldBallY + BALL_SIZE)) / 
                            (float)(g_BallY + BALL_SIZE - (oldBallY + BALL_SIZE));
            crossingX = oldBallX + (int)(crossingRatio * (g_BallX - oldBallX));
            
            /* Did it cross through paddle width? */
            if (crossingX + BALL_SIZE > g_PaddleX && crossingX < g_PaddleX + PADDLE_WIDTH)
            {
                /* Handle collision like normal */
                g_BallX = crossingX;
                g_BallVY = -abs(g_BallVY);
                g_BallY = PADDLE_Y - BALL_SIZE;
                
                /* Same angle calculation */
                hitPos = (g_BallX + BALL_SIZE/2) - g_PaddleX;
                if (hitPos < PADDLE_WIDTH/3)
                    g_BallVX = -g_CurrentBallSpeed;
                else if (hitPos > (PADDLE_WIDTH*2)/3)
                    g_BallVX = g_CurrentBallSpeed;
                else
                    g_BallVX = (g_BallVX > 0) ? g_CurrentBallSpeed/2 : -g_CurrentBallSpeed/2;
            }
        }
    }

    /* Calculate brick rows/cols that might have collisions */
    startRow = (g_BallY - 40) / BRICK_HEIGHT;
    endRow = (ballBottom - 40) / BRICK_HEIGHT;
    startCol = g_BallX / BRICK_WIDTH;
    endCol = ballRight / BRICK_WIDTH;
    
    /* Clamp to valid ranges */
    if (startRow < 0) startRow = 0;
    if (endRow >= BRICK_ROWS) endRow = BRICK_ROWS - 1;
    if (startCol < 0) startCol = 0;
    if (endCol >= BRICK_COLS) endCol = BRICK_COLS - 1;
    
    /* Check bricks for collision */
    for (i = startRow; i <= endRow && !hitBrick; i++)
    {
        for (j = startCol; j <= endCol && !hitBrick; j++)
        {
            if (g_Bricks[i][j])
            {
                brickX = j * BRICK_WIDTH + BRICK_GAP;
                brickY = i * BRICK_HEIGHT + BRICK_GAP + 40;
                collision = FALSE;
                
                /* Normal collision */
                if (g_BallX < brickX + brickWidth &&
                    ballRight > brickX &&
                    g_BallY < brickY + brickHeight &&
                    ballBottom > brickY)
                {
                    collision = TRUE;
                }
                /* Tunneling check for fast balls */
                else if (abs(g_BallVX) > brickWidth || abs(g_BallVY) > brickHeight)
                {
                    /* Vertical edge crossing */
                    if ((oldBallY + BALL_SIZE <= brickY && ballBottom >= brickY) ||
                        (oldBallY >= brickY + brickHeight && g_BallY <= brickY + brickHeight))
                    {
                        tunnelCheckT = 0.0f;
                        if (oldBallY + BALL_SIZE <= brickY)
                        {
                            tunnelCheckT = (float)(brickY - (oldBallY + BALL_SIZE)) / 
                                           (float)(ballBottom - (oldBallY + BALL_SIZE));
                        }
                        else
                        {
                            tunnelCheckT = (float)((brickY + brickHeight) - oldBallY) / 
                                           (float)(g_BallY - oldBallY);
                        }
                        
                        crossX = oldBallX + (int)(tunnelCheckT * (g_BallX - oldBallX));
                        if (crossX + BALL_SIZE > brickX && crossX < brickX + brickWidth)
                        {
                            collision = TRUE;
                        }
                    }
                    
                    /* Horizontal edge crossing */
                    if (!collision &&
                        ((oldBallX + BALL_SIZE <= brickX && ballRight >= brickX) ||
                         (oldBallX >= brickX + brickWidth && g_BallX <= brickX + brickWidth)))
                    {
                        tunnelCheckT = 0.0f;
                        if (oldBallX + BALL_SIZE <= brickX)
                        {
                            tunnelCheckT = (float)(brickX - (oldBallX + BALL_SIZE)) / 
                                           (float)(ballRight - (oldBallX + BALL_SIZE));
                        }
                        else
                        {
                            tunnelCheckT = (float)((brickX + brickWidth) - oldBallX) / 
                                           (float)(g_BallX - oldBallX);
                        }
                        
                        crossY = oldBallY + (int)(tunnelCheckT * (g_BallY - oldBallY));
                        if (crossY + BALL_SIZE > brickY && crossY < brickY + brickHeight)
                        {
                            collision = TRUE;
                        }
                    }
                }
                
                /* Handle brick collision */
                if (collision)
                {
                    /* Destroy brick and update score */
                    g_Bricks[i][j] = 0;
                    g_Score += 10;
                    g_BricksDestroyed++;
                    hitBrick = TRUE;
                    
                    /* Increase ball speed */
                    g_CurrentBallSpeed += BALL_SPEED_INCREMENT;
                    if (g_CurrentBallSpeed > BALL_SPEED_MAX)
                    {
                        g_CurrentBallSpeed = BALL_SPEED_MAX;
                    }
                    
                    /* Adjust velocity with new speed */
                    if (g_BallVX > 0)
                        g_BallVX = g_CurrentBallSpeed;
                    else
                        g_BallVX = -g_CurrentBallSpeed;
                        
                    if (g_BallVY > 0)
                        g_BallVY = g_CurrentBallSpeed;
                    else
                        g_BallVY = -g_CurrentBallSpeed;
                    
                    /* Check for win (no bricks left) */
                    bricksLeft = 0;
                    for (k = 0; k < BRICK_ROWS; k++)
                    {
                        for (l = 0; l < BRICK_COLS; l++)
                        {
                            if (g_Bricks[k][l])
                            {
                                bricksLeft++;
                                break;
                            }
                        }
                        if (bricksLeft > 0) break;
                    }
                    
                    if (bricksLeft == 0)
                    {
                        GameInit();
                        return;
                    }
                    
                    /* Determine which side was hit */
                    leftDist = abs((g_BallX + BALL_SIZE) - brickX);
                    rightDist = abs(g_BallX - (brickX + brickWidth));
                    topDist = abs((g_BallY + BALL_SIZE) - brickY);
                    bottomDist = abs(g_BallY - (brickY + brickHeight));
                    
                    /* Find closest edge */
                    minDist = leftDist;
                    if (rightDist < minDist) minDist = rightDist;
                    if (topDist < minDist) minDist = topDist;
                    if (bottomDist < minDist) minDist = bottomDist;
                    
                    /* Bounce based on hit side */
                    if (minDist == leftDist || minDist == rightDist)
                    {
                        g_BallVX = -g_BallVX;
                    }
                    else
                    {
                        g_BallVY = -g_BallVY;
                    }
                }
            }
        }
    }
}

/* Reset Ball Position */
void ResetBall(void)
{
    g_BallX = WINDOW_WIDTH / 2;
    g_BallY = WINDOW_HEIGHT / 2;
    /* Keep the current ball speed when resetting, don't slow down */
    g_BallVX = (rand() % 2 == 0) ? g_CurrentBallSpeed : -g_CurrentBallSpeed;
    g_BallVY = -g_CurrentBallSpeed;
}

/* Render Game */
void GameRender(void)
{
    int i, j;
    static char scoreText[64]; /* Static for better performance */
    static RECT rect;
    static RECT brickRect;
    static RECT paddleRect;
    static RECT ballRect;
    static SIZE textSize;
    static const char* gameOverText = "GAME OVER - Press 'R' to Restart";
    static const char* pausedText = "PAUSED - Press SPACE to Resume";
    static int brickWidth = 0;
    static int brickHeight = 0;
    
    /* Cache values for better performance */
    if (brickWidth == 0)
    {
        brickWidth = BRICK_WIDTH - BRICK_GAP*2;
        brickHeight = BRICK_HEIGHT - BRICK_GAP*2;
    }
    
    /* Get client area for double buffering */
    GetClientRect(g_hWnd, &rect);
    
    /* Clear the memory DC */
    FillRect(g_hdcMemory, &rect, g_hBrushBackground);
    
    /* Draw Bricks - optimize by drawing only active bricks */
    for (i = 0; i < BRICK_ROWS; i++)
    {
        for (j = 0; j < BRICK_COLS; j++)
        {
            if (g_Bricks[i][j])
            {
                brickRect.left = j * BRICK_WIDTH + BRICK_GAP;
                brickRect.top = i * BRICK_HEIGHT + BRICK_GAP + 40;  /* 40px from top */
                brickRect.right = brickRect.left + brickWidth;
                brickRect.bottom = brickRect.top + brickHeight;
                
                FillRect(g_hdcMemory, &brickRect, g_hBrushBrick);
            }
        }
    }
    
    /* Draw Paddle */
    paddleRect.left = g_PaddleX;
    paddleRect.top = PADDLE_Y;
    paddleRect.right = g_PaddleX + PADDLE_WIDTH;
    paddleRect.bottom = PADDLE_Y + PADDLE_HEIGHT;
    
    FillRect(g_hdcMemory, &paddleRect, g_hBrushPaddle);
    
    /* Draw Ball */
    ballRect.left = g_BallX;
    ballRect.top = g_BallY;
    ballRect.right = g_BallX + BALL_SIZE;
    ballRect.bottom = g_BallY + BALL_SIZE;
    
    FillRect(g_hdcMemory, &ballRect, g_hBrushBall);
    
    /* Draw Score */
    wsprintf(scoreText, "Score: %d    Lives: %d", g_Score, g_Lives);
    SetBkMode(g_hdcMemory, TRANSPARENT);
    SetTextColor(g_hdcMemory, RGB(255, 255, 255));
    TextOut(g_hdcMemory, 10, 10, scoreText, lstrlen(scoreText));
    
    /* Draw Game Over or Paused */
    if (g_GameOver)
    {
        GetTextExtentPoint32(g_hdcMemory, gameOverText, lstrlen(gameOverText), &textSize);
        TextOut(g_hdcMemory, (WINDOW_WIDTH - textSize.cx) / 2, WINDOW_HEIGHT / 2, 
                gameOverText, lstrlen(gameOverText));
    }
    else if (g_GamePaused)
    {
        GetTextExtentPoint32(g_hdcMemory, pausedText, lstrlen(pausedText), &textSize);
        TextOut(g_hdcMemory, (WINDOW_WIDTH - textSize.cx) / 2, WINDOW_HEIGHT / 2, 
                pausedText, lstrlen(pausedText));
    }
    
    /* BitBlt the memory DC to the screen */
    BitBlt(g_hDC, 0, 0, rect.right, rect.bottom, g_hdcMemory, 0, 0, SRCCOPY);
}


/* Hide Mouse Cursor */
void HideMouseCursor(void)
{
    /* Setting to NULL completely hides the cursor system-wide */
    /* Using g_hEmptyCursor makes it invisible but still tracked */
    SetCursor(g_hEmptyCursor);
    
    /* For complete elimination of cursor through the OS */
    while (ShowCursor(FALSE) >= 0)
        ; /* Loop until cursor is fully hidden */
}

/* Show Mouse Cursor */
void ShowMouseCursor(void)
{
    /* Restore default system cursor */
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    /* Ensure cursor is visible */
    while (ShowCursor(TRUE) < 0)
        ; /* Loop until cursor is fully visible */
}