#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>  /* For rand() function */
#include <time.h>    /* For time() function */

/* Game constants */
#define GRAVITY 0.05
#define THRUST_POWER 0.10
#define ROTATION_SPEED 5.0
#define FRAME_DELAY 30
#define FUEL_MAX 200.0
#define LANDING_SAFE_SPEED 0.8
#define NUM_TERRAIN_POINTS 20

/* Game states */
#define STATE_PLAYING 0
#define STATE_LANDED 1
#define STATE_CRASHED 2

/* Game structures */
typedef struct {
    double x;
    double y;
    double velocityX;
    double velocityY;
    double angle;
    double fuel;
    int thrustOn;
    int rotatingLeft;
    int rotatingRight;
} Lander;

typedef struct {
    int width;
    int height;
    int groundLevel;
    int landingPadLeft;
    int landingPadRight;
    POINT terrainPoints[NUM_TERRAIN_POINTS];
} GameField;

/* Global variables */
HWND g_hwnd;
HDC g_memDC;
HBITMAP g_memBitmap;
HBITMAP g_oldBitmap;
Lander g_lander;
GameField g_field;
int g_gameState = STATE_PLAYING;
int g_starsInitialized = 0;  /* Flag to track if stars have been initialized */
POINT g_brightStars[100];    /* Brighter stars (will use LTGRAY_BRUSH) */
POINT g_faintStars[150];     /* Fainter stars (will use GRAY_BRUSH) */

/* Function prototypes */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitGame(void);
void InitializeTerrain(void);
void GameLoop(void);
void UpdateGameState(void);
void RenderFrame(HDC hdc);
void DrawLander(HDC hdc);
void DrawGround(HDC hdc);
void DrawStats(HDC hdc);
void DrawStars(HDC hdc);
void InitializeStars(void);
void CheckCollision(void);
void TransformPoint(POINT* point, double x, double y, double angle);

/* Windows entry point */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc;
    MSG msg;

    /* Register window class */
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);  /* Use standard application icon */
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "MoonLanderClass";
    
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    /* Create main window */
    g_hwnd = CreateWindow(
        "MoonLanderClass",
        "Moon Lander",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    
    if (g_hwnd == NULL) {
        MessageBox(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    /* Initialize game state */
    InitGame();
    
    /* Show the window */
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    
    /* Set up game timer */
    if (SetTimer(g_hwnd, 1, FRAME_DELAY, NULL) == 0) {
        MessageBox(NULL, "Failed to create timer", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    /* Main message loop */
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    /* Clean up */
    KillTimer(g_hwnd, 1);
    SelectObject(g_memDC, g_oldBitmap);
    DeleteObject(g_memBitmap);
    DeleteDC(g_memDC);

    return (int)msg.wParam;
}

/* Window procedure */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HDC hdc = GetDC(hwnd);
            g_memDC = CreateCompatibleDC(hdc);
            ReleaseDC(hwnd, hdc);
            return 0;
        }
        
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            HDC hdc;
            
            /* Update screen dimensions */
            g_field.width = width;
            g_field.height = height;
            g_field.groundLevel = height - 50;
            g_field.landingPadLeft = width / 2 - 40;
            g_field.landingPadRight = width / 2 + 40;
            
            /* Recreate backbuffer */
            if (g_memBitmap != NULL) {
                SelectObject(g_memDC, g_oldBitmap);
                DeleteObject(g_memBitmap);
            }
            
            hdc = GetDC(hwnd);
            g_memBitmap = CreateCompatibleBitmap(hdc, width, height);
            g_oldBitmap = SelectObject(g_memDC, g_memBitmap);
            ReleaseDC(hwnd, hdc);
            
            return 0;
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            /* Draw the backbuffer to the window */
            BitBlt(hdc, 0, 0, g_field.width, g_field.height, g_memDC, 0, 0, SRCCOPY);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_TIMER:
            if (wParam == 1) {
                GameLoop();
                return 0;
            }
            break;
            
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_UP:
                    g_lander.thrustOn = 1;
                    break;

                case VK_LEFT:
                    g_lander.rotatingLeft = 1;
                    break;

                case VK_RIGHT:
                    g_lander.rotatingRight = 1;
                    break;

                case 'R':
                case 'r':
                    if (g_gameState != STATE_PLAYING) {
                        InitGame();
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;

                case 'Q':
                case 'q':
                case VK_ESCAPE:
                    /* Quit the game */
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
            }
            return 0;
            
        case WM_KEYUP:
            switch (wParam) {
                case VK_UP:
                    g_lander.thrustOn = 0;
                    break;
                    
                case VK_LEFT:
                    g_lander.rotatingLeft = 0;
                    break;
                    
                case VK_RIGHT:
                    g_lander.rotatingRight = 0;
                    break;
            }
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/* Initialize stars in the sky */
void InitializeStars(void) {
    int i;
    int width = g_field.width;
    int groundLevel = g_field.groundLevel;

    /* Initialize random seed only once */
    static int seedInitialized = 0;
    if (!seedInitialized) {
        srand((unsigned int)time(NULL));
        seedInitialized = 1;
    }

    /* Initialize bright stars (top half of screen, more visible) */
    for (i = 0; i < 100; i++) {
        g_brightStars[i].x = rand() % width;
        /* Keep most bright stars in the top 70% of the sky */
        g_brightStars[i].y = rand() % (int)(groundLevel * 0.7);
    }

    /* Initialize faint stars (more evenly distributed, but still above ground) */
    for (i = 0; i < 150; i++) {
        g_faintStars[i].x = rand() % width;
        g_faintStars[i].y = rand() % groundLevel;
    }

    g_starsInitialized = 1;
}

/* Initialize terrain with random mountains and valleys */
void InitializeTerrain(void) {
    int i;
    int landingPadIndex;
    int baseGroundLevel;
    int x;
    int y;
    int prevY;
    int step;
    static int seedInitialized = 0;

    /* Initialize random seed only once */
    if (!seedInitialized) {
        srand((unsigned int)time(NULL));
        seedInitialized = 1;
    }

    /* Set base ground level */
    baseGroundLevel = g_field.height - 50;
    g_field.groundLevel = baseGroundLevel;

    /* Choose terrain points with some randomness */
    step = g_field.width / (NUM_TERRAIN_POINTS - 1);
    prevY = baseGroundLevel;

    /* The landing pad will be somewhere in the middle of the terrain */
    landingPadIndex = NUM_TERRAIN_POINTS / 2;

    /* Position first point */
    g_field.terrainPoints[0].x = 0;
    g_field.terrainPoints[0].y = baseGroundLevel - (rand() % 20);
    prevY = g_field.terrainPoints[0].y;

    /* Generate remaining points */
    for (i = 1; i < NUM_TERRAIN_POINTS; i++) {
        x = i * step;

        /* Position around landing pad should be flat */
        if (i == landingPadIndex - 1 || i == landingPadIndex || i == landingPadIndex + 1) {
            y = baseGroundLevel;
        } else {
            /* Random height changes, but not too drastic from point to point */
            y = prevY + (rand() % 40) - 20;

            /* Keep within reasonable bounds */
            if (y < baseGroundLevel - 80) {
                y = baseGroundLevel - 80;
            } else if (y > baseGroundLevel + 20) {
                y = baseGroundLevel + 20;
            }
        }

        g_field.terrainPoints[i].x = x;
        g_field.terrainPoints[i].y = y;
        prevY = y;
    }

    /* Define landing pad in the middle section */
    g_field.landingPadLeft = g_field.terrainPoints[landingPadIndex - 1].x;
    g_field.landingPadRight = g_field.terrainPoints[landingPadIndex + 1].x;
}

/* Initialize game state */
void InitGame(void) {
    RECT clientRect;

    GetClientRect(g_hwnd, &clientRect);

    /* Initialize game field */
    g_field.width = clientRect.right;
    g_field.height = clientRect.bottom;
    g_field.groundLevel = g_field.height - 50;

    /* Initialize terrain */
    InitializeTerrain();

    /* Initialize lander */
    g_lander.x = g_field.width / 4;  /* Start more to the left */
    g_lander.y = 50;
    g_lander.velocityX = 1.0;
    g_lander.velocityY = 0.0;
    g_lander.angle = 0.0;
    g_lander.fuel = FUEL_MAX;
    g_lander.thrustOn = 0;
    g_lander.rotatingLeft = 0;
    g_lander.rotatingRight = 0;

    g_gameState = STATE_PLAYING;

    /* Initialize stars if not already done */
    if (!g_starsInitialized) {
        InitializeStars();
    }
}

/* Main game loop */
void GameLoop(void) {
    if (g_gameState == STATE_PLAYING) {
        UpdateGameState();
        CheckCollision();
    }
    
    RenderFrame(g_memDC);
    
    /* Update window */
    InvalidateRect(g_hwnd, NULL, FALSE);
}

/* Update game state */
void UpdateGameState(void) {
    double radians;
    double thrustX;
    double thrustY;

    /* Update rotation */
    if (g_lander.rotatingLeft) {
        g_lander.angle -= ROTATION_SPEED;
        if (g_lander.angle < 0) {
            g_lander.angle += 360.0;
        }
    }

    if (g_lander.rotatingRight) {
        g_lander.angle += ROTATION_SPEED;
        if (g_lander.angle >= 360.0) {
            g_lander.angle -= 360.0;
        }
    }

    /* Apply thrust if engine is on and we have fuel */
    if (g_lander.thrustOn && g_lander.fuel > 0) {
        radians = g_lander.angle * 3.14159265358979323846 / 180.0;

        thrustX = THRUST_POWER * sin(radians);
        thrustY = -THRUST_POWER * cos(radians);

        g_lander.velocityX += thrustX;
        g_lander.velocityY += thrustY;

        g_lander.fuel -= 0.5;
        if (g_lander.fuel < 0) {
            g_lander.fuel = 0;
        }
    }

    /* Apply gravity */
    g_lander.velocityY += GRAVITY;

    /* Update position */
    g_lander.x += g_lander.velocityX;
    g_lander.y += g_lander.velocityY;

    /* Handle screen wrapping (left/right only) */
    if (g_lander.x < 0) {
        g_lander.x = g_field.width;
    } else if (g_lander.x > g_field.width) {
        g_lander.x = 0;
    }
}

/* Check for collisions with ground */
void CheckCollision(void) {
    int landerBottom;
    int onLandingPad;
    double horizontalSpeed;
    double verticalSpeed;
    int condOnPad;
    int condVertSpeed;
    int condHorizSpeed;
    int condAngle;
    int terrainHeight;
    int x1, x2, y1, y2;
    double t;
    int i;

    landerBottom = (int)g_lander.y + 15; /* Lander bottom (with legs) */

    /* Check if the lander is below ground at its current X position */
    /* Find terrain height at lander position by interpolating between terrain points */
    terrainHeight = 0;
    x1 = 0;
    x2 = 0;
    y1 = 0;
    y2 = 0;
    t = 0.0;

    /* Find the two terrain points that the lander is between */
    for (i = 0; i < NUM_TERRAIN_POINTS - 1; i++) {
        if (g_lander.x >= g_field.terrainPoints[i].x && g_lander.x <= g_field.terrainPoints[i+1].x) {
            x1 = g_field.terrainPoints[i].x;
            y1 = g_field.terrainPoints[i].y;
            x2 = g_field.terrainPoints[i+1].x;
            y2 = g_field.terrainPoints[i+1].y;

            /* Interpolate to find the terrain height at the lander's x position */
            if (x2 - x1 > 0) {
                t = ((double)g_lander.x - x1) / (x2 - x1);
                terrainHeight = (int)(y1 + t * (y2 - y1));
            } else {
                terrainHeight = y1;
            }

            break;
        }
    }

    /* Handle edge cases */
    if (g_lander.x < 0) {
        terrainHeight = g_field.terrainPoints[0].y;
    } else if (g_lander.x > g_field.width) {
        terrainHeight = g_field.terrainPoints[NUM_TERRAIN_POINTS-1].y;
    }

    if (landerBottom >= terrainHeight) {
        /* We've hit the ground - determine if it was a landing or crash */
        horizontalSpeed = fabs(g_lander.velocityX);
        verticalSpeed = fabs(g_lander.velocityY);
        onLandingPad = (g_lander.x >= g_field.landingPadLeft && g_lander.x <= g_field.landingPadRight);

        /* Calculate exact condition checks */
        condOnPad = onLandingPad;
        condVertSpeed = (verticalSpeed <= LANDING_SAFE_SPEED);
        condHorizSpeed = (horizontalSpeed <= LANDING_SAFE_SPEED);
        condAngle = (g_lander.angle >= 350.0 || g_lander.angle <= 10.0);

        /* Add a tolerance for floating point comparisons - use a larger value to be more lenient */
        #define EPSILON 0.01

        /* Check landing conditions */

        /* Final landing decision with added tolerance */
        if (condOnPad &&
            (verticalSpeed <= LANDING_SAFE_SPEED + EPSILON) &&
            (horizontalSpeed <= LANDING_SAFE_SPEED + EPSILON) &&
            (g_lander.angle >= 350.0 - EPSILON || g_lander.angle <= 10.0 + EPSILON)) {

            g_gameState = STATE_LANDED;
        } else {
            g_gameState = STATE_CRASHED;
        }

        /* Stop the lander */
        g_lander.y = terrainHeight - 15;
        g_lander.velocityX = 0;
        g_lander.velocityY = 0;
    }
}

/* Draw stars in the sky */
void DrawStars(HDC hdc) {
    int i;
    HBRUSH lightGrayBrush;
    HBRUSH grayBrush;

    /* Get standard system brushes */
    lightGrayBrush = GetStockObject(LTGRAY_BRUSH);
    grayBrush = GetStockObject(GRAY_BRUSH);

    /* Draw brighter stars (light gray) */
    for (i = 0; i < 100; i++) {
        RECT starRect;
        starRect.left = g_brightStars[i].x;
        starRect.top = g_brightStars[i].y;
        starRect.right = starRect.left + 1;  /* 1-pixel stars */
        starRect.bottom = starRect.top + 1;
        FillRect(hdc, &starRect, lightGrayBrush);
    }

    /* Draw fainter stars (regular gray) */
    for (i = 0; i < 150; i++) {
        RECT starRect;
        starRect.left = g_faintStars[i].x;
        starRect.top = g_faintStars[i].y;
        starRect.right = starRect.left + 1;  /* 1-pixel stars */
        starRect.bottom = starRect.top + 1;
        FillRect(hdc, &starRect, grayBrush);
    }
}

/* Render the current frame */
void RenderFrame(HDC hdc) {
    RECT clientRect;
    RECT textRect;
    HBRUSH blackBrush;
    HFONT font;
    HFONT smallFont;
    HFONT oldFont;
    char message[256];
    char crashReason[256];
    char details[128];
    double horizontalSpeed;
    double verticalSpeed;
    double vDiff;
    double hDiff;
    int onLandingPad;
    int goodAngle;

    GetClientRect(g_hwnd, &clientRect);

    /* Clear the background */
    blackBrush = GetStockObject(BLACK_BRUSH);
    FillRect(hdc, &clientRect, blackBrush);

    /* Draw stars in the background */
    DrawStars(hdc);

    /* Draw game elements */
    DrawGround(hdc);
    DrawLander(hdc);
    DrawStats(hdc);

    /* Draw game state message */
    if (g_gameState != STATE_PLAYING) {
        strcpy(crashReason, "");

        textRect.left = 100;
        textRect.top = 150;
        textRect.right = g_field.width - 100;
        textRect.bottom = 250;

        font = CreateFont(36, 0, 0, 0, FW_BOLD, 0, 0, 0,
                          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

        if (font != NULL) {
            oldFont = SelectObject(hdc, font);
            SetBkMode(hdc, TRANSPARENT);

            if (g_gameState == STATE_LANDED) {
                SetTextColor(hdc, RGB(0, 255, 0));
                strcpy(message, "SUCCESSFUL LANDING!");
            } else {
                horizontalSpeed = fabs(g_lander.velocityX);
                verticalSpeed = fabs(g_lander.velocityY);
                onLandingPad = (g_lander.x >= g_field.landingPadLeft && g_lander.x <= g_field.landingPadRight);
                goodAngle = (g_lander.angle >= 350.0 || g_lander.angle <= 10.0);

                SetTextColor(hdc, RGB(255, 0, 0));
                strcpy(message, "CRASH!");

                /* Determine crash reason */
                if (!onLandingPad) {
                    strcpy(crashReason, "Not on landing pad");
                } else if (verticalSpeed > LANDING_SAFE_SPEED && horizontalSpeed > LANDING_SAFE_SPEED) {
                    sprintf(crashReason, "Vertical speed (%.1f) and horizontal speed (%.1f) too high",
                           verticalSpeed, horizontalSpeed);
                } else if (verticalSpeed > LANDING_SAFE_SPEED) {
                    sprintf(crashReason, "Vertical speed too high (%.1f > %.1f)",
                           verticalSpeed, LANDING_SAFE_SPEED);
                } else if (horizontalSpeed > LANDING_SAFE_SPEED) {
                    sprintf(crashReason, "Horizontal speed too high (%.1f > %.1f)",
                           horizontalSpeed, LANDING_SAFE_SPEED);
                } else if (!goodAngle) {
                    /* Calculate display angle for message */
                    double msgAngle;

                    msgAngle = g_lander.angle;
                    if (msgAngle > 180.0) {
                        msgAngle -= 360.0;
                    }

                    sprintf(crashReason, "Lander not upright (angle: %+.1f)", msgAngle);
                } else {
                    /* Check for very slight misses in landing criteria */
                    strcpy(details, "");

                    if (verticalSpeed > LANDING_SAFE_SPEED * 0.9) {
                        sprintf(details, "V-Speed %.2f close to limit %.2f. ", verticalSpeed, LANDING_SAFE_SPEED);
                    }

                    if (horizontalSpeed > LANDING_SAFE_SPEED * 0.9) {
                        sprintf(details + strlen(details), "H-Speed %.2f close to limit %.2f. ",
                               horizontalSpeed, LANDING_SAFE_SPEED);
                    }

                    /* Check if angle is just outside the limit */
                    if ((g_lander.angle > 10.0 && g_lander.angle < 20.0) ||
                        (g_lander.angle < 350.0 && g_lander.angle > 340.0)) {
                        sprintf(details + strlen(details), "Angle %.1f close to limit. ", g_lander.angle);
                    }

                    if (strlen(details) > 0) {
                        sprintf(crashReason, "Borderline crash: %s", details);
                    } else {
                        /* More detailed comparison with safety limit */
                        vDiff = verticalSpeed - LANDING_SAFE_SPEED;
                        hDiff = horizontalSpeed - LANDING_SAFE_SPEED;

                        if (vDiff > 0 && vDiff < 0.01) {
                            sprintf(crashReason, "Vertical speed (%.6f) exceeds limit (%.6f) by %.8f",
                                   verticalSpeed, LANDING_SAFE_SPEED, vDiff);
                        }
                        else if (hDiff > 0 && hDiff < 0.01) {
                            sprintf(crashReason, "Horizontal speed (%.6f) exceeds limit (%.6f) by %.8f",
                                   horizontalSpeed, LANDING_SAFE_SPEED, hDiff);
                        }
                        else if (g_lander.angle > 10.0 && g_lander.angle < 10.1) {
                            sprintf(crashReason, "Angle (%.6f) slightly exceeds 10.0 limit",
                                   g_lander.angle);
                        }
                        else if (g_lander.angle < 350.0 && g_lander.angle > 349.9) {
                            sprintf(crashReason, "Angle (%.6f) slightly below 350.0 limit",
                                   g_lander.angle);
                        }
                        else {
                            sprintf(crashReason, "Landing check failed by tiny margin - V=%.6f, H=%.6f, A=%.6f",
                                   verticalSpeed, horizontalSpeed, g_lander.angle);
                        }
                    }
                }
            }

            DrawText(hdc, message, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            /* Draw crash reason if applicable */
            if (g_gameState == STATE_CRASHED && crashReason[0] != '\0') {
                textRect.top += 50;
                textRect.bottom += 50;

                smallFont = CreateFont(24, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
                if (smallFont != NULL) {
                    SelectObject(hdc, smallFont);
                    SetTextColor(hdc, RGB(255, 200, 0));
                    DrawText(hdc, crashReason, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(hdc, font);
                    DeleteObject(smallFont);
                }
            }

            /* Draw restart/quit message */
            textRect.top += 50;
            textRect.bottom += 50;
            SetTextColor(hdc, RGB(255, 255, 255));
            DrawText(hdc, "Press R to restart or Q to quit", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, oldFont);
            DeleteObject(font);
        }
    }
}

/* Draw the moon lander */
void DrawLander(HDC hdc) {
    POINT landerPoints[5];
    POINT transformedPoints[5];
    POINT leftLegPoints[2];
    POINT rightLegPoints[2];
    POINT transformedLeftLeg[2];
    POINT transformedRightLeg[2];
    POINT thrustPoints[3];
    HPEN whitePen;
    HPEN oldPen;
    HPEN thrustPen;
    int i;

    /* Base lander shape (centered at origin) */
    landerPoints[0].x = 0;
    landerPoints[0].y = -10;  /* Nose */
    landerPoints[1].x = 10;
    landerPoints[1].y = 10;   /* Bottom right */
    landerPoints[2].x = 0;
    landerPoints[2].y = 5;    /* Bottom middle */
    landerPoints[3].x = -10;
    landerPoints[3].y = 10;   /* Bottom left */
    landerPoints[4].x = 0;
    landerPoints[4].y = -10;  /* Back to nose */

    /* Landing legs */
    leftLegPoints[0].x = -9;
    leftLegPoints[0].y = 8;     /* Left leg top */
    leftLegPoints[1].x = -15;
    leftLegPoints[1].y = 15;    /* Left leg bottom */

    rightLegPoints[0].x = 9;
    rightLegPoints[0].y = 8;    /* Right leg top */
    rightLegPoints[1].x = 15;
    rightLegPoints[1].y = 15;   /* Right leg bottom */

    /* Transform lander points based on position and angle */
    for (i = 0; i < 5; i++) {
        TransformPoint(&transformedPoints[i], landerPoints[i].x, landerPoints[i].y, g_lander.angle);
        transformedPoints[i].x += (int)g_lander.x;
        transformedPoints[i].y += (int)g_lander.y;
    }

    /* Transform leg points */
    for (i = 0; i < 2; i++) {
        TransformPoint(&transformedLeftLeg[i], leftLegPoints[i].x, leftLegPoints[i].y, g_lander.angle);
        transformedLeftLeg[i].x += (int)g_lander.x;
        transformedLeftLeg[i].y += (int)g_lander.y;

        TransformPoint(&transformedRightLeg[i], rightLegPoints[i].x, rightLegPoints[i].y, g_lander.angle);
        transformedRightLeg[i].x += (int)g_lander.x;
        transformedRightLeg[i].y += (int)g_lander.y;
    }

    /* Draw lander */
    whitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    if (whitePen != NULL) {
        oldPen = SelectObject(hdc, whitePen);

        /* Draw lander body */
        Polyline(hdc, transformedPoints, 5);

        /* Draw landing legs */
        Polyline(hdc, transformedLeftLeg, 2);
        Polyline(hdc, transformedRightLeg, 2);

        /* Draw thrust if engine is on */
        if (g_lander.thrustOn && g_lander.fuel > 0) {
            thrustPen = CreatePen(PS_SOLID, 2, RGB(255, 165, 0));
            if (thrustPen != NULL) {
                SelectObject(hdc, thrustPen);

                /* Thrust points */
                TransformPoint(&thrustPoints[0], -5, 7, g_lander.angle);
                thrustPoints[0].x += (int)g_lander.x;
                thrustPoints[0].y += (int)g_lander.y;

                TransformPoint(&thrustPoints[1], 0, 20, g_lander.angle);
                thrustPoints[1].x += (int)g_lander.x;
                thrustPoints[1].y += (int)g_lander.y;

                TransformPoint(&thrustPoints[2], 5, 7, g_lander.angle);
                thrustPoints[2].x += (int)g_lander.x;
                thrustPoints[2].y += (int)g_lander.y;

                Polyline(hdc, thrustPoints, 3);

                DeleteObject(thrustPen);
            }
        }

        SelectObject(hdc, oldPen);
        DeleteObject(whitePen);
    }
}

/* Draw the ground and landing pad */
void DrawGround(HDC hdc) {
    HPEN whitePen;
    HPEN oldPen;
    HBRUSH grayBrush;
    HBRUSH darkGrayBrush;
    int i;
    int landingPadIndex;
    POINT polyPoints[NUM_TERRAIN_POINTS + 2]; /* Terrain points plus two points for bottom corners */

    /* Create brushes for ground */
    grayBrush = GetStockObject(GRAY_BRUSH);
    darkGrayBrush = GetStockObject(DKGRAY_BRUSH);

    /* Copy terrain points to create a polygon */
    for (i = 0; i < NUM_TERRAIN_POINTS; i++) {
        polyPoints[i].x = g_field.terrainPoints[i].x;
        polyPoints[i].y = g_field.terrainPoints[i].y;
    }

    /* Add bottom corners to close the polygon */
    polyPoints[NUM_TERRAIN_POINTS].x = g_field.width;
    polyPoints[NUM_TERRAIN_POINTS].y = g_field.height;
    polyPoints[NUM_TERRAIN_POINTS + 1].x = 0;
    polyPoints[NUM_TERRAIN_POINTS + 1].y = g_field.height;

    /* Fill ground area as a polygon */
    SelectObject(hdc, darkGrayBrush);
    Polygon(hdc, polyPoints, NUM_TERRAIN_POINTS + 2);

    /* Create pens for ground lines */
    whitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    if (whitePen != NULL) {
        oldPen = SelectObject(hdc, whitePen);

        /* Draw terrain outline */
        Polyline(hdc, g_field.terrainPoints, NUM_TERRAIN_POINTS);

        /* Find the landing pad segment */
        for (i = 0; i < NUM_TERRAIN_POINTS - 1; i++) {
            if (g_field.terrainPoints[i].x == g_field.landingPadLeft) {
                landingPadIndex = i;
                break;
            }
        }

        /* Landing pad markers only (no green line) */
        whitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        if (whitePen != NULL) {
            SelectObject(hdc, whitePen);

            /* Add landing pad markers */
            MoveToEx(hdc, g_field.landingPadLeft, g_field.terrainPoints[landingPadIndex].y, NULL);
            LineTo(hdc, g_field.landingPadLeft, g_field.terrainPoints[landingPadIndex].y - 5);
            MoveToEx(hdc, g_field.landingPadRight, g_field.terrainPoints[landingPadIndex].y, NULL);
            LineTo(hdc, g_field.landingPadRight, g_field.terrainPoints[landingPadIndex].y - 5);

            DeleteObject(whitePen);
        }

        SelectObject(hdc, oldPen);
        DeleteObject(whitePen);
    }
}

/* Draw game statistics */
void DrawStats(HDC hdc) {
    char statsText[256];
    char statusText[256];
    char speedIndicator[64];
    char angleIndicator[64];
    char positionIndicator[64];
    RECT textRect;
    RECT statusRect;
    RECT indicatorRect;
    HFONT font;
    HFONT oldFont;
    double verticalSpeed;
    double horizontalSpeed;
    double totalSpeed;
    double altitude;
    double displayAngle;
    int onPad;
    int goodAngle;
    COLORREF speedColor;
    COLORREF angleColor;
    COLORREF positionColor;

    /* Calculate speeds and position info */
    verticalSpeed = g_lander.velocityY;
    horizontalSpeed = g_lander.velocityX;
    totalSpeed = sqrt(verticalSpeed * verticalSpeed + horizontalSpeed * horizontalSpeed);
    altitude = g_field.groundLevel - g_lander.y;
    onPad = (g_lander.x >= g_field.landingPadLeft && g_lander.x <= g_field.landingPadRight);
    goodAngle = (g_lander.angle >= 350.0 || g_lander.angle <= 10.0);

    /* Convert angle for display using +/- format */
    if (g_lander.angle <= 180.0) {
        displayAngle = g_lander.angle;  /* 0 to 180 stays the same */
    } else {
        displayAngle = g_lander.angle - 360.0;  /* 181-359 becomes -179 to -1 */
    }

    /* Set indicator colors based on landing criteria */
    speedColor = (fabs(verticalSpeed) <= LANDING_SAFE_SPEED &&
                 fabs(horizontalSpeed) <= LANDING_SAFE_SPEED) ?
                 RGB(0, 255, 0) : RGB(255, 0, 0);  /* GREEN for good, RED for bad */

    angleColor = goodAngle ? RGB(0, 255, 0) : RGB(255, 0, 0);  /* GREEN for good, RED for bad */
    positionColor = onPad ? RGB(0, 255, 0) : RGB(255, 0, 0);  /* GREEN for good, RED for bad */

    /* Format primary stats text */
    sprintf(statsText, "Altitude: %.1f   Fuel: %.1f   [Q] Quit", altitude, g_lander.fuel);

    /* Format detailed status text with +/- angle format */
    sprintf(statusText, "V-Speed: %.1f   H-Speed: %.1f   Angle: %+.1f",
            verticalSpeed, horizontalSpeed, displayAngle);

    /* Set up text areas */
    textRect.left = 10;
    textRect.top = 10;
    textRect.right = g_field.width - 10;
    textRect.bottom = 35;

    statusRect.left = 10;
    statusRect.top = 35;
    statusRect.right = g_field.width - 10;
    statusRect.bottom = 60;

    /* Create font and draw text */
    font = CreateFont(20, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Courier New");

    if (font != NULL) {
        oldFont = SelectObject(hdc, font);
        SetBkMode(hdc, TRANSPARENT);

        /* Draw primary stats */
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawText(hdc, statsText, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        /* Draw detailed status with color indicators */
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawText(hdc, statusText, -1, &statusRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        /* Draw landing status indicators on the right side */
        if (g_gameState == STATE_PLAYING) {

            /* Speed indicator */
            sprintf(speedIndicator, "Speed: %s",
                   (fabs(verticalSpeed) <= LANDING_SAFE_SPEED &&
                    fabs(horizontalSpeed) <= LANDING_SAFE_SPEED) ? "GOOD" : "BAD!");

            /* Angle indicator */
            sprintf(angleIndicator, "Angle: %s", goodAngle ? "GOOD" : "BAD!");

            /* Position indicator */
            sprintf(positionIndicator, "Position: %s", onPad ? "GOOD" : "BAD!");

            /* We've already calculated displayAngle above, so we're good to go */

            /* Draw all indicators */
            indicatorRect.left = g_field.width - 200;
            indicatorRect.top = 10;
            indicatorRect.right = g_field.width - 10;
            indicatorRect.bottom = 30;

            SetTextColor(hdc, speedColor);
            DrawText(hdc, speedIndicator, -1, &indicatorRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            indicatorRect.top += 20;
            indicatorRect.bottom += 20;
            SetTextColor(hdc, angleColor);
            DrawText(hdc, angleIndicator, -1, &indicatorRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            indicatorRect.top += 20;
            indicatorRect.bottom += 20;
            SetTextColor(hdc, positionColor);
            DrawText(hdc, positionIndicator, -1, &indicatorRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            /* No terminal velocity indicator */
        }

        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }
}

/* Transform a point by rotating it */
void TransformPoint(POINT* point, double x, double y, double angle) {
    double radians;
    double newX;
    double newY;

    radians = angle * 3.14159265358979323846 / 180.0;

    newX = x * cos(radians) - y * sin(radians);
    newY = x * sin(radians) + y * cos(radians);

    point->x = (int)newX;
    point->y = (int)newY;
}

