/* RISCoban for Windows NT
   Written by Claude
   Public Domain          */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <io.h>      /* For _findfirst, _findnext */
#include <stdlib.h>  /* For qsort */
#include <search.h>  /* For _findfirst on some compilers */
#include "../arch.h"

/* Flag for allowed RISC processor detection */
BOOL isAllowedProcessor = FALSE;

/* Resource identifiers */
#define IDB_ROBOARM 201
#define IDB_MIPS 202
#define IDB_AXP 203
#define IDB_PPC 204
#define IDB_ARM 205
#define IDB_SOCKET 206
#define IDB_SOCKET_MIPS 207
#define IDB_SOCKET_AXP 208
#define IDB_SOCKET_PPC 209
#define IDB_SOCKET_ARM 210
#define IDI_RISCOBAN 101

/* Level resources */
#include "levels.h"

/* Game constants */
#define CELL_SIZE 32
#define GRID_ROWS 20
#define GRID_COLS 20

/* Bitmap handles */
HBITMAP hRoboArmBitmap = NULL;
HBITMAP hMipsBitmap = NULL;
HBITMAP hAxpBitmap = NULL;
HBITMAP hPpcBitmap = NULL;
HBITMAP hArmBitmap = NULL;
HBITMAP hSocketBitmap = NULL;
HBITMAP hSocketMipsBitmap = NULL;
HBITMAP hSocketAxpBitmap = NULL;
HBITMAP hSocketPpcBitmap = NULL;
HBITMAP hSocketArmBitmap = NULL;

/* Game elements */
#define EMPTY   0
#define WALL    1
#define BOX     2
#define TARGET  3
#define PLAYER  4
#define BOX_ON_TARGET 5
#define PLAYER_ON_TARGET 6

/* Box type tracking (for consistent processor types) */
#define MAX_BOXES 100
int boxTypes[MAX_BOXES][2]; /* [box_id][0] = row, [box_id][1] = col */
int boxTypeImages[MAX_BOXES]; /* Store which processor type (0=MIPS, 1=AXP, 2=PPC, 3=ARM) */
int numBoxesTracked = 0;

/* Colors */
#define COLOR_FLOOR    RGB(0, 128, 0)      /* Green (PCB color) */
#define COLOR_WALL     RGB(50, 50, 50)     /* Dark gray (PCB edge) */
#define COLOR_BOX      RGB(150, 75, 0)     /* Brown */
#define COLOR_TARGET   RGB(255, 0, 0)      /* Red */
#define COLOR_PLAYER   RGB(0, 0, 255)      /* Blue */
#define COLOR_BOX_OK   RGB(0, 128, 0)      /* Green */
#define COLOR_CIRCUIT  RGB(150, 150, 150)  /* Light gray for circuit traces */
#define COLOR_LIGHTGREEN RGB(0, 255, 0)    /* Light Green from 16-color palette */

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
int CountBoxes(void);
void CheckProcessorType(void);
void DrawBSODScreen(HDC hdc, RECT clientRect);
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
        MessageBox(NULL, "Congratulations! You completed all levels!", "RISCoban", MB_OK | MB_ICONINFORMATION);
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
    
    /* Reset box counters and track box positions */
    {
        int i, j, boxId, totalBoxes;
        static int *totalBoxesPtr = NULL;
        static int *boxCounterPtr = NULL;
        
        /* Find the addresses of the static variables in DrawBox */
        if (totalBoxesPtr == NULL) {
            /* Force recounting on next draw */
            totalBoxesPtr = (int*)0; /* Can't get actual address in C89, this is a workaround */
            boxCounterPtr = (int*)0;
        }
        
        /* Reset box tracking */
        numBoxesTracked = 0;
        
        /* First count total boxes */
        totalBoxes = 0;
        for (i = 0; i < levelHeight; i++) {
            for (j = 0; j < levelWidth; j++) {
                if (grid[i][j] == BOX || grid[i][j] == BOX_ON_TARGET) {
                    totalBoxes++;
                }
            }
        }
        
        /* Scan the grid to record all box positions */
        for (i = 0; i < levelHeight; i++) {
            for (j = 0; j < levelWidth; j++) {
                if (grid[i][j] == BOX || grid[i][j] == BOX_ON_TARGET) {
                    if (numBoxesTracked < MAX_BOXES) {
                        boxTypes[numBoxesTracked][0] = i;  /* row */
                        boxTypes[numBoxesTracked][1] = j;  /* col */
                        
                        /* Assign processor type based on position in the level */
                        if (totalBoxes == 1) {
                            boxTypeImages[numBoxesTracked] = 0; /* MIPS for single box */
                        } else if (totalBoxes == 2) {
                            boxTypeImages[numBoxesTracked] = (numBoxesTracked == 0) ? 0 : 1; /* MIPS, AXP */
                        } else if (totalBoxes == 3) {
                            /* For exactly 3 boxes, use MIPS, AXP, PPC */
                            boxTypeImages[numBoxesTracked] = numBoxesTracked;
                        } else {
                            /* For 4+ boxes, cycle through all processor types */
                            boxTypeImages[numBoxesTracked] = numBoxesTracked % 4;
                        }
                        
                        numBoxesTracked++;
                    }
                }
            }
        }
    }

    /* Resize the window to match the level dimensions plus small pixel margin */
    hwnd = FindWindow("RISCobanClass", NULL);
    if (hwnd != NULL) {
        RECT rect;

        /* Set desired client area size (with margin) */
        rect.left = 0;
        rect.top = 0;
        rect.right = CELL_SIZE * levelWidth + 20; /* extra space for margin */
        rect.bottom = CELL_SIZE * levelHeight + 20; /* extra space for margin */

        /* Adjust the rectangle to include non-client area (borders, title bar, etc.) */
        AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

        /* Set the new size using the adjusted rectangle */
        SetWindowPos(hwnd, NULL, 0, 0,
                     rect.right - rect.left,  /* Width including non-client area */
                     rect.bottom - rect.top,  /* Height including non-client area */
                     SWP_NOMOVE | SWP_NOZORDER);

        /* Update the window title with the current level name */
        UpdateWindowTitle(hwnd, levelName);
    }

    return TRUE;
}

/* "Draw" a wall (but actually don't draw anything - invisible boundary) */
void DrawWall(HDC hdc, int x, int y)
{
    /* Don't draw anything - walls are invisible boundaries */
    /* The MovePlayer function already handles collision detection */
}

/* Get count of boxes in level */
int CountBoxes(void)
{
    int i, j;
    int count = 0;

    for(i = 0; i < levelHeight; i++) {
        for(j = 0; j < levelWidth; j++) {
            if(grid[i][j] == BOX || grid[i][j] == BOX_ON_TARGET) {
                count++;
            }
        }
    }

    return count;
}

/* Find the box index based on its position */
int FindBoxIndex(int row, int col)
{
    int i;
    
    for (i = 0; i < numBoxesTracked; i++) {
        if (boxTypes[i][0] == row && boxTypes[i][1] == col) {
            return i;
        }
    }
    
    /* Box not found, should not happen */
    return 0;
}

/* Draw a box/processor chip sprite based on box number */
void DrawBox(HDC hdc, int x, int y, BOOL onTarget)
{
    /* Count boxes to determine which sprites to use */
    static int totalBoxes = 0;
    static int boxCounter = 0;
    HBITMAP boxBitmap = NULL;
    HBITMAP socketBitmap = NULL;
    int boxIndex = 0;
    int gridRow, gridCol;
    
    /* Only count boxes once when level loads */
    if (totalBoxes == 0) {
        totalBoxes = CountBoxes();
        boxCounter = 0;
    }
    
    /* Determine which grid cell this is */
    gridRow = (y - 10) / CELL_SIZE;
    gridCol = (x - 10) / CELL_SIZE;
    
    /* Find which box this is */
    boxIndex = FindBoxIndex(gridRow, gridCol);
    
    /* Select box bitmap based on type */
    if (boxIndex >= 0 && boxIndex < numBoxesTracked) {
        /* Use the pre-assigned type for this box */
        switch (boxTypeImages[boxIndex]) {
            case 0: /* MIPS */
                boxBitmap = hMipsBitmap;
                socketBitmap = hSocketMipsBitmap;
                break;
            case 1: /* AXP */
                boxBitmap = hAxpBitmap;
                socketBitmap = hSocketAxpBitmap;
                break;
            case 2: /* PPC */
                boxBitmap = hPpcBitmap;
                socketBitmap = hSocketPpcBitmap;
                break;
            case 3: /* ARM */
                boxBitmap = hArmBitmap;
                socketBitmap = hSocketArmBitmap;
                break;
            default:
                boxBitmap = hMipsBitmap; /* Fallback to MIPS */
                socketBitmap = hSocketMipsBitmap;
                break;
        }
    } else {
        /* Box index not found, fallback to MIPS */
        boxBitmap = hMipsBitmap;
        socketBitmap = hSocketMipsBitmap;
    }
    
    /* Use the appropriate processor bitmap */
    if (boxBitmap != NULL) {
        /* Create a compatible DC for the bitmap */
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = SelectObject(hdcMem, boxBitmap);

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

        /* Draw processor chip outline */
        pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        SelectObject(hdc, pen);

        /* Box outline */
        Rectangle(hdc, x + 2, y + 2, x + CELL_SIZE - 2, y + CELL_SIZE - 2);

        /* Draw chip details */
        MoveToEx(hdc, x + 2, y + CELL_SIZE/3, NULL);
        LineTo(hdc, x + CELL_SIZE - 2, y + CELL_SIZE/3);

        MoveToEx(hdc, x + 2, y + 2*CELL_SIZE/3, NULL);
        LineTo(hdc, x + CELL_SIZE - 2, y + 2*CELL_SIZE/3);

        /* Pins */
        {
            int i;
            for (i = 0; i < 4; i++) {
            MoveToEx(hdc, x + 8 + i*6, y + 2, NULL);
            LineTo(hdc, x + 8 + i*6, y);

            MoveToEx(hdc, x + 8 + i*6, y + CELL_SIZE - 2, NULL);
            LineTo(hdc, x + 8 + i*6, y + CELL_SIZE);
            }
        }

        DeleteObject(SelectObject(hdc, GetStockObject(BLACK_PEN)));
    }
}

/* Draw a target/socket sprite */
void DrawTarget(HDC hdc, int x, int y)
{
    /* Use the socket bitmap */
    if (hSocketBitmap != NULL) {
        /* Create a compatible DC for the bitmap */
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = SelectObject(hdcMem, hSocketBitmap);

        /* Draw the bitmap */
        BitBlt(hdc, x, y, CELL_SIZE, CELL_SIZE, hdcMem, 0, 0, SRCCOPY);

        /* Clean up */
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
    } else {
        /* Fallback if bitmap loading failed */
        HPEN pen;
        HBRUSH socketBrush;

        /* Create a darker area for the socket base */
        socketBrush = CreateSolidBrush(RGB(0, 90, 0)); /* Darker green */
        SelectObject(hdc, socketBrush);
        Rectangle(hdc, x + 2, y + 2, x + CELL_SIZE - 2, y + CELL_SIZE - 2);
        DeleteObject(socketBrush);

        /* Draw socket outline */
        pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200)); /* Silver color */
        SelectObject(hdc, pen);

        /* Draw socket connector pads */
        {
            int i;
            
            /* Draw golden socket pads around the perimeter */
            HBRUSH padBrush = CreateSolidBrush(RGB(230, 180, 0)); /* Gold color */
            SelectObject(hdc, padBrush);
            
            /* Left side pins */
            for (i = 0; i < 3; i++) {
                Rectangle(hdc, x + 2, y + 6 + i*8, x + 6, y + 10 + i*8);
            }
            
            /* Right side pins */
            for (i = 0; i < 3; i++) {
                Rectangle(hdc, x + CELL_SIZE - 6, y + 6 + i*8, x + CELL_SIZE - 2, y + 10 + i*8);
            }
            
            /* Top pins */
            for (i = 0; i < 2; i++) {
                Rectangle(hdc, x + 12 + i*8, y + 2, x + 16 + i*8, y + 6);
            }
            
            /* Bottom pins */
            for (i = 0; i < 2; i++) {
                Rectangle(hdc, x + 12 + i*8, y + CELL_SIZE - 6, x + 16 + i*8, y + CELL_SIZE - 2);
            }
            
            DeleteObject(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        }

        /* Draw central socket hole */
        pen = CreatePen(PS_SOLID, 1, RGB(50, 50, 50)); /* Dark gray */
        SelectObject(hdc, pen);
        
        /* Gray center area for chip */
        {
            HBRUSH centerBrush = CreateSolidBrush(RGB(80, 80, 80));
            SelectObject(hdc, centerBrush);
            Rectangle(hdc, x + 8, y + 8, x + CELL_SIZE - 8, y + CELL_SIZE - 8);
            DeleteObject(centerBrush);
        }

        DeleteObject(SelectObject(hdc, GetStockObject(BLACK_PEN)));
    }
}

/* Draw a player/roboarm sprite */
void DrawPlayer(HDC hdc, int x, int y)
{
    if (hRoboArmBitmap != NULL) {
        /* Create a compatible DC for the bitmap */
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = SelectObject(hdcMem, hRoboArmBitmap);

        /* Draw the bitmap */
        BitBlt(hdc, x, y, CELL_SIZE, CELL_SIZE, hdcMem, 0, 0, SRCCOPY);

        /* Clean up */
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
    } else {
        /* Fallback if bitmap loading failed */
        HBRUSH brush;
        HPEN pen;

        /* Draw simplified roboarm outline */
        pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        SelectObject(hdc, pen);

        brush = CreateSolidBrush(COLOR_PLAYER);
        SelectObject(hdc, brush);

        /* Robot arm base */
        Rectangle(hdc, x + 10, y + 20, x + CELL_SIZE - 10, y + CELL_SIZE - 5);

        /* Robot arm */
        MoveToEx(hdc, x + CELL_SIZE/2, y + 20, NULL);
        LineTo(hdc, x + CELL_SIZE/2, y + 5);

        /* Robot hand */
        Ellipse(hdc, x + CELL_SIZE/2 - 6, y + 5, x + CELL_SIZE/2 + 6, y + 17);

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

    /* Clear background with white first (for outer area) */
    brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &clientRect, brush);
    DeleteObject(brush);
    
    /* Create the PCB area with green background */
    {
        RECT pcbRect;
        /* Calculate PCB area (the entire area including walls) */
        pcbRect.left = 10; /* margin */
        pcbRect.top = 10; /* margin */
        pcbRect.right = 10 + levelWidth * CELL_SIZE;
        pcbRect.bottom = 10 + levelHeight * CELL_SIZE;
        
        /* Fill the PCB area with green */
        brush = CreateSolidBrush(COLOR_FLOOR);
        FillRect(memDC, &pcbRect, brush);
        DeleteObject(brush);
        
        /* Draw a light green border around the PCB */
        {
            HPEN borderPen = CreatePen(PS_SOLID, 1, COLOR_LIGHTGREEN);
            HBRUSH oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
            SelectObject(memDC, borderPen);
            Rectangle(memDC, pcbRect.left, pcbRect.top, pcbRect.right, pcbRect.bottom);
            SelectObject(memDC, oldBrush);
            DeleteObject(borderPen);
        }
    }
    
    /* No circuit traces in this version */

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

            /* No circuit pattern details on the PCB */

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
                    {
                        /* Get the box index */
                        int boxIndex = FindBoxIndex(i, j);
                        HBITMAP socketBitmap = NULL;
                        
                        /* Select the right socket-with-chip bitmap based on box type */
                        if (boxIndex >= 0 && boxIndex < numBoxesTracked) {
                            switch (boxTypeImages[boxIndex]) {
                                case 0: /* MIPS */
                                    socketBitmap = hSocketMipsBitmap;
                                    break;
                                case 1: /* AXP */
                                    socketBitmap = hSocketAxpBitmap;
                                    break;
                                case 2: /* PPC */
                                    socketBitmap = hSocketPpcBitmap;
                                    break;
                                case 3: /* ARM */
                                    socketBitmap = hSocketArmBitmap;
                                    break;
                                default:
                                    socketBitmap = hSocketMipsBitmap;
                                    break;
                            }
                        } else {
                            /* Box index not found, fallback to MIPS */
                            socketBitmap = hSocketMipsBitmap;
                        }
                        
                        /* Draw the socket-with-chip bitmap */
                        if (socketBitmap != NULL) {
                            HDC hdcMem = CreateCompatibleDC(memDC);
                            HBITMAP hOldBitmap = SelectObject(hdcMem, socketBitmap);
                            BitBlt(memDC, x, y, CELL_SIZE, CELL_SIZE, hdcMem, 0, 0, SRCCOPY);
                            SelectObject(hdcMem, hOldBitmap);
                            DeleteDC(hdcMem);
                        } else {
                            /* Fallback if bitmap loading failed */
                            DrawTarget(memDC, x, y);
                            DrawBox(memDC, x, y, TRUE);
                        }
                    }
                    break;

                case PLAYER_ON_TARGET:
                    DrawTarget(memDC, x, y);
                    DrawPlayer(memDC, x, y);
                    break;
            }
        }
    }

    /* Draw perimeter lines around accessible areas - AFTER drawing all game elements */
    {
        int i, j;
        HPEN perimeterPen = CreatePen(PS_SOLID, 1, COLOR_LIGHTGREEN);
        SelectObject(memDC, perimeterPen);
        
        /* Find outermost accessible cells and draw lines */
        
        /* Top edge */
        for (j = 0; j < levelWidth; j++) {
            if (grid[0][j] != WALL) {
                /* Found first accessible column from top */
                int startCol = j;
                
                /* Find last consecutive accessible column */
                while (j < levelWidth && grid[0][j] != WALL) {
                    j++;
                }
                
                /* Draw line from start to end */
                MoveToEx(memDC, 10 + startCol * CELL_SIZE, 10, NULL);
                LineTo(memDC, 10 + j * CELL_SIZE, 10);
            }
        }
        
        /* Bottom edge */
        for (j = 0; j < levelWidth; j++) {
            if (grid[levelHeight-1][j] != WALL) {
                /* Found first accessible column from bottom */
                int startCol = j;
                
                /* Find last consecutive accessible column */
                while (j < levelWidth && grid[levelHeight-1][j] != WALL) {
                    j++;
                }
                
                /* Draw line from start to end */
                MoveToEx(memDC, 10 + startCol * CELL_SIZE, 10 + levelHeight * CELL_SIZE, NULL);
                LineTo(memDC, 10 + j * CELL_SIZE, 10 + levelHeight * CELL_SIZE);
            }
        }
        
        /* Left edge */
        for (i = 0; i < levelHeight; i++) {
            if (grid[i][0] != WALL) {
                /* Found first accessible row from left */
                int startRow = i;
                
                /* Find last consecutive accessible row */
                while (i < levelHeight && grid[i][0] != WALL) {
                    i++;
                }
                
                /* Draw line from start to end */
                MoveToEx(memDC, 10, 10 + startRow * CELL_SIZE, NULL);
                LineTo(memDC, 10, 10 + i * CELL_SIZE);
            }
        }
        
        /* Right edge */
        for (i = 0; i < levelHeight; i++) {
            if (grid[i][levelWidth-1] != WALL) {
                /* Found first accessible row from right */
                int startRow = i;
                
                /* Find last consecutive accessible row */
                while (i < levelHeight && grid[i][levelWidth-1] != WALL) {
                    i++;
                }
                
                /* Draw line from start to end */
                MoveToEx(memDC, 10 + levelWidth * CELL_SIZE, 10 + startRow * CELL_SIZE, NULL);
                LineTo(memDC, 10 + levelWidth * CELL_SIZE, 10 + i * CELL_SIZE);
            }
        }
        
        /* Now draw internal perimeter lines where accessible meets inaccessible */
        for (i = 0; i < levelHeight; i++) {
            for (j = 0; j < levelWidth; j++) {
                if (grid[i][j] != WALL) {
                    /* Check neighboring cells */
                    /* Top neighbor */
                    if (i > 0 && grid[i-1][j] == WALL) {
                        MoveToEx(memDC, 10 + j * CELL_SIZE, 10 + i * CELL_SIZE, NULL);
                        LineTo(memDC, 10 + (j+1) * CELL_SIZE, 10 + i * CELL_SIZE);
                    }
                    
                    /* Bottom neighbor */
                    if (i < levelHeight-1 && grid[i+1][j] == WALL) {
                        MoveToEx(memDC, 10 + j * CELL_SIZE, 10 + (i+1) * CELL_SIZE, NULL);
                        LineTo(memDC, 10 + (j+1) * CELL_SIZE, 10 + (i+1) * CELL_SIZE);
                    }
                    
                    /* Left neighbor */
                    if (j > 0 && grid[i][j-1] == WALL) {
                        MoveToEx(memDC, 10 + j * CELL_SIZE, 10 + i * CELL_SIZE, NULL);
                        LineTo(memDC, 10 + j * CELL_SIZE, 10 + (i+1) * CELL_SIZE);
                    }
                    
                    /* Right neighbor */
                    if (j < levelWidth-1 && grid[i][j+1] == WALL) {
                        MoveToEx(memDC, 10 + (j+1) * CELL_SIZE, 10 + i * CELL_SIZE, NULL);
                        LineTo(memDC, 10 + (j+1) * CELL_SIZE, 10 + (i+1) * CELL_SIZE);
                    }
                }
            }
        }
        
        DeleteObject(perimeterPen);
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
    int i, oldBoxRow, oldBoxCol;

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
        int boxIndex;

        /* Check if the box can be moved */
        if(boxNewX < 0 || boxNewY < 0 || boxNewX >= levelWidth || boxNewY >= levelHeight)
            return;

        if(grid[boxNewY][boxNewX] == WALL || grid[boxNewY][boxNewX] == BOX || grid[boxNewY][boxNewX] == BOX_ON_TARGET)
            return;
            
        /* Find the box being moved and update its position in our tracking array */
        boxIndex = FindBoxIndex(newY, newX);
        oldBoxRow = boxTypes[boxIndex][0];
        oldBoxCol = boxTypes[boxIndex][1];
        
        /* Update the box position in our tracking array */
        boxTypes[boxIndex][0] = boxNewY;
        boxTypes[boxIndex][1] = boxNewX;

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
        sprintf(title, "RISCoban - Level %d", levelIndex + 1);
    } else {
        sprintf(title, "RISCoban - %s", levelName);
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

/* Check processor type using arch.h */
void CheckProcessorType(void)
{
    SYSTEM_INFO sysInfo;
    
    /* Get system information */
    GetSystemInfo(&sysInfo);
    
    /* Check if processor is an allowed RISC architecture */
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_MIPS:
        case PROCESSOR_ARCHITECTURE_ALPHA:
        case PROCESSOR_ARCHITECTURE_ALPHA64:
        case PROCESSOR_ARCHITECTURE_PPC:
        case PROCESSOR_ARCHITECTURE_ARM:
        case PROCESSOR_ARCHITECTURE_ARM64:
            isAllowedProcessor = TRUE;
            break;
        default:
            isAllowedProcessor = FALSE;
            break;
    }
}

/* Draw a blue screen of death for non-RISC processors */
void DrawBSODScreen(HDC hdc, RECT clientRect)
{
    HBRUSH blueBrush;
    HFONT hTitleFont, hFont, hOldFont;
    RECT titleRect, textRect, infoRect;
    int margin = 25;
    
    /* Create blue background brush (Windows NT BSOD color) */
    blueBrush = CreateSolidBrush(RGB(0, 0, 128));
    FillRect(hdc, &clientRect, blueBrush);
    DeleteObject(blueBrush);
    
    /* Set text color to white */
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkColor(hdc, RGB(0, 0, 128));
    
    /* Use system fixed font for authentic BSOD look */
    hTitleFont = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    
    /* Use the same system font for body text */
    hFont = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    
    /* Set up title rectangle at the top */
    titleRect = clientRect;
    titleRect.left += margin;
    titleRect.top += margin;
    titleRect.right -= margin;
    titleRect.bottom = titleRect.top + 25;
    
    /* Set up main text rectangle */
    textRect = clientRect;
    textRect.left += margin;
    textRect.top = titleRect.bottom + 15;
    textRect.right -= margin;
    textRect.bottom = textRect.top + 150;
    
    /* Set up information rectangle at the bottom */
    infoRect = clientRect;
    infoRect.left += margin;
    infoRect.top = textRect.bottom + 5;
    infoRect.right -= margin;
    infoRect.bottom -= margin;
    
    /* Draw title with larger font */
    hOldFont = SelectObject(hdc, hTitleFont);
    DrawText(hdc, "RISCOBAN - SYSTEM ERROR", -1, &titleRect, DT_LEFT);
    
    /* Draw main error message */
    SelectObject(hdc, hFont);
    DrawText(hdc, 
             "A fatal exception 0x0000007B has occurred at 0028:C0011E36 in VXD INTEL(01) +\r\n"
             "0000D4F1. The current application will be terminated.\r\n\r\n"
             "ERROR: WRONG CPU TYPE DETECTED\r\n"
             "INTEL PROCESSOR INVALID EXCEPTION 0xC0000418\r\n\r\n"
             "The system has detected that the processor is not RISC-compatible.\r\n"
             "This application requires a RISC processor such as MIPS, Alpha, ARM, or PowerPC.\r\n"
             "Intel x86 processors are not supported by this application.\r\n\r\n"
             "* Press any key to terminate the application.",
             -1, &textRect, DT_LEFT);
    
    /* Draw bottom information */
    DrawText(hdc, 
             "Technical information:\r\n\r\n"
             "*** STOP: 0x0000007B (0xC0000418,0xFFFFFFFF,0x00000000,0x00000000)\r\n"
             "*** INTEL.SYS - Address C0011E36 base at C0010000, DateStamp 3d6dd67e",
             -1, &infoRect, DT_LEFT);
    
    /* Clean up - no need to delete stock objects */
    SelectObject(hdc, hOldFont);
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
            /* If running on Intel CPU, any key exits the program */
            if (!isAllowedProcessor) {
                PostQuitMessage(0);
                break;
            }
            
            /* Normal game controls for non-Intel CPUs */
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
                /* Set a timer to load the next level after a delay */
                SetTimer(hwnd, 1, 2000, NULL); /* 2000 ms = 2 seconds */
                /* Redraw the window immediately to show completion */
                InvalidateRect(hwnd, NULL, FALSE);
            }

            break;

        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT clientRect;
                
                /* Get client area dimensions */
                GetClientRect(hwnd, &clientRect);
                
                if (!isAllowedProcessor) {
                    /* Draw BSOD for Intel processors */
                    DrawBSODScreen(hdc, clientRect);
                } else {
                    /* Draw normal game grid for RISC processors */
                    DrawGrid(hdc);
                }

                EndPaint(hwnd, &ps);
            }
            break;

        case WM_SIZE:
            /* Handle window size changes */
            InvalidateRect(hwnd, NULL, FALSE);
            break;
            
        case WM_TIMER:
            if (wParam == 1) {
                /* Timer for level completion */
                KillTimer(hwnd, 1);
                LoadNextLevel();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

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
    
    /* Check processor type */
    CheckProcessorType();

    /* Load all bitmaps from resources */
    hRoboArmBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_ROBOARM));
    hMipsBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_MIPS));
    hAxpBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_AXP));
    hPpcBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_PPC));
    hArmBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_ARM));
    hSocketBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SOCKET));
    hSocketMipsBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SOCKET_MIPS));
    hSocketAxpBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SOCKET_AXP));
    hSocketPpcBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SOCKET_PPC));
    hSocketArmBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SOCKET_ARM));

    /* Scan for level files and sort them */
    ScanLevelFiles();

    /* Load the first level */
    if (numLevels > 0) {
        /* Start with the first level in alphabetical order */
        LoadLevel(levelFiles[0]);
    } else {
        /* Fallback if no levels found */
        MessageBox(NULL, "No level files found in 'levels' directory!", "RISCoban Warning", MB_ICONWARNING | MB_OK);
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
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RISCOBAN));
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "RISCobanClass";
    wc.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RISCOBAN));

    if(!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    /* Create the Window with level dimensions */
    {
        RECT rect;
        int windowWidth, windowHeight;

        if (!isAllowedProcessor) {
            /* For BSOD, use fixed larger size (720x480 client area) */
            windowWidth = 720;
            windowHeight = 480;
        } else {
            /* Normal game window for RISC processors */
            windowWidth = CELL_SIZE * levelWidth + 20;  /* Client area width with margin */
            windowHeight = CELL_SIZE * levelHeight + 20; /* Client area height with margin */
        }

        /* Set desired client area size */
        rect.left = 0;
        rect.top = 0;
        rect.right = windowWidth;
        rect.bottom = windowHeight;

        /* Adjust the rectangle to include non-client area */
        AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

        hwnd = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            "RISCobanClass",
            "RISCoban",  /* Initial title, will be updated after creation */
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left,  /* Width including non-client area */
            rect.bottom - rect.top,  /* Height including non-client area */
            NULL, NULL, hInstance, NULL);
    }

    if(hwnd == NULL)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    /* Set window title based on processor type */
    if (!isAllowedProcessor) {
        SetWindowText(hwnd, "SYSTEM ERROR");
    } else {
        UpdateWindowTitle(hwnd, currentLevel);
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* Message Loop */
    while(GetMessage(&Msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    /* Clean up resources */
    if (hRoboArmBitmap != NULL) {
        DeleteObject(hRoboArmBitmap);
    }
    if (hMipsBitmap != NULL) {
        DeleteObject(hMipsBitmap);
    }
    if (hAxpBitmap != NULL) {
        DeleteObject(hAxpBitmap);
    }
    if (hPpcBitmap != NULL) {
        DeleteObject(hPpcBitmap);
    }
    if (hArmBitmap != NULL) {
        DeleteObject(hArmBitmap);
    }
    if (hSocketBitmap != NULL) {
        DeleteObject(hSocketBitmap);
    }
    if (hSocketMipsBitmap != NULL) {
        DeleteObject(hSocketMipsBitmap);
    }
    if (hSocketAxpBitmap != NULL) {
        DeleteObject(hSocketAxpBitmap);
    }
    if (hSocketPpcBitmap != NULL) {
        DeleteObject(hSocketPpcBitmap);
    }
    if (hSocketArmBitmap != NULL) {
        DeleteObject(hSocketArmBitmap);
    }

    return Msg.wParam;
}
