#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define GRID_SIZE 4
#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 400 

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawGrid(HDC hdc);
void DisplayAllTiles(HDC hdc);
void InitializeGame(void);
void AddRandomTile(void);
void MoveTiles(int);
BOOL GameOver(void);
COLORREF GetTileColor(int);
COLORREF GetTileFontColor(int);

int grid[GRID_SIZE][GRID_SIZE] = {0};
HINSTANCE hInst;
char szAppName[] = "2048";
HFONT hFont;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    HWND hwnd;
    MSG msg;
    WNDCLASS wndclass;

    hInst = hInstance;

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

    if (!RegisterClass(&wndclass))
        return 0;

    hwnd = CreateWindow(szAppName, "2048",
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        WINDOW_WIDTH+7, WINDOW_HEIGHT+26,
                        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    srand((unsigned int)time(NULL));
    InitializeGame();

    hFont = CreateFont(WINDOW_HEIGHT / (GRID_SIZE * 3), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                       ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

    InvalidateRect(hwnd, NULL, TRUE);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(hFont);
    return msg.wParam;
}

// Function to display all possible tiles for color reference
void DisplayAllTiles(HDC hdc)
{
    int value = 2;
    RECT rect;
    char str[10];
    HBRUSH hBrush, hOldBrush;
    int tileWidth = WINDOW_WIDTH / 4;
    int tileHeight = WINDOW_HEIGHT / 6;
    int col = 0;
    
    // Set transparent background mode and font
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, hFont);
    
    // Clear the window
    rect.left = 0;
    rect.top = 0;
    rect.right = WINDOW_WIDTH;
    rect.bottom = WINDOW_HEIGHT;
    FillRect(hdc, &rect, GetStockObject(WHITE_BRUSH));
    
    // Display each possible tile value
    while (value <= 2048 || col < 4) {
        int row = col / 2;
        int colPos = col % 2;
        
        rect.left = colPos * tileWidth + 50;
        rect.top = row * tileHeight + 20;
        rect.right = rect.left + tileWidth - 20;
        rect.bottom = rect.top + tileHeight - 20;
        
        hBrush = CreateSolidBrush(GetTileColor(value));
        hOldBrush = SelectObject(hdc, hBrush);
        
        FillRect(hdc, &rect, hBrush);
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
        
        sprintf(str, "%d", value);
        SetTextColor(hdc, GetTileFontColor(value));
        DrawText(hdc, str, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(hdc, hOldBrush);
        DeleteObject(hBrush);
        
        value *= 2;
        col++;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    PAINTSTRUCT ps;
    static BOOL showColorReference = FALSE;

    switch (message)
    {
    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        if (showColorReference)
            DisplayAllTiles(hdc);
        else
            DrawGrid(hdc);
        EndPaint(hwnd, &ps);
        return 0;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_LEFT:
            MoveTiles(0);
            break;
        case VK_RIGHT:
            MoveTiles(1);
            break;
        case VK_UP:
            MoveTiles(2);
            break;
        case VK_DOWN:
            MoveTiles(3);
            break;
        case 'C':
            // Toggle color reference display
            showColorReference = !showColorReference;
            break;
        }
        InvalidateRect(hwnd, NULL, TRUE);
        if (!showColorReference && GameOver())
            MessageBox(hwnd, "Game Over!", "2048", MB_OK);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

void DrawGrid(HDC hdc)
{
    int i, j;
    RECT rect;
    char str[10];
    HFONT hOldFont;
    HBRUSH hBrush, hOldBrush;
    HPEN hPen, hOldPen;

    hOldFont = SelectObject(hdc, hFont);
    hPen = CreatePen(PS_SOLID, 2, RGB(128, 128, 128));  // Gray pen for grid lines
    hOldPen = SelectObject(hdc, hPen);

    for (i = 0; i < GRID_SIZE; i++)
    {
        for (j = 0; j < GRID_SIZE; j++)
        {
            rect.left = j * WINDOW_WIDTH / GRID_SIZE;
            rect.top = i * WINDOW_HEIGHT / GRID_SIZE;
            rect.right = (j + 1) * WINDOW_WIDTH / GRID_SIZE;
            rect.bottom = (i + 1) * WINDOW_HEIGHT / GRID_SIZE;

            hBrush = CreateSolidBrush(GetTileColor(grid[i][j]));
            hOldBrush = SelectObject(hdc, hBrush);
            
            FillRect(hdc, &rect, hBrush);
            
            // Draw grid lines
            Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);

            SelectObject(hdc, hOldBrush);
            DeleteObject(hBrush);

            if (grid[i][j] != 0)
            {
                sprintf(str, "%d", grid[i][j]);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, GetTileFontColor(grid[i][j]));
                DrawText(hdc, str, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    SelectObject(hdc, hOldFont);
}

void InitializeGame(void)
{
    int i, j;

    for (i = 0; i < GRID_SIZE; i++)
        for (j = 0; j < GRID_SIZE; j++)
            grid[i][j] = 0;

    AddRandomTile();
    AddRandomTile();
}

void AddRandomTile(void)
{
    int i, j;

    do
    {
        i = rand() % GRID_SIZE;
        j = rand() % GRID_SIZE;
    } while (grid[i][j] != 0);

    grid[i][j] = (rand() % 2 + 1) * 2;
}

void MoveTiles(int direction)
{
    int i, j, k;
    BOOL moved = FALSE;
    BOOL merged[GRID_SIZE][GRID_SIZE] = {FALSE};  // Track which tiles were merged this move

    switch (direction)
    {
    case 0: // Left
        for (i = 0; i < GRID_SIZE; i++)
        {
            k = 0;
            for (j = 0; j < GRID_SIZE; j++)
            {
                if (grid[i][j] != 0)
                {
                    if (k > 0 && grid[i][k - 1] == grid[i][j] && !merged[i][k - 1])
                    {
                        grid[i][k - 1] *= 2;
                        grid[i][j] = 0;
                        merged[i][k - 1] = TRUE;  // Mark as merged
                        moved = TRUE;
                    }
                    else
                    {
                        if (k != j)
                        {
                            grid[i][k] = grid[i][j];
                            grid[i][j] = 0;
                            moved = TRUE;
                        }
                        k++;
                    }
                }
            }
        }
        break;

    case 1: // Right
        for (i = 0; i < GRID_SIZE; i++)
        {
            k = GRID_SIZE - 1;
            for (j = GRID_SIZE - 1; j >= 0; j--)
            {
                if (grid[i][j] != 0)
                {
                    if (k < GRID_SIZE - 1 && grid[i][k + 1] == grid[i][j] && !merged[i][k + 1])
                    {
                        grid[i][k + 1] *= 2;
                        grid[i][j] = 0;
                        merged[i][k + 1] = TRUE;  // Mark as merged
                        moved = TRUE;
                    }
                    else
                    {
                        if (k != j)
                        {
                            grid[i][k] = grid[i][j];
                            grid[i][j] = 0;
                            moved = TRUE;
                        }
                        k--;
                    }
                }
            }
        }
        break;

    case 2: // Up
        for (j = 0; j < GRID_SIZE; j++)
        {
            k = 0;
            for (i = 0; i < GRID_SIZE; i++)
            {
                if (grid[i][j] != 0)
                {
                    if (k > 0 && grid[k - 1][j] == grid[i][j] && !merged[k - 1][j])
                    {
                        grid[k - 1][j] *= 2;
                        grid[i][j] = 0;
                        merged[k - 1][j] = TRUE;  // Mark as merged
                        moved = TRUE;
                    }
                    else
                    {
                        if (k != i)
                        {
                            grid[k][j] = grid[i][j];
                            grid[i][j] = 0;
                            moved = TRUE;
                        }
                        k++;
                    }
                }
            }
        }
        break;

    case 3: // Down
        for (j = 0; j < GRID_SIZE; j++)
        {
            k = GRID_SIZE - 1;
            for (i = GRID_SIZE - 1; i >= 0; i--)
            {
                if (grid[i][j] != 0)
                {
                    if (k < GRID_SIZE - 1 && grid[k + 1][j] == grid[i][j] && !merged[k + 1][j])
                    {
                        grid[k + 1][j] *= 2;
                        grid[i][j] = 0;
                        merged[k + 1][j] = TRUE;  // Mark as merged
                        moved = TRUE;
                    }
                    else
                    {
                        if (k != i)
                        {
                            grid[k][j] = grid[i][j];
                            grid[i][j] = 0;
                            moved = TRUE;
                        }
                        k--;
                    }
                }
            }
        }
        break;
    }

    if (moved)
        AddRandomTile();
}

BOOL GameOver(void)
{
    int i, j;

    for (i = 0; i < GRID_SIZE; i++)
    {
        for (j = 0; j < GRID_SIZE; j++)
        {
            if (grid[i][j] == 0)
                return FALSE;

            if (i < GRID_SIZE - 1 && grid[i][j] == grid[i + 1][j])
                return FALSE;

            if (j < GRID_SIZE - 1 && grid[i][j] == grid[i][j + 1])
                return FALSE;
        }
    }

    return TRUE;
}

COLORREF GetTileColor(int value)
{
    switch (value)
    {
    case 2:     return RGB(255, 255, 255);
    case 4:     return RGB(255, 255, 0);
    case 8:     return RGB(255, 0, 0);
    case 16:    return RGB(128, 0, 0);
    case 32:    return RGB(0, 255, 128);
    case 64:    return RGB(0, 0, 255);
    case 128:   return RGB(255, 0, 255);
    case 256:   return RGB(0, 255, 255);
    case 512:   return RGB(128, 0, 0);
    case 1024:  return RGB(0, 128, 0);
    case 2048:  return RGB(0, 0, 128);
    default:    return RGB(192, 192, 192);
    }
}

// Get text color based on tile value
COLORREF GetTileFontColor(int value)
{
    // Use dark text for light backgrounds, light text for dark backgrounds
    switch (value)
    {
    case 2:
    case 4:
        return RGB(0, 0, 0);  // Black text for light tiles
    default:
        return RGB(255, 255, 255);  // White text for dark tiles
    }
}

