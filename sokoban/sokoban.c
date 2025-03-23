/* Sokoban for Windows NT
   Written by Claude
   Public Domain          */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <io.h>      /* For _findfirst, _findnext */
#include <stdlib.h>  /* For qsort */
#include <search.h>  /* For _findfirst on some compilers */

/* Resource identifiers */
#define IDB_FORKLIFT 201
#define IDB_CRATE 202
#define IDB_TRUCK_EMPTY 203
#define IDB_TRUCK_FULL 204
#define IDB_WALL 205
#define IDI_SOKOBAN 101

/* Level resources */
#include "levels.h"

/* Game constants */
#define CELL_SIZE 32
#define GRID_ROWS 20
#define GRID_COLS 20

/* Bitmap handles */
HBITMAP hForkliftBitmap = NULL;
HBITMAP hCrateBitmap = NULL;
HBITMAP hTruckEmptyBitmap = NULL;
HBITMAP hTruckFullBitmap = NULL;
HBITMAP hWallBitmap = NULL;

/* Game elements */
#define EMPTY   0
#define WALL    1
#define BOX     2
#define TARGET  3
#define PLAYER  4
#define BOX_ON_TARGET 5
#define PLAYER_ON_TARGET 6

/* Colors */
#define COLOR_FLOOR    RGB(255, 255, 255)  /* White */
#define COLOR_WALL     RGB(100, 100, 100)  /* Dark gray */
#define COLOR_BOX      RGB(150, 75, 0)     /* Brown */
#define COLOR_TARGET   RGB(255, 0, 0)      /* Red */
#define COLOR_PLAYER   RGB(0, 0, 255)      /* Blue */
#define COLOR_BOX_OK   RGB(0, 128, 0)      /* Green */

/* Player position */
int playerX = 1;
int playerY = 1;

/* Level management */
char currentLevel[100] = "";
char levelFiles[100][100];  /* Array to store level file paths */
int numLevels = 0;         /* Number of levels found */
int currentLevelIndex = 0; /* Index of current level in the levelFiles array */

/* Level dimensions (will be detected from the loaded level) */
int levelWidth = 0;
int levelHeight = 0;

/* Game grid */
int grid[GRID_ROWS][GRID_COLS] = {0};

/* Function declarations */
BOOL LoadLevel(const char *filename);
void ScanLevelFiles(void);
BOOL LoadNextLevel(void);
void UpdateWindowTitle(HWND hwnd, const char *levelPath);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* String comparison function for qsort */
int CompareStrings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* Initialize the level list from embedded resources */
void ScanLevelFiles() {
    int i;
    char levelName[20];
    
    /* Reset level count */
    numLevels = NUM_LEVELS;
    
    /* Generate level names based on resource IDs */
    for (i = 0; i < numLevels; i++) {
        sprintf(levelName, "Level %d", i);
        strcpy(levelFiles[i], levelName);
    }
    
    /* Find current level index */
    {
        int i;
        for (i = 0; i < numLevels; i++) {
            if (strcmp(currentLevel, levelFiles[i]) == 0) {
                currentLevelIndex = i;
                break;
            }
        }
    }
}

/* Load the next level in sequence */
BOOL LoadNextLevel() {
    if (numLevels == 0 || currentLevelIndex >= numLevels - 1) {
        /* We're at the last level or no levels found */
        MessageBox(NULL, "Congratulations! You completed all levels!", "Sokoban", MB_OK | MB_ICONINFORMATION);
        return FALSE;
    }
    
    /* Move to the next level */
    currentLevelIndex++;
    return LoadLevel(levelFiles[currentLevelIndex]);
}

/* Load a level from embedded resource */
BOOL LoadLevel(const char *levelName)
{
    HRSRC hResInfo;
    HGLOBAL hResData;
    LPVOID pData;
    DWORD resSize;
    char *levelData, *line, *nextLine;
    int row = 0, col = 0;
    int i, j;
    int levelIndex;
    HWND hwnd;
    
    /* Save the current level name */
    strcpy(currentLevel, levelName);
    
    /* Convert level name to resource index */
    if (sscanf(levelName, "Level %d", &levelIndex) != 1) {
        /* If not in our format, find by string comparison */
        for (i = 0; i < numLevels; i++) {
            if (strcmp(levelName, levelFiles[i]) == 0) {
                levelIndex = i;
                break;
            }
        }
    }
    
    /* Verify level index is valid */
    if (levelIndex < 0 || levelIndex >= numLevels) {
        MessageBox(NULL, "Invalid level index!", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Reset the grid */
    for (i = 0; i < GRID_ROWS; i++) {
        for (j = 0; j < GRID_COLS; j++) {
            grid[i][j] = EMPTY;
        }
    }
    
    /* Load level resource by ID */
    hResInfo = FindResource(NULL, MAKEINTRESOURCE(3000 + levelIndex), RT_RCDATA);
    
    if (!hResInfo) {
        MessageBox(NULL, "Failed to find level resource!", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        MessageBox(NULL, "Failed to load level resource!", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    pData = LockResource(hResData);
    if (!pData) {
        MessageBox(NULL, "Failed to lock level resource!", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    resSize = SizeofResource(NULL, hResInfo);
    
    /* Create a null-terminated copy of the resource data */
    levelData = (char *)malloc(resSize + 1);
    if (!levelData) {
        MessageBox(NULL, "Out of memory!", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    memcpy(levelData, pData, resSize);
    levelData[resSize] = '\0';
    
    /* Level data loaded successfully */
    
    /* First pass: determine level dimensions */
    levelWidth = 0;
    levelHeight = 0;
    
    line = levelData;
    while (line && *line) {
        /* Find end of line */
        nextLine = strpbrk(line, "\r\n");
        if (nextLine) {
            char *endLine = nextLine; /* Save position for measuring line length */
            
            /* Skip all newline characters */
            while (*nextLine == '\r' || *nextLine == '\n') {
                nextLine++;
            }
            
            /* Null-terminate this line for processing */
            *endLine = '\0';
        }
        
        /* Measure line length */
        col = strlen(line);
        
        if (col > 0) {
            /* Count only lines with content */
            if (col > levelWidth) {
                levelWidth = col;
            }
            levelHeight++;
        }
        
        /* Move to next line or break if no more lines */
        if (!nextLine || !*nextLine) {
            break;
        }
        line = nextLine;
    }
    
    /* Level dimensions now determined */
    
    /* CRITICAL FIX: Re-read the data for the second pass */
    /* Reset the resource pointer and parse again */
    free(levelData);
    
    /* Create a fresh null-terminated copy of the resource data */
    levelData = (char *)malloc(resSize + 1);
    if (!levelData) {
        MessageBox(NULL, "Out of memory!", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    memcpy(levelData, pData, resSize);
    levelData[resSize] = '\0';
    
    /* Second pass: actually load the level */
    row = 0;
    line = levelData;
    
    while (line && *line && row < levelHeight) {
        nextLine = strpbrk(line, "\r\n");
        if (nextLine) {
            char *endLine = nextLine;
            
            /* Skip all newline characters */
            while (*nextLine == '\r' || *nextLine == '\n') {
                nextLine++;
            }
            
            /* Null-terminate this line for processing */
            *endLine = '\0';
        }
        
        /* Process row */
        
        col = 0;
        for (i = 0; line[i] != '\0' && col < levelWidth; i++) {
            switch (line[i]) {
                case '#': /* Wall */
                    grid[row][col] = WALL;
                    col++;
                    break;
                    
                case '@': /* Player */
                    grid[row][col] = PLAYER;
                    playerX = col;
                    playerY = row;
                    col++;
                    break;
                    
                case '+': /* Player on target */
                    grid[row][col] = PLAYER_ON_TARGET;
                    playerX = col;
                    playerY = row;
                    col++;
                    break;
                    
                case '$': /* Box */
                    grid[row][col] = BOX;
                    col++;
                    break;
                    
                case '*': /* Box on target */
                    grid[row][col] = BOX_ON_TARGET;
                    col++;
                    break;
                    
                case '.': /* Target */
                    grid[row][col] = TARGET;
                    col++;
                    break;
                    
                case ' ': /* Empty space */
                    grid[row][col] = EMPTY;
                    col++;
                    break;
                    
                default:
                    /* Skip unknown characters */
                    break;
            }
        }
        
        /* Fill in the rest of the row with empty spaces */
        while (col < levelWidth) {
            grid[row][col] = EMPTY;
            col++;
        }
        
        row++;
        
        /* Move to next line or break if no more lines */
        if (!nextLine || !*nextLine) {
            break;
        }
        line = nextLine;
    }
    
    /* Level loading complete */
    
    free(levelData);
    
    /* Resize the window to match the level dimensions plus small pixel margin */
    hwnd = FindWindow("SokobanClass", NULL);
    if (hwnd != NULL) {
        /* Window size includes borders (16 for width, 38 for height on standard Windows) 
           Plus a few pixels of margin on all sides */
        SetWindowPos(hwnd, NULL, 0, 0, 
                     CELL_SIZE * levelWidth + 40, /* extra space for margin */
                     CELL_SIZE * levelHeight + 60, /* extra space for margin */
                     SWP_NOMOVE | SWP_NOZORDER);
        
        /* Update the window title with the current level name */
        UpdateWindowTitle(hwnd, levelName);
    }
    
    return TRUE;
}

/* Draw a wall sprite */
void DrawWall(HDC hdc, int x, int y)
{
    /* First fill the cell with wall color to prevent any gaps */
    RECT rect = {x, y, x + CELL_SIZE, y + CELL_SIZE};
    HBRUSH brush = CreateSolidBrush(COLOR_WALL);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    
    /* Use the wall bitmap */
    if (hWallBitmap != NULL) {
        /* Create a compatible DC for the bitmap */
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = SelectObject(hdcMem, hWallBitmap);
        
        /* Draw the bitmap */
        BitBlt(hdc, x, y, CELL_SIZE, CELL_SIZE, hdcMem, 0, 0, SRCCOPY);
        
        /* Clean up */
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
    } else {
        /* Fallback if bitmap loading failed */
        RECT rect = {x, y, x + CELL_SIZE, y + CELL_SIZE};
        HBRUSH brush;
        HPEN pen;
        
        /* Fill with wall color */
        brush = CreateSolidBrush(COLOR_WALL);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        
        /* Draw bricks pattern */
        pen = CreatePen(PS_SOLID, 1, RGB(50, 50, 50));
        SelectObject(hdc, pen);
        
        /* Horizontal lines */
        MoveToEx(hdc, x, y + CELL_SIZE/3, NULL);
        LineTo(hdc, x + CELL_SIZE, y + CELL_SIZE/3);
        
        MoveToEx(hdc, x, y + 2*CELL_SIZE/3, NULL);
        LineTo(hdc, x + CELL_SIZE, y + 2*CELL_SIZE/3);
        
        /* Vertical lines - staggered for brick effect */
        MoveToEx(hdc, x + CELL_SIZE/2, y, NULL);
        LineTo(hdc, x + CELL_SIZE/2, y + CELL_SIZE/3);
        
        MoveToEx(hdc, x + CELL_SIZE/4, y + CELL_SIZE/3, NULL);
        LineTo(hdc, x + CELL_SIZE/4, y + 2*CELL_SIZE/3);
        
        MoveToEx(hdc, x + 3*CELL_SIZE/4, y + CELL_SIZE/3, NULL);
        LineTo(hdc, x + 3*CELL_SIZE/4, y + 2*CELL_SIZE/3);
        
        MoveToEx(hdc, x + CELL_SIZE/2, y + 2*CELL_SIZE/3, NULL);
        LineTo(hdc, x + CELL_SIZE/2, y + CELL_SIZE);
        
        DeleteObject(SelectObject(hdc, GetStockObject(BLACK_PEN)));
    }
}

/* Draw a box/crate sprite */
void DrawBox(HDC hdc, int x, int y, BOOL onTarget)
{
    /* Use the crate bitmap */
    if (hCrateBitmap != NULL) {
        /* Create a compatible DC for the bitmap */
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = SelectObject(hdcMem, hCrateBitmap);
        
        /* Draw the bitmap */
        BitBlt(hdc, x, y, CELL_SIZE, CELL_SIZE, hdcMem, 0, 0, SRCCOPY);
        
        /* Clean up */
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
    } else {
        /* Fallback if bitmap loading failed */
        RECT rect = {x, y, x + CELL_SIZE, y + CELL_SIZE};
        HBRUSH brush;
        HPEN pen;
        
        /* Fill with box color */
        brush = CreateSolidBrush(onTarget ? COLOR_BOX_OK : COLOR_BOX);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        
        /* Draw box edges and details */
        pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        SelectObject(hdc, pen);
        
        /* Box outline */
        Rectangle(hdc, x + 2, y + 2, x + CELL_SIZE - 2, y + CELL_SIZE - 2);
        
        /* Draw wooden planks */
        MoveToEx(hdc, x + 2, y + CELL_SIZE/3, NULL);
        LineTo(hdc, x + CELL_SIZE - 2, y + CELL_SIZE/3);
        
        MoveToEx(hdc, x + 2, y + 2*CELL_SIZE/3, NULL);
        LineTo(hdc, x + CELL_SIZE - 2, y + 2*CELL_SIZE/3);
        
        /* Nails/details */
        SetPixel(hdc, x + CELL_SIZE/4, y + CELL_SIZE/6, RGB(100, 100, 100));
        SetPixel(hdc, x + 3*CELL_SIZE/4, y + CELL_SIZE/6, RGB(100, 100, 100));
        SetPixel(hdc, x + CELL_SIZE/4, y + CELL_SIZE/2, RGB(100, 100, 100));
        SetPixel(hdc, x + 3*CELL_SIZE/4, y + CELL_SIZE/2, RGB(100, 100, 100));
        SetPixel(hdc, x + CELL_SIZE/4, y + 5*CELL_SIZE/6, RGB(100, 100, 100));
        SetPixel(hdc, x + 3*CELL_SIZE/4, y + 5*CELL_SIZE/6, RGB(100, 100, 100));
        
        DeleteObject(SelectObject(hdc, GetStockObject(BLACK_PEN)));
    }
}

/* Draw a target/truck loading bay sprite */
void DrawTarget(HDC hdc, int x, int y)
{
    /* Use the truck-empty bitmap */
    if (hTruckEmptyBitmap != NULL) {
        /* Create a compatible DC for the bitmap */
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = SelectObject(hdcMem, hTruckEmptyBitmap);
        
        /* Draw the bitmap */
        BitBlt(hdc, x, y, CELL_SIZE, CELL_SIZE, hdcMem, 0, 0, SRCCOPY);
        
        /* Clean up */
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
    } else {
        /* Fallback if bitmap loading failed */
        HPEN pen;
        
        /* Draw loading bay marker - X shape */
        pen = CreatePen(PS_SOLID, 2, COLOR_TARGET);
        SelectObject(hdc, pen);
        
        /* Draw X */
        MoveToEx(hdc, x + 5, y + 5, NULL);
        LineTo(hdc, x + CELL_SIZE - 5, y + CELL_SIZE - 5);
        
        MoveToEx(hdc, x + CELL_SIZE - 5, y + 5, NULL);
        LineTo(hdc, x + 5, y + CELL_SIZE - 5);
        
        /* Draw square around it */
        MoveToEx(hdc, x + 4, y + 4, NULL);
        LineTo(hdc, x + CELL_SIZE - 4, y + 4);
        LineTo(hdc, x + CELL_SIZE - 4, y + CELL_SIZE - 4);
        LineTo(hdc, x + 4, y + CELL_SIZE - 4);
        LineTo(hdc, x + 4, y + 4);
        
        DeleteObject(SelectObject(hdc, GetStockObject(BLACK_PEN)));
    }
}

/* Draw a player/forklift sprite */
void DrawPlayer(HDC hdc, int x, int y)
{
    if (hForkliftBitmap != NULL) {
        /* Create a compatible DC for the bitmap */
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = SelectObject(hdcMem, hForkliftBitmap);
        
        /* Draw the bitmap */
        BitBlt(hdc, x, y, CELL_SIZE, CELL_SIZE, hdcMem, 0, 0, SRCCOPY);
        
        /* Clean up */
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
    } else {
        /* Fallback if bitmap loading failed */
        HBRUSH brush;
        HPEN pen;
        
        /* Draw simplified forklift outline */
        pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        SelectObject(hdc, pen);
        
        brush = CreateSolidBrush(COLOR_PLAYER);
        SelectObject(hdc, brush);
        
        /* Forklift body */
        Rectangle(hdc, x + 8, y + 8, x + CELL_SIZE - 8, y + CELL_SIZE - 8);
        
        /* Forks */
        MoveToEx(hdc, x + 2, y + CELL_SIZE/3, NULL);
        LineTo(hdc, x + 8, y + CELL_SIZE/3);
        
        MoveToEx(hdc, x + 2, y + 2*CELL_SIZE/3, NULL);
        LineTo(hdc, x + 8, y + 2*CELL_SIZE/3);
        
        /* Operator compartment */
        Rectangle(hdc, x + CELL_SIZE - 12, y + 10, x + CELL_SIZE - 4, y + CELL_SIZE - 10);
        
        /* Wheels */
        Rectangle(hdc, x + 10, y + CELL_SIZE - 6, x + 16, y + CELL_SIZE);
        Rectangle(hdc, x + CELL_SIZE - 16, y + CELL_SIZE - 6, x + CELL_SIZE - 10, y + CELL_SIZE);
        
        DeleteObject(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        DeleteObject(SelectObject(hdc, GetStockObject(BLACK_PEN)));
    }
}

/* Draw the game grid */
void DrawGrid(HDC hdc)
{
    int i, j;
    RECT cellRect, clientRect;
    HBRUSH brush;
    HWND hwnd;
    HDC memDC;
    HBITMAP memBitmap, oldBitmap;
    int windowWidth, windowHeight;
    
    /* Get the window handle */
    hwnd = WindowFromDC(hdc);
    
    /* Get the client area dimensions */
    GetClientRect(hwnd, &clientRect);
    windowWidth = clientRect.right - clientRect.left;
    windowHeight = clientRect.bottom - clientRect.top;
    
    /* Create memory DC for double-buffering */
    memDC = CreateCompatibleDC(hdc);
    memBitmap = CreateCompatibleBitmap(hdc, windowWidth, windowHeight);
    oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
    
    /* Clear background with light gray */
    brush = CreateSolidBrush(RGB(220, 220, 220));
    FillRect(memDC, &clientRect, brush);
    DeleteObject(brush);
    
    /* Draw the game grid */
    for(i = 0; i < levelHeight; i++)
    {
        for(j = 0; j < levelWidth; j++)
        {
            int x = j * CELL_SIZE + 10; /* 10px margin */
            int y = i * CELL_SIZE + 10; /* 10px margin */
            
            cellRect.left = x;
            cellRect.top = y;
            cellRect.right = x + CELL_SIZE;
            cellRect.bottom = y + CELL_SIZE;
            
            /* Draw floor for all cells except walls */
            if(grid[i][j] != WALL) {
                brush = CreateSolidBrush(COLOR_FLOOR);
                FillRect(memDC, &cellRect, brush);
                DeleteObject(brush);
            }
            
            /* Draw game elements */
            switch(grid[i][j])
            {
                case WALL:
                    DrawWall(memDC, x, y);
                    break;
                    
                case BOX:
                    DrawBox(memDC, x, y, FALSE);
                    break;
                    
                case TARGET:
                    DrawTarget(memDC, x, y);
                    break;
                    
                case PLAYER:
                    DrawPlayer(memDC, x, y);
                    break;
                    
                case BOX_ON_TARGET:
                    if (hTruckFullBitmap != NULL) {
                        /* Draw loaded truck bitmap */
                        HDC bitmapDC = CreateCompatibleDC(memDC);
                        HBITMAP oldBmp = SelectObject(bitmapDC, hTruckFullBitmap);
                        BitBlt(memDC, x, y, CELL_SIZE, CELL_SIZE, bitmapDC, 0, 0, SRCCOPY);
                        SelectObject(bitmapDC, oldBmp);
                        DeleteDC(bitmapDC);
                    } else {
                        /* Fallback to old method */
                        DrawTarget(memDC, x, y);
                        DrawBox(memDC, x, y, TRUE);
                    }
                    break;
                    
                case PLAYER_ON_TARGET:
                    DrawTarget(memDC, x, y);
                    DrawPlayer(memDC, x, y);
                    break;
            }
        }
    }
    
    /* Copy the memory DC to the window DC */
    BitBlt(hdc, 0, 0, windowWidth, windowHeight, memDC, 0, 0, SRCCOPY);
    
    /* Clean up */
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

/* Move player in a direction */
void MovePlayer(HWND hwnd, int dx, int dy)
{
    int newX = playerX + dx;
    int newY = playerY + dy;
    
    /* Check if the move is valid */
    if(newX < 0 || newY < 0 || newX >= levelWidth || newY >= levelHeight)
        return;
        
    if(grid[newY][newX] == WALL)
        return;
        
    /* Moving to an empty space or target */
    if(grid[newY][newX] == EMPTY || grid[newY][newX] == TARGET)
    {
        /* Update player's current position */
        if(grid[playerY][playerX] == PLAYER_ON_TARGET)
            grid[playerY][playerX] = TARGET;
        else
            grid[playerY][playerX] = EMPTY;
            
        /* Update player's new position */
        if(grid[newY][newX] == TARGET)
            grid[newY][newX] = PLAYER_ON_TARGET;
        else
            grid[newY][newX] = PLAYER;
            
        playerX = newX;
        playerY = newY;
    }
    /* Moving a box */
    else if(grid[newY][newX] == BOX || grid[newY][newX] == BOX_ON_TARGET)
    {
        int boxNewX = newX + dx;
        int boxNewY = newY + dy;
        
        /* Check if the box can be moved */
        if(boxNewX < 0 || boxNewY < 0 || boxNewX >= levelWidth || boxNewY >= levelHeight)
            return;
            
        if(grid[boxNewY][boxNewX] == WALL || grid[boxNewY][boxNewX] == BOX || grid[boxNewY][boxNewX] == BOX_ON_TARGET)
            return;
            
        /* Move the box */
        if(grid[boxNewY][boxNewX] == TARGET)
            grid[boxNewY][boxNewX] = BOX_ON_TARGET;
        else
            grid[boxNewY][boxNewX] = BOX;
            
        /* Update player's current position */
        if(grid[playerY][playerX] == PLAYER_ON_TARGET)
            grid[playerY][playerX] = TARGET;
        else
            grid[playerY][playerX] = EMPTY;
            
        /* Update player's new position */
        if(grid[newY][newX] == BOX_ON_TARGET)
            grid[newY][newX] = PLAYER_ON_TARGET;
        else
            grid[newY][newX] = PLAYER;
            
        playerX = newX;
        playerY = newY;
    }
    
    /* Redraw the window - FALSE means don't erase background first (prevents flicker) */
    InvalidateRect(hwnd, NULL, FALSE);
}

/* Update window title with level name */
void UpdateWindowTitle(HWND hwnd, const char *levelName)
{
    char title[256];
    int levelIndex;
    
    /* Create window title with the level name */
    if (sscanf(levelName, "Level %d", &levelIndex) == 1) {
        /* If using our internal format, add 1 to make it 1-based for display */
        sprintf(title, "Sokoban - Level %d", levelIndex + 1);
    } else {
        sprintf(title, "Sokoban - %s", levelName);
    }
    
    /* Set the window title */
    SetWindowText(hwnd, title);
}

/* Check if the game is complete */
BOOL CheckWin(void)
{
    int i, j;
    
    for(i = 0; i < levelHeight; i++)
    {
        for(j = 0; j < levelWidth; j++)
        {
            /* If there's a target without a box, game is not complete */
            if(grid[i][j] == TARGET || grid[i][j] == PLAYER_ON_TARGET)
                return FALSE;
        }
    }
    
    /* All targets have boxes on them */
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        case WM_KEYDOWN:
            switch(wParam)
            {
                case VK_UP:
                    MovePlayer(hwnd, 0, -1);
                    break;
                case VK_DOWN:
                    MovePlayer(hwnd, 0, 1);
                    break;
                case VK_LEFT:
                    MovePlayer(hwnd, -1, 0);
                    break;
                case VK_RIGHT:
                    MovePlayer(hwnd, 1, 0);
                    break;
                case VK_PRIOR: /* Page Up - Next level */
                    if (currentLevelIndex < numLevels - 1) {
                        currentLevelIndex++;
                        LoadLevel(levelFiles[currentLevelIndex]);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    break;
                case VK_NEXT: /* Page Down - Previous level */
                    if (currentLevelIndex > 0) {
                        currentLevelIndex--;
                        LoadLevel(levelFiles[currentLevelIndex]);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    break;
                case 'R': /* Reset current level */
                    /* Reset the current level */
                    LoadLevel(currentLevel);
                    
                    /* Redraw the window */
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                    
                /* Level selection with number keys */
                case '1': case '2': case '3': case '4': case '5':
                case '6': case '7': case '8': case '9': case '0':
                    {
                        int levelNum = (wParam == '0') ? 9 : (wParam - '1');
                        if (levelNum >= 0 && levelNum < numLevels) {
                            LoadLevel(levelFiles[levelNum]);
                            InvalidateRect(hwnd, NULL, FALSE);
                        }
                    }
                    break;
            }
            
            /* Check if the game is complete */
            if(CheckWin()) {
                /* Load the next level instead of showing a message */
                LoadNextLevel();
                /* Redraw the window immediately */
                InvalidateRect(hwnd, NULL, FALSE);
            }
            
            break;
            
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                /* Draw the game grid with double-buffering */
                DrawGrid(hdc);
                
                EndPaint(hwnd, &ps);
            }
            break;
            
        case WM_SIZE:
            /* Handle window size changes */
            InvalidateRect(hwnd, NULL, FALSE);
            break;
            
        /* Remove scrollbar handling */
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;
    
    /* Load all bitmaps from resources */
    hForkliftBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_FORKLIFT));
    hCrateBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_CRATE));
    hTruckEmptyBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_TRUCK_EMPTY));
    hTruckFullBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_TRUCK_FULL));
    hWallBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_WALL));
    
    /* Scan for level files and sort them */
    ScanLevelFiles();
    
    /* Load the first level */
    if (numLevels > 0) {
        /* Start with the first level in alphabetical order */
        LoadLevel(levelFiles[0]);
    } else {
        /* Fallback if no levels found */
        MessageBox(NULL, "No level files found in 'levels' directory!", "Warning", MB_ICONWARNING | MB_OK);
        /* Create a dummy level in memory */
        strcpy(currentLevel, "No Levels");
        levelWidth = 5;
        levelHeight = 5;
    }
    
    /* Register the Window Class */
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW; /* Add redraw styles for better refresh */
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SOKOBAN));
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "SokobanClass";
    wc.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SOKOBAN));

    if(!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    /* Create the Window with level dimensions */
    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "SokobanClass",
        "Sokoban",  /* Initial title, will be updated after creation */
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        CELL_SIZE * levelWidth + 50, /* Window borders + margin */
        CELL_SIZE * levelHeight + 70, /* Window borders + margin */
        NULL, NULL, hInstance, NULL);

    if(hwnd == NULL)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    /* Set window title for the initial level */
    UpdateWindowTitle(hwnd, currentLevel);
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* Message Loop */
    while(GetMessage(&Msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    
    /* Clean up resources */
    if (hForkliftBitmap != NULL) {
        DeleteObject(hForkliftBitmap);
    }
    if (hCrateBitmap != NULL) {
        DeleteObject(hCrateBitmap);
    }
    if (hTruckEmptyBitmap != NULL) {
        DeleteObject(hTruckEmptyBitmap);
    }
    if (hTruckFullBitmap != NULL) {
        DeleteObject(hTruckFullBitmap);
    }
    if (hWallBitmap != NULL) {
        DeleteObject(hWallBitmap);
    }
    
    return Msg.wParam;
}