#include <windows.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "../arch.h"

// Define min and max functions if they're not already defined
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

// Constants
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define TANK_WIDTH 40
#define TANK_HEIGHT 20
#define TANK_SPEED 2.0        // Fast on roads
#define TANK_GRASS_SPEED 1.0  // Normal on grass
#define TURRET_SIZE 16
#define BARREL_LENGTH 35
#define BARREL_WIDTH 6

// Projectile properties
#define MAX_PROJECTILES 20
#define PROJECTILE_RADIUS 4
#define PROJECTILE_SPEED 6.0
#define FIRE_COOLDOWN 250  // milliseconds between shots

// Enemy tank properties
#define MAX_ENEMIES 3
#define ENEMY_FIRE_COOLDOWN 2000  // milliseconds between enemy shots
#define ENEMY_DETECTION_RANGE 500  // Distance at which enemies detect player (increased range)
#define ENEMY_MOVEMENT_SPEED 0.8   // Enemy movement speed

// Health and collision properties
#define MAX_HEALTH 3             // Maximum health points
#define EXPLOSION_DURATION 45    // Animation frames for explosion (longer for more drama)
#define COLLISION_RADIUS 20      // Collision radius for tanks
#define RESPAWN_DELAY 3000       // Delay in milliseconds before respawning
#define HIT_FLASH_DURATION 5     // How long hit flash lasts (in frames)

// Road properties
#define ROAD_WIDTH 40
#define GRID_SIZE 300
#define MAX_ROADS 100

// Camera/map scrolling
#define SCROLL_MARGIN 150 // Pixels from edge of screen before scrolling starts
#define ROAD_DENSITY 0.2  // Probability of creating a road

// Forward function declarations
void GenerateRoadsAroundPosition(double worldX, double worldY);

// Road direction enum
typedef enum {
    HORIZONTAL,
    VERTICAL,
    INTERSECTION
} RoadDirection;

// Road segment structure
typedef struct {
    int gridX;
    int gridY;
    RoadDirection direction;
    BOOL active;
} RoadSegment;

// Window procedure function declaration
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Projectile structure
typedef struct {
    double x;
    double y;
    double angle;
    double speed;
    BOOL active;
    int age;
    BOOL isEnemy;  // Flag to track if projectile is from enemy or player
} Projectile;

// Enemy tank structure
typedef struct {
    double x;
    double y;
    double tankAngle;    // in radians
    double turretAngle;  // in radians
    BOOL active;
    DWORD lastShotTime;
    BOOL muzzleFlashActive;
    int muzzleFlashTimer;
    int health;          // Health points (0 = destroyed)
    BOOL exploding;      // Is tank currently exploding?
    int explosionTimer;  // Timer for explosion animation
    BOOL hitFlashActive; // Is hit flash effect active?
    int hitFlashTimer;   // Timer for hit flash animation
} EnemyTank;

// Water and mud features have been removed

// Game state structure
typedef struct {
    // Tank position and rotation
    double tankX;
    double tankY;
    double tankAngle;    // in radians
    double turretAngle;  // in radians
    int tankHealth;      // Player tank health (0 = destroyed)
    BOOL playerExploding; // Is player tank exploding?
    int playerExplosionTimer; // Timer for player explosion animation
    BOOL playerActive;   // Is player tank active?
    BOOL playerHitFlashActive; // Is player hit flash effect active?
    int playerHitFlashTimer;   // Timer for player hit flash animation
    BOOL inputLockActive;  // Block input after death until keys are released
    
    // Camera/Map position
    double cameraX;
    double cameraY;
    
    // Mouse position
    int mouseX;
    int mouseY;
    
    // Time tracking
    DWORD lastFrameTime;
    DWORD lastShotTime;
    
    // Projectiles
    Projectile projectiles[MAX_PROJECTILES];
    int projectileCount;
    
    // Effects
    BOOL muzzleFlashActive;
    int muzzleFlashTimer;
    
    // Enemy tanks
    EnemyTank enemies[MAX_ENEMIES];
    int enemyCount;
    int enemiesDestroyed; // Count of destroyed enemies
    
    // Road system
    RoadSegment roads[MAX_ROADS];
    int roadCount;
    
    // Terrain features have been removed
} GameState;

// Global variables
GameState gameState;
char szAppName[] = "Tankz";
char szWindowClass[] = "TankzWindow";
int currentSessionKills = 0;  // Track kills for current life session
BOOL radarActive = FALSE;     // Track if radar mode is active
BOOL gamePaused = TRUE;       // Start the game paused
BOOL mouseWasPressed = FALSE; // Track mouse button state for handling clicks

// Double buffer variables
HBITMAP hBufferBitmap = NULL;
HDC hBufferDC = NULL;
int bufferWidth = 0;
int bufferHeight = 0;

// CPU usage optimization
BOOL gameNeedsRedraw = TRUE;  // Track if redraw is needed
BOOL playerHasMoved = FALSE;  // Track if player has moved
BOOL animationsActive = FALSE; // Track if any animations are active

// Function prototypes
void FireProjectile(void);
void FireEnemyProjectile(EnemyTank *enemy);
void UpdateProjectiles(void);
void CheckProjectileCollisions(void);
void DamagePlayerTank(void);
void DamageEnemyTank(EnemyTank *enemy);
void DrawExplosion(HDC hdc, double x, double y, int timer);
void RespawnPlayerTank(void);
void DrawProjectiles(HDC hdc);
void DrawTank(HDC hdc);
void DrawEnemyTanks(HDC hdc);
void DrawEnemyTank(HDC hdc, EnemyTank *enemy);
void SpawnEnemyTank(double x, double y);
int CountActiveEnemies(void);
void UpdateEnemyTanks(void);
BOOL IsTankOnRoad(void);
void DrawRoadSegment(HDC hdc, RoadSegment* road);
void DrawBackground(HDC hdc);
void DrawRadar(HDC hdc);
void UpdateGame(void);
void ScreenToWorld(int screenX, int screenY, double* worldX, double* worldY);
void WorldToScreen(double worldX, double worldY, int* screenX, int* screenY);
void AddRoad(int gridX, int gridY, RoadDirection direction);
BOOL RoadExistsAt(int gridX, int gridY);
double CalculateDistance(double x1, double y1, double x2, double y2);
void InitDoubleBuffer(HWND hwnd);
void ResizeDoubleBuffer(HWND hwnd);
void CleanupDoubleBuffer(void);
void WaitForVSync(void);
void UpdateWindowTitle(HWND hWnd);
void FindRoadPosition(double* outX, double* outY);
void DrawPauseScreen(HDC hdc);
void DrawCrosshair(HDC hdc);
HINSTANCE hInst;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   PSTR szCmdLine, int iCmdShow)
{
    HWND     hwnd;
    MSG      msg;
    WNDCLASS wndclass;
    
    hInst = hInstance;
    
    // Initialize game state
    // Default tank position (will be updated to a road position)
    gameState.tankX = SCREEN_WIDTH / 2;
    gameState.tankY = SCREEN_HEIGHT / 2;
    gameState.tankAngle = -3.14159 / 2;  // Initially pointing upward
    gameState.turretAngle = -3.14159 / 2;
    gameState.tankHealth = MAX_HEALTH;   // Start with full health
    gameState.playerExploding = FALSE;
    gameState.playerExplosionTimer = 0;
    gameState.playerActive = TRUE;
    gameState.playerHitFlashActive = FALSE;
    gameState.playerHitFlashTimer = 0;
    gameState.inputLockActive = FALSE;
    gameState.cameraX = 0;  // Start with camera at origin
    gameState.cameraY = 0;  // Start with camera at origin
    gameState.lastFrameTime = GetTickCount();
    gameState.lastShotTime = GetTickCount();
    gameState.projectileCount = 0;
    gameState.muzzleFlashActive = FALSE;
    gameState.muzzleFlashTimer = 0;
    gameState.enemyCount = 0;
    gameState.enemiesDestroyed = 0;
    
    // Initialize current session kills
    currentSessionKills = 0;
    
    // Seed random number generator
    srand((unsigned int)time(NULL));
    
    {
        int i;
        // Initialize all projectiles as inactive
        for (i = 0; i < MAX_PROJECTILES; i++) {
            gameState.projectiles[i].active = FALSE;
            gameState.projectiles[i].isEnemy = FALSE;
        }
        
        // Initialize enemy tanks as inactive
        for (i = 0; i < MAX_ENEMIES; i++) {
            gameState.enemies[i].active = FALSE;
            gameState.enemies[i].muzzleFlashActive = FALSE;
            gameState.enemies[i].health = MAX_HEALTH;
            gameState.enemies[i].exploding = FALSE;
            gameState.enemies[i].explosionTimer = 0;
            gameState.enemies[i].hitFlashActive = FALSE;
            gameState.enemies[i].hitFlashTimer = 0;
        }
        
        // Initialize road system
        gameState.roadCount = 0;
        for (i = 0; i < MAX_ROADS; i++) {
            gameState.roads[i].active = FALSE;
        }
        
        // Water and mud features have been removed
    }
    
    // Generate initial roads (will create the center intersection)
    GenerateRoadsAroundPosition(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    
    // Place player tank on a road
    FindRoadPosition(&gameState.tankX, &gameState.tankY);
    
    // Spawn initial enemy tanks - closer to player
    SpawnEnemyTank(gameState.tankX + 100, gameState.tankY + 100);
    SpawnEnemyTank(gameState.tankX - 100, gameState.tankY - 100);
    
    // Register the window class
    // CS_OWNDC provides a dedicated DC for each window which is better for double buffering
    wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = hInstance;
    wndclass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor       = LoadCursor(NULL, IDC_CROSS); // Use crosshair cursor for better aiming
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.lpszMenuName  = NULL;
    wndclass.lpszClassName = szWindowClass;
    
    if (!RegisterClass(&wndclass))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error",
                   MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // Create the window
    hwnd = CreateWindow(
        szWindowClass,           // window class name
        szAppName,               // window title
        WS_OVERLAPPEDWINDOW,     // window style
        CW_USEDEFAULT,           // initial x position
        CW_USEDEFAULT,           // initial y position
        SCREEN_WIDTH,            // initial x size
        SCREEN_HEIGHT,           // initial y size
        NULL,                    // parent window handle
        NULL,                    // window menu handle
        hInstance,               // program instance handle
        NULL);                   // creation parameters
        
    if (!hwnd)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error",
                   MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // Show and update the window
    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);
    
    // Message loop
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return msg.wParam;
}

// Fire a projectile from the player tank's turret
void FireProjectile()
{
    int i;
    int index;
    double barrelEndX, barrelEndY;
    DWORD currentTime;
    
    // Check if player is active (not exploding or dead)
    if (!gameState.playerActive || gameState.playerExploding) {
        return;
    }

    // Check if enough time has passed since the last shot (rate limiting)
    currentTime = GetTickCount();
    if (currentTime - gameState.lastShotTime < FIRE_COOLDOWN) {
        return;
    }
    
    // Find an inactive projectile slot
    index = -1;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (!gameState.projectiles[i].active) {
            index = i;
            break;
        }
    }
    
    // If we found a slot, create a new projectile
    if (index != -1) {
        // Calculate the position at the end of the barrel
        barrelEndX = gameState.tankX + cos(gameState.turretAngle) * BARREL_LENGTH;
        barrelEndY = gameState.tankY + sin(gameState.turretAngle) * BARREL_LENGTH;
        
        // Initialize the projectile
        gameState.projectiles[index].x = barrelEndX;
        gameState.projectiles[index].y = barrelEndY;
        gameState.projectiles[index].angle = gameState.turretAngle;
        gameState.projectiles[index].speed = PROJECTILE_SPEED;
        gameState.projectiles[index].active = TRUE;
        gameState.projectiles[index].age = 0;
        gameState.projectiles[index].isEnemy = FALSE;
        
        // Increment active projectile count
        gameState.projectileCount++;
        
        // Update last shot time
        gameState.lastShotTime = currentTime;
        
        // Activate muzzle flash effect
        gameState.muzzleFlashActive = TRUE;
        gameState.muzzleFlashTimer = 5; // Flash will last for 5 frames
    }
}

// Fire a projectile from an enemy tank
void FireEnemyProjectile(EnemyTank *enemy)
{
    int i;
    int index;
    double barrelEndX, barrelEndY;
    DWORD currentTime;
    
    // Check if enough time has passed since the last shot (rate limiting)
    currentTime = GetTickCount();
    if (currentTime - enemy->lastShotTime < ENEMY_FIRE_COOLDOWN) {
        return;
    }
    
    // Find an inactive projectile slot
    index = -1;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (!gameState.projectiles[i].active) {
            index = i;
            break;
        }
    }
    
    // If we found a slot, create a new projectile
    if (index != -1) {
        // Calculate the position at the end of the barrel
        barrelEndX = enemy->x + cos(enemy->turretAngle) * BARREL_LENGTH;
        barrelEndY = enemy->y + sin(enemy->turretAngle) * BARREL_LENGTH;
        
        // Initialize the projectile
        gameState.projectiles[index].x = barrelEndX;
        gameState.projectiles[index].y = barrelEndY;
        gameState.projectiles[index].angle = enemy->turretAngle;
        gameState.projectiles[index].speed = PROJECTILE_SPEED;
        gameState.projectiles[index].active = TRUE;
        gameState.projectiles[index].age = 0;
        gameState.projectiles[index].isEnemy = TRUE;
        
        // Increment active projectile count
        gameState.projectileCount++;
        
        // Update last shot time
        enemy->lastShotTime = currentTime;
        
        // Activate muzzle flash effect
        enemy->muzzleFlashActive = TRUE;
        enemy->muzzleFlashTimer = 5; // Flash will last for 5 frames
    }
}

// Helper function to calculate distance between two points
double CalculateDistance(double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

// Find a position on a road for tank spawning
void FindRoadPosition(double* outX, double* outY)
{
    int i, count = 0;
    int randomIndex, selectedRoadIndex = -1;
    RoadSegment* road;
    
    // Count active roads
    for (i = 0; i < MAX_ROADS; i++) {
        if (gameState.roads[i].active) {
            count++;
        }
    }
    
    // Generate initial roads if none exist
    if (count == 0) {
        GenerateRoadsAroundPosition(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        
        // Count roads again
        count = 0;
        for (i = 0; i < MAX_ROADS; i++) {
            if (gameState.roads[i].active) {
                count++;
            }
        }
    }
    
    // Select a random active road
    if (count > 0) {
        randomIndex = rand() % count;
        count = 0;
        
        for (i = 0; i < MAX_ROADS; i++) {
            if (gameState.roads[i].active) {
                if (count == randomIndex) {
                    selectedRoadIndex = i;
                    break;
                }
                count++;
            }
        }
    } else {
        // Fallback if still no roads (should never happen)
        *outX = SCREEN_WIDTH / 2;
        *outY = SCREEN_HEIGHT / 2;
        return;
    }
    
    // Get the selected road
    road = &gameState.roads[selectedRoadIndex];
    
    // Calculate world position at center of the road
    *outX = (road->gridX * GRID_SIZE) + (GRID_SIZE / 2);
    *outY = (road->gridY * GRID_SIZE) + (GRID_SIZE / 2);
    
    // For horizontal roads, adjust Y to be in the middle of the road
    if (road->direction == HORIZONTAL) {
        // No adjustment needed, already at center of grid cell which is center of road
    }
    // For vertical roads, adjust X to be in the middle of the road
    else if (road->direction == VERTICAL) {
        // No adjustment needed, already at center of grid cell which is center of road
    }
    // For intersections, position is already at center of grid cell
}

// Spawn an enemy tank at the specified position
void SpawnEnemyTank(double x, double y)
{
    int i;
    
    // Find an inactive enemy slot
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!gameState.enemies[i].active && !gameState.enemies[i].exploding) {
            // Initialize the enemy tank
            gameState.enemies[i].x = x;
            gameState.enemies[i].y = y;
            gameState.enemies[i].tankAngle = ((double)rand() / RAND_MAX) * 2 * 3.14159; // Random angle
            gameState.enemies[i].turretAngle = gameState.enemies[i].tankAngle;
            gameState.enemies[i].active = TRUE;
            gameState.enemies[i].lastShotTime = GetTickCount();
            gameState.enemies[i].muzzleFlashActive = FALSE;
            gameState.enemies[i].health = MAX_HEALTH;
            gameState.enemies[i].exploding = FALSE;
            gameState.enemies[i].explosionTimer = 0;
            gameState.enemies[i].hitFlashActive = FALSE;
            gameState.enemies[i].hitFlashTimer = 0;
            
            // Increment active enemy count
            gameState.enemyCount++;
            return;
        }
    }
}

// Update all active projectiles
void UpdateProjectiles()
{
    int i;
    int screenMargin;
    Projectile* p;
    int projectileScreenX, projectileScreenY;
    
    screenMargin = 100; // Extra margin beyond screen bounds
    
    // Move each projectile
    for (i = 0; i < MAX_PROJECTILES; i++) {
        p = &gameState.projectiles[i];
        
        if (!p->active) {
            continue;
        }
        
        // Move the projectile
        p->x += cos(p->angle) * p->speed;
        p->y += sin(p->angle) * p->speed;
        p->age++;
        
        // Convert to screen coordinates
        projectileScreenX = (int)(p->x - gameState.cameraX);
        projectileScreenY = (int)(p->y - gameState.cameraY);
        
        // Check if projectile is off-screen with margin
        if (projectileScreenX < -screenMargin || projectileScreenX > SCREEN_WIDTH + screenMargin ||
            projectileScreenY < -screenMargin || projectileScreenY > SCREEN_HEIGHT + screenMargin) {
            p->active = FALSE;
            gameState.projectileCount--;
        }
    }
    
    // Check for collisions
    CheckProjectileCollisions();
}

// Check for collisions between projectiles and tanks
void CheckProjectileCollisions()
{
    int i, j;
    Projectile* p;
    EnemyTank* enemy;
    double distance;
    
    for (i = 0; i < MAX_PROJECTILES; i++) {
        p = &gameState.projectiles[i];
        
        if (!p->active) {
            continue;
        }
        
        // Check if player projectile hits enemy tank
        if (!p->isEnemy) {
            for (j = 0; j < MAX_ENEMIES; j++) {
                enemy = &gameState.enemies[j];
                
                if (!enemy->active || enemy->exploding) {
                    continue;
                }
                
                // Calculate distance between projectile and enemy tank
                distance = CalculateDistance(p->x, p->y, enemy->x, enemy->y);
                
                // Check for collision
                if (distance < COLLISION_RADIUS) {
                    // Hit detected!
                    p->active = FALSE;
                    gameState.projectileCount--;
                    
                    // Damage enemy tank
                    DamageEnemyTank(enemy);
                    break;
                }
            }
        }
        // Check if enemy projectile hits player tank
        else if (gameState.playerActive && !gameState.playerExploding) {
            distance = CalculateDistance(p->x, p->y, gameState.tankX, gameState.tankY);
            
            // Check for collision
            if (distance < COLLISION_RADIUS) {
                // Hit detected!
                p->active = FALSE;
                gameState.projectileCount--;
                
                // Damage player tank
                DamagePlayerTank();
            }
        }
    }
}

// Handle damage to player tank
void DamagePlayerTank()
{
    // Reduce health
    gameState.tankHealth--;
    
    // Activate hit flash effect
    gameState.playerHitFlashActive = TRUE;
    gameState.playerHitFlashTimer = HIT_FLASH_DURATION;
    
    // Check if tank is destroyed
    if (gameState.tankHealth <= 0) {
        // Trigger explosion effect
        gameState.playerExploding = TRUE;
        gameState.playerExplosionTimer = EXPLOSION_DURATION;
        gameState.playerActive = FALSE;
        // No need for hit flash if exploding
        gameState.playerHitFlashActive = FALSE;
        
        // Add an input lock flag to prevent immediate restart from held keys
        gameState.inputLockActive = TRUE; // Block input until all keys are released
    }
}

// Handle damage to enemy tank
void DamageEnemyTank(EnemyTank *enemy)
{
    // Reduce health
    enemy->health--;
    
    // Activate hit flash effect
    enemy->hitFlashActive = TRUE;
    enemy->hitFlashTimer = HIT_FLASH_DURATION;
    
    // Check if tank is destroyed
    if (enemy->health <= 0) {
        // Trigger explosion effect - same as player explosion
        enemy->exploding = TRUE;
        enemy->explosionTimer = EXPLOSION_DURATION;
        // Keep active state TRUE during explosion so enemy doesn't disappear immediately
        // We'll set active to FALSE when explosion is complete
        
        // No need for hit flash if exploding
        enemy->hitFlashActive = FALSE;
        
        // Increment destroyed count
        gameState.enemiesDestroyed++;
        
        // Increment current session kill count
        currentSessionKills++;
        
        // Update window title with new kill count
        UpdateWindowTitle(FindWindow(szWindowClass, NULL));
        
        // Play dramatic explosion sound effect (if we had sound)
    }
}

// Handle player tank respawn
void RespawnPlayerTank()
{
    // Reset player tank
    gameState.tankHealth = MAX_HEALTH;
    gameState.playerExploding = FALSE;
    gameState.playerActive = TRUE;
    gameState.playerHitFlashActive = FALSE;
    gameState.playerHitFlashTimer = 0;
    
    // Place player tank on a road
    FindRoadPosition(&gameState.tankX, &gameState.tankY);
    
    // Don't reset kill count - keep the existing score
    // currentSessionKills = 0; // Removed to keep score across deaths
    
    // Turn off radar if it was active
    radarActive = FALSE;
    
    // Immediately start gameplay - don't show pause screen
    gamePaused = FALSE;
    
    // Reset mouse state to prevent issues
    mouseWasPressed = FALSE;
    
    // Update window title with current kill count
    UpdateWindowTitle(FindWindow(szWindowClass, NULL));
}

// Draw all active projectiles
void DrawProjectiles(HDC hdc)
{
    int i;
    HBRUSH projectileBrush;
    HPEN noPen;
    Projectile* p;
    int projectileScreenX, projectileScreenY;
    
    projectileBrush = CreateSolidBrush(RGB(0, 0, 0)); /* Pure black for bullets */
    noPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    
    SelectObject(hdc, projectileBrush);
    SelectObject(hdc, noPen);
    
    for (i = 0; i < MAX_PROJECTILES; i++) {
        p = &gameState.projectiles[i];
        
        if (!p->active) {
            continue;
        }
        
        // Convert to screen coordinates
        projectileScreenX = (int)(p->x - gameState.cameraX);
        projectileScreenY = (int)(p->y - gameState.cameraY);
        
        // Draw projectile as a circle
        Ellipse(hdc, 
            projectileScreenX - PROJECTILE_RADIUS, 
            projectileScreenY - PROJECTILE_RADIUS,
            projectileScreenX + PROJECTILE_RADIUS, 
            projectileScreenY + PROJECTILE_RADIUS);
    }
    
    // Clean up resources
    DeleteObject(projectileBrush);
    DeleteObject(noPen);
}

// Draw an explosion at the specified location
void DrawExplosion(HDC hdc, double x, double y, int timer)
{
    int screenX, screenY;
    HBRUSH explosionBrush;
    HPEN noPen;
    int explosionSize;
    COLORREF explosionColor;
    
    // Convert world position to screen coordinates
    screenX = (int)(x - gameState.cameraX);
    screenY = (int)(y - gameState.cameraY);
    
    // Skip if explosion is off-screen
    if (screenX < -100 || screenX > SCREEN_WIDTH + 100 ||
        screenY < -100 || screenY > SCREEN_HEIGHT + 100) {
        return;
    }
    
    noPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    SelectObject(hdc, noPen);
    
    // Simplified explosion colors for 16-color palette
    if (timer > EXPLOSION_DURATION * 2/3) {
        // Initial bright yellow phase
        explosionColor = RGB(255, 255, 0); // Yellow
    } else if (timer > EXPLOSION_DURATION * 1/3) {
        // Transition to red phase
        explosionColor = RGB(255, 0, 0); // Red
    } else {
        // Final burning phase - using orange-red color instead of gray to maintain the burning look
        explosionColor = RGB(255, 100, 0); // Bright orange-red
    }
    
    // Explosion size: start small, grow quickly, then shrink
    // Larger, more dramatic explosion
    if (timer > EXPLOSION_DURATION * 2/3) {
        // Initial rapid expansion
        explosionSize = (EXPLOSION_DURATION - timer) * 3;  // Faster expansion
    } else if (timer > EXPLOSION_DURATION * 1/3) {
        // Maximum size - bigger explosion
        explosionSize = 30 + (timer - EXPLOSION_DURATION * 1/3) / 2;
    } else {
        // Shrinking smoke cloud - lasts longer
        explosionSize = 25 + timer / 2;
    }
    
    // Draw explosion
    explosionBrush = CreateSolidBrush(explosionColor);
    SelectObject(hdc, explosionBrush);
    
    Ellipse(hdc, 
           screenX - explosionSize, 
           screenY - explosionSize,
           screenX + explosionSize, 
           screenY + explosionSize);
    
    // Draw additional explosion circles for a more dramatic effect
    if (timer > EXPLOSION_DURATION * 1/3) {
        int offsetX, offsetY;
        COLORREF secondaryColor;
        
        // First additional explosion circle
        offsetX = (timer % 7) - 3;
        offsetY = ((timer / 2) % 7) - 3;
        
        // Simpler color for secondary explosion (16-color palette)
        if (timer > EXPLOSION_DURATION * 2/3) {
            secondaryColor = RGB(255, 255, 0); // Yellow (standard VGA color)
        } else {
            secondaryColor = RGB(255, 165, 0);  // Orange (standard VGA color)
        }
        
        explosionBrush = CreateSolidBrush(secondaryColor);
        SelectObject(hdc, explosionBrush);
        
        Ellipse(hdc, 
               screenX - explosionSize/2 + offsetX, 
               screenY - explosionSize/2 + offsetY,
               screenX + explosionSize/2 + offsetX, 
               screenY + explosionSize/2 + offsetY);
        
        DeleteObject(explosionBrush);
        
        // Second additional explosion circle - only in early phase
        if (timer > EXPLOSION_DURATION * 1/2) {
            offsetX = -((timer % 7) - 3);
            offsetY = -((timer / 3) % 7) - 3;
            
            explosionBrush = CreateSolidBrush(RGB(255, 255, 0)); // Yellow (standard VGA color)
            SelectObject(hdc, explosionBrush);
            
            Ellipse(hdc, 
                   screenX - explosionSize/3 + offsetX, 
                   screenY - explosionSize/3 + offsetY,
                   screenX + explosionSize/3 + offsetX, 
                   screenY + explosionSize/3 + offsetY);
                   
            DeleteObject(explosionBrush);
        }
    }
    
    // Clean up resources
    DeleteObject(explosionBrush);
    DeleteObject(noPen);
}

// Draw the tank on the screen
void DrawTank(HDC hdc)
{
    HBRUSH tankBrush;
    HPEN tankPen;
    HBRUSH turretBrush;
    COLORREF barrelColor;
    HPEN barrelPen;
    double barrelEndX, barrelEndY;
    int oldGraphicsMode;
    int tankScreenX, tankScreenY;
    int barrelScreenEndX, barrelScreenEndY;
    XFORM originalTransform;
    XFORM rotationTransform;
    COLORREF flashColor;
    HBRUSH flashBrush;
    HPEN noPen;
    
    // If tank is exploding, draw explosion and return
    if (gameState.playerExploding) {
        DrawExplosion(hdc, gameState.tankX, gameState.tankY, gameState.playerExplosionTimer);
        return;
    }
    
    // If tank is not active, don't draw it
    if (!gameState.playerActive) {
        return;
    }
    
    // Draw tank body - yellow for player tank, white if hit flash is active
    if (gameState.playerHitFlashActive) {
        // White flash when hit
        tankBrush = CreateSolidBrush(RGB(255, 255, 255));  /* White flash when hit */
    } else {
        tankBrush = CreateSolidBrush(RGB(128, 128, 0));  /* Dark yellow (olive) in 16-color palette */
    }
    tankPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0)); /* Black outline */
    
    SelectObject(hdc, tankBrush);
    SelectObject(hdc, tankPen);
    
    // Save the current state of the device context
    oldGraphicsMode = SetGraphicsMode(hdc, GM_ADVANCED);
    GetWorldTransform(hdc, &originalTransform);
    
    // Calculate screen coordinates for tank
    tankScreenX = (int)(gameState.tankX - gameState.cameraX);
    tankScreenY = (int)(gameState.tankY - gameState.cameraY);
    
    // Set up rotation transformation
    rotationTransform.eM11 = (float)cos(gameState.tankAngle);
    rotationTransform.eM12 = (float)sin(gameState.tankAngle);
    rotationTransform.eM21 = (float)-sin(gameState.tankAngle);
    rotationTransform.eM22 = (float)cos(gameState.tankAngle);
    rotationTransform.eDx = (float)tankScreenX;
    rotationTransform.eDy = (float)tankScreenY;
    
    SetWorldTransform(hdc, &rotationTransform);
    
    // Draw the rectangle centered at the origin (since we've translated to tank position)
    Rectangle(hdc, 
              -TANK_WIDTH/2, 
              -TANK_HEIGHT/2,
              TANK_WIDTH/2, 
              TANK_HEIGHT/2);
              
    // Restore original transformation
    SetWorldTransform(hdc, &originalTransform);
    SetGraphicsMode(hdc, oldGraphicsMode);
    
    // Draw turret (a circle for now)
    if (gameState.playerHitFlashActive) {
        // White flash when hit
        turretBrush = CreateSolidBrush(RGB(255, 255, 255));  /* White flash when hit */
    } else {
        turretBrush = CreateSolidBrush(RGB(255, 255, 0));  /* Bright yellow for player turret */
    }
    SelectObject(hdc, turretBrush);
    
    Ellipse(hdc, 
            tankScreenX - TURRET_SIZE/2, 
            tankScreenY - TURRET_SIZE/2,
            tankScreenX + TURRET_SIZE/2, 
            tankScreenY + TURRET_SIZE/2);
    
    // Draw barrel
    // Calculate the endpoint of the barrel
    barrelEndX = gameState.tankX + cos(gameState.turretAngle) * BARREL_LENGTH;
    barrelEndY = gameState.tankY + sin(gameState.turretAngle) * BARREL_LENGTH;
    
    // Convert barrel endpoint to screen coordinates
    barrelScreenEndX = (int)(barrelEndX - gameState.cameraX);
    barrelScreenEndY = (int)(barrelEndY - gameState.cameraY);
    
    // Set barrel color, white if hit flash is active
    if (gameState.playerHitFlashActive) {
        barrelColor = RGB(255, 255, 255); /* White flash when hit */
    } else {
        barrelColor = RGB(255, 255, 0); /* Bright yellow for player barrel */
    }
    
    // Draw a line for the barrel
    barrelPen = CreatePen(PS_SOLID, BARREL_WIDTH, barrelColor);
    SelectObject(hdc, barrelPen);
    
    MoveToEx(hdc, tankScreenX, tankScreenY, NULL);
    LineTo(hdc, barrelScreenEndX, barrelScreenEndY);
    
    // Draw a tiny red circle at the end of the barrel if firing
    if (gameState.muzzleFlashActive) {
        // Standard VGA bright red for muzzle flash
        flashColor = RGB(255, 0, 0);
        
        flashBrush = CreateSolidBrush(flashColor);
        noPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0)); // No outline for clean circle
        
        SelectObject(hdc, flashBrush);
        SelectObject(hdc, noPen);
        
        // Draw a tiny circle (4 pixel diameter) at the end of the barrel
        Ellipse(hdc, 
               barrelScreenEndX - 2, 
               barrelScreenEndY - 2,
               barrelScreenEndX + 2, 
               barrelScreenEndY + 2);
               
        DeleteObject(flashBrush);
        DeleteObject(noPen);
    }
    
    // Health bars removed as requested
    
    // Clean up resources
    DeleteObject(tankBrush);
    DeleteObject(tankPen);
    DeleteObject(turretBrush);
    DeleteObject(barrelPen);
}

// Road-related helper functions

// Helper function to get a key for a grid position
char* GetGridKey(int gridX, int gridY, char* buffer)
{
    sprintf(buffer, "%d,%d", gridX, gridY);
    return buffer;
}

// Helper function to check if a road exists at a grid position
BOOL RoadExistsAt(int gridX, int gridY)
{
    int i;
    for (i = 0; i < MAX_ROADS; i++) {
        if (gameState.roads[i].active && 
            gameState.roads[i].gridX == gridX && 
            gameState.roads[i].gridY == gridY) {
            return TRUE;
        }
    }
    return FALSE;
}

// Add a new road at the specified grid position
void AddRoad(int gridX, int gridY, RoadDirection direction)
{
    int i;
    
    // Find an available slot
    for (i = 0; i < MAX_ROADS; i++) {
        if (!gameState.roads[i].active) {
            gameState.roads[i].gridX = gridX;
            gameState.roads[i].gridY = gridY;
            gameState.roads[i].direction = direction;
            gameState.roads[i].active = TRUE;
            gameState.roadCount++;
            return;
        }
    }
}

// Water and mud features have been removed

// Generate roads around the player's position
void GenerateRoadsAroundPosition(double worldX, double worldY)
{
    int startGridX, startGridY, endGridX, endGridY;
    int gridX, gridY;
    BOOL hasNorth, hasSouth, hasEast, hasWest;
    BOOL shouldCreateRoad;
    
    // Determine the grid cells that are visible on screen
    startGridX = (int)floor((worldX - SCREEN_WIDTH/2) / GRID_SIZE) - 1;
    startGridY = (int)floor((worldY - SCREEN_HEIGHT/2) / GRID_SIZE) - 1;
    endGridX = (int)ceil((worldX + SCREEN_WIDTH/2) / GRID_SIZE) + 1;
    endGridY = (int)ceil((worldY + SCREEN_HEIGHT/2) / GRID_SIZE) + 1;
    
    // Ensure there is at least one road segment visible at startup
    if (gameState.roadCount == 0) {
        // Create a simple crossroads at the center
        AddRoad(0, 0, INTERSECTION);
        AddRoad(-1, 0, HORIZONTAL);
        AddRoad(1, 0, HORIZONTAL);
        AddRoad(0, -1, VERTICAL);
        AddRoad(0, 1, VERTICAL);
    }
    
    // Generate road segments for each grid cell in the visible area
    for (gridY = startGridY; gridY <= endGridY; gridY++) {
        for (gridX = startGridX; gridX <= endGridX; gridX++) {
            // Skip if this cell already has a road segment
            if (RoadExistsAt(gridX, gridY)) {
                continue;
            }
            
            // Check adjacent cells for roads to potentially connect to
            hasNorth = RoadExistsAt(gridX, gridY-1);
            hasSouth = RoadExistsAt(gridX, gridY+1);
            hasEast = RoadExistsAt(gridX+1, gridY);
            hasWest = RoadExistsAt(gridX-1, gridY);
            
            // Determine if we should create a road here based on adjacent roads
            // and random chance
            shouldCreateRoad = FALSE;
            
            // If we have connecting roads, more likely to create a road
            if (hasNorth || hasSouth || hasEast || hasWest) {
                shouldCreateRoad = ((float)rand() / RAND_MAX) < ROAD_DENSITY * 1.5;
            } else {
                // Much lower chance for isolated roads
                shouldCreateRoad = ((float)rand() / RAND_MAX) < ROAD_DENSITY * 0.15;
            }
            
            if (shouldCreateRoad) {
                RoadDirection direction;
                
                // Determine the direction based on adjacent roads
                if ((hasNorth || hasSouth) && (hasEast || hasWest)) {
                    direction = INTERSECTION;
                } else if (hasNorth || hasSouth) {
                    // If connecting to a north or south road, continue vertically
                    // but also have a chance to create an intersection
                    if (((float)rand() / RAND_MAX) < 0.25) {
                        direction = INTERSECTION;
                    } else {
                        direction = VERTICAL;
                    }
                } else if (hasEast || hasWest) {
                    // If connecting to an east or west road, continue horizontally
                    // but also have a chance to create an intersection
                    if (((float)rand() / RAND_MAX) < 0.25) {
                        direction = INTERSECTION;
                    } else {
                        direction = HORIZONTAL;
                    }
                } else {
                    // Random direction for isolated roads
                    if (((float)rand() / RAND_MAX) < 0.5) {
                        direction = HORIZONTAL;
                    } else {
                        direction = VERTICAL;
                    }
                }
                
                // Create the road segment
                AddRoad(gridX, gridY, direction);
            }
        }
    }
}

// Check if the tank is on a road
BOOL IsTankOnRoad()
{
    int gridX, gridY, i;
    RoadSegment* road;
    double cellX, cellY;
    
    // World coordinates of tank
    double worldX = gameState.tankX;
    double worldY = gameState.tankY;
    
    // Convert world position to grid coordinates
    gridX = (int)floor(worldX / GRID_SIZE);
    gridY = (int)floor(worldY / GRID_SIZE);
    
    // Find road in this grid cell
    for (i = 0; i < MAX_ROADS; i++) {
        road = &gameState.roads[i];
        
        if (!road->active || road->gridX != gridX || road->gridY != gridY) {
            continue;
        }
        
        // Calculate position within the grid cell
        cellX = worldX - (gridX * GRID_SIZE);
        cellY = worldY - (gridY * GRID_SIZE);
        
        // Check if tank is on the road based on road direction
        switch (road->direction) {
            case HORIZONTAL: {
                double roadY = GRID_SIZE/2 - ROAD_WIDTH/2;
                return (cellY >= roadY && cellY <= roadY + ROAD_WIDTH);
            }
                
            case VERTICAL: {
                double roadX = GRID_SIZE/2 - ROAD_WIDTH/2;
                return (cellX >= roadX && cellX <= roadX + ROAD_WIDTH);
            }
                
            case INTERSECTION: {
                double roadY = GRID_SIZE/2 - ROAD_WIDTH/2;
                double roadX = GRID_SIZE/2 - ROAD_WIDTH/2;
                return (cellY >= roadY && cellY <= roadY + ROAD_WIDTH) || 
                       (cellX >= roadX && cellX <= roadX + ROAD_WIDTH);
            }
        }
    }
    
    return FALSE;
}

// Water and mud features have been removed

// Draw a single road segment
void DrawRoadSegment(HDC hdc, RoadSegment* road)
{
    int screenX, screenY, roadY, roadX;
    int worldX, worldY;
    int stripeOffset, numStripes, i;
    int intersectionCenterX, intersectionCenterY;
    int stripeX, stripeY;
    HBRUSH roadBrush, stripeBrush;
    HPEN noPen;
    
    /* Constants for stripe placement */
    #define STRIPE_LENGTH 20  /* Length of each stripe */
    #define STRIPE_WIDTH 4    /* Width of each stripe */
    #define STRIPE_GAP 30     /* Gap between stripes */
    #define STRIPE_CYCLE (STRIPE_LENGTH + STRIPE_GAP)  /* Total distance between stripe starts */
    
    // Calculate world coordinates
    worldX = road->gridX * GRID_SIZE;
    worldY = road->gridY * GRID_SIZE;
    
    // Convert to screen coordinates (adjust for camera position)
    screenX = worldX - (int)gameState.cameraX;
    screenY = worldY - (int)gameState.cameraY;
    
    // Skip drawing if outside screen with margin
    if (screenX + GRID_SIZE < -100 || screenX > SCREEN_WIDTH + 100 ||
        screenY + GRID_SIZE < -100 || screenY > SCREEN_HEIGHT + 100) {
        return;
    }
    
    // Create brushes and pens with standard VGA colors
    roadBrush = CreateSolidBrush(RGB(128, 128, 128)); // Gray for the road
    stripeBrush = CreateSolidBrush(RGB(255, 255, 255)); // White for road stripes
    noPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    
    SelectObject(hdc, roadBrush);
    SelectObject(hdc, noPen);
    
    // Calculate actual world position
    worldX = road->gridX * GRID_SIZE;
    worldY = road->gridY * GRID_SIZE;
    
    // Calculate grid-independent stripe offsets
    // Use road's world position to determine stripe positions so they align across grid cells
    
    // Draw based on direction
    switch (road->direction) {
        case HORIZONTAL:
            // Draw horizontal road
            roadY = screenY + GRID_SIZE/2 - ROAD_WIDTH/2;
            Rectangle(hdc, screenX-1, roadY, screenX + GRID_SIZE+1, roadY + ROAD_WIDTH);
            
            // Draw stripes based on global position
            SelectObject(hdc, stripeBrush);
            
            // Calculate stripe offset in this grid cell based on world position
            // This ensures stripes align across grid cell boundaries
            stripeOffset = (worldX % STRIPE_CYCLE + STRIPE_CYCLE) % STRIPE_CYCLE;
            // How many stripes can fit in this road segment
            numStripes = (GRID_SIZE + stripeOffset) / STRIPE_CYCLE + 1;
            
            // Draw each stripe
            for (i = 0; i < numStripes; i++) {
                stripeX = screenX - stripeOffset + (i * STRIPE_CYCLE);
                // Only draw if the stripe is entirely within the road segment
                if (stripeX >= screenX && stripeX + STRIPE_LENGTH <= screenX + GRID_SIZE) {
                    // Draw stripe in the middle of the road
                    Rectangle(hdc, stripeX, roadY + ROAD_WIDTH/2 - STRIPE_WIDTH/2, 
                              stripeX + STRIPE_LENGTH, roadY + ROAD_WIDTH/2 + STRIPE_WIDTH/2);
                }
            }
            break;
            
        case VERTICAL:
            // Draw vertical road
            roadX = screenX + GRID_SIZE/2 - ROAD_WIDTH/2;
            Rectangle(hdc, roadX, screenY-1, roadX + ROAD_WIDTH, screenY + GRID_SIZE+1);
            
            // Draw stripes
            SelectObject(hdc, stripeBrush);
            
            // Calculate stripe offset in this grid cell based on world position
            // This ensures stripes align across grid cell boundaries
            stripeOffset = (worldY % STRIPE_CYCLE + STRIPE_CYCLE) % STRIPE_CYCLE;
            // How many stripes can fit in this road segment
            numStripes = (GRID_SIZE + stripeOffset) / STRIPE_CYCLE + 1;
            
            // Draw each stripe
            for (i = 0; i < numStripes; i++) {
                stripeY = screenY - stripeOffset + (i * STRIPE_CYCLE);
                // Only draw if the stripe is entirely within the road segment
                if (stripeY >= screenY && stripeY + STRIPE_LENGTH <= screenY + GRID_SIZE) {
                    // Draw stripe in the middle of the road
                    Rectangle(hdc, roadX + ROAD_WIDTH/2 - STRIPE_WIDTH/2, stripeY, 
                              roadX + ROAD_WIDTH/2 + STRIPE_WIDTH/2, stripeY + STRIPE_LENGTH);
                }
            }
            break;
            
        case INTERSECTION:
            // Draw horizontal road
            roadY = screenY + GRID_SIZE/2 - ROAD_WIDTH/2;
            Rectangle(hdc, screenX-1, roadY, screenX + GRID_SIZE+1, roadY + ROAD_WIDTH);
            
            // Draw vertical road
            roadX = screenX + GRID_SIZE/2 - ROAD_WIDTH/2;
            Rectangle(hdc, roadX, screenY-1, roadX + ROAD_WIDTH, screenY + GRID_SIZE+1);
            
            /* Get the center of the intersection */
            intersectionCenterX = screenX + GRID_SIZE/2;
            intersectionCenterY = screenY + GRID_SIZE/2;
            
            // Draw stripes for horizontal road
            SelectObject(hdc, stripeBrush);
            
            // Calculate stripe offset in this grid cell for horizontal road
            // This ensures stripes align across grid cell boundaries
            stripeOffset = (worldX % STRIPE_CYCLE + STRIPE_CYCLE) % STRIPE_CYCLE;
            // How many stripes can fit in this road segment
            numStripes = (GRID_SIZE + stripeOffset) / STRIPE_CYCLE + 1;
            
            // Draw each stripe
            for (i = 0; i < numStripes; i++) {
                stripeX = screenX - stripeOffset + (i * STRIPE_CYCLE);
                
                // Skip drawing if stripe is in the intersection area
                if (stripeX < intersectionCenterX - ROAD_WIDTH/2 - STRIPE_LENGTH || 
                    stripeX > intersectionCenterX + ROAD_WIDTH/2) {
                    
                    // Only draw if the stripe is entirely within the road segment
                    if (stripeX >= screenX && stripeX + STRIPE_LENGTH <= screenX + GRID_SIZE) {
                        // Draw stripe in the middle of the road
                        Rectangle(hdc, stripeX, roadY + ROAD_WIDTH/2 - STRIPE_WIDTH/2, 
                                  stripeX + STRIPE_LENGTH, roadY + ROAD_WIDTH/2 + STRIPE_WIDTH/2);
                    }
                }
            }
            
            // Draw stripes for vertical road
            // Calculate stripe offset in this grid cell for vertical road
            // This ensures stripes align across grid cell boundaries
            stripeOffset = (worldY % STRIPE_CYCLE + STRIPE_CYCLE) % STRIPE_CYCLE;
            // How many stripes can fit in this road segment
            numStripes = (GRID_SIZE + stripeOffset) / STRIPE_CYCLE + 1;
            
            // Draw each stripe
            for (i = 0; i < numStripes; i++) {
                stripeY = screenY - stripeOffset + (i * STRIPE_CYCLE);
                
                // Skip drawing if stripe is in the intersection area
                if (stripeY < intersectionCenterY - ROAD_WIDTH/2 - STRIPE_LENGTH || 
                    stripeY > intersectionCenterY + ROAD_WIDTH/2) {
                    
                    // Only draw if the stripe is entirely within the road segment
                    if (stripeY >= screenY && stripeY + STRIPE_LENGTH <= screenY + GRID_SIZE) {
                        // Draw stripe in the middle of the road
                        Rectangle(hdc, roadX + ROAD_WIDTH/2 - STRIPE_WIDTH/2, stripeY, 
                                  roadX + ROAD_WIDTH/2 + STRIPE_WIDTH/2, stripeY + STRIPE_LENGTH);
                    }
                }
            }
            break;
    }
    
    // Clean up resources
    DeleteObject(roadBrush);
    DeleteObject(stripeBrush);
    DeleteObject(noPen);
}

// Water and mud features have been removed

// Draw an enemy tank
void DrawEnemyTank(HDC hdc, EnemyTank *enemy)
{
    HBRUSH tankBrush;
    HPEN tankPen;
    HBRUSH turretBrush;
    COLORREF barrelColor;
    HPEN barrelPen;
    double barrelEndX, barrelEndY;
    int oldGraphicsMode;
    int tankScreenX, tankScreenY;
    int barrelScreenEndX, barrelScreenEndY;
    XFORM originalTransform;
    XFORM rotationTransform;
    COLORREF flashColor;
    HBRUSH flashBrush;
    HPEN noPen;
    
    // Skip drawing if outside screen with large margin
    tankScreenX = (int)(enemy->x - gameState.cameraX);
    tankScreenY = (int)(enemy->y - gameState.cameraY);
    if (tankScreenX < -100 || tankScreenX > SCREEN_WIDTH + 100 ||
        tankScreenY < -100 || tankScreenY > SCREEN_HEIGHT + 100) {
        return;
    }
    
    // If enemy is exploding, draw explosion and return
    if (enemy->exploding) {
        DrawExplosion(hdc, enemy->x, enemy->y, enemy->explosionTimer);
        return;
    }
    
    // If enemy is not active (and not exploding), don't draw it
    if (!enemy->active) {
        return;
    }
    
    // Draw tank body - red for enemies, white if hit flash is active
    if (enemy->hitFlashActive) {
        // White flash when hit
        tankBrush = CreateSolidBrush(RGB(255, 255, 255));  /* White flash when hit */
    } else {
        tankBrush = CreateSolidBrush(RGB(255, 0, 0));  /* Red color for enemy tank body */
    }
    tankPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0)); /* Black outline */
    
    SelectObject(hdc, tankBrush);
    SelectObject(hdc, tankPen);
    
    // Save the current state of the device context
    oldGraphicsMode = SetGraphicsMode(hdc, GM_ADVANCED);
    GetWorldTransform(hdc, &originalTransform);
    
    // Set up rotation transformation
    rotationTransform.eM11 = (float)cos(enemy->tankAngle);
    rotationTransform.eM12 = (float)sin(enemy->tankAngle);
    rotationTransform.eM21 = (float)-sin(enemy->tankAngle);
    rotationTransform.eM22 = (float)cos(enemy->tankAngle);
    rotationTransform.eDx = (float)tankScreenX;
    rotationTransform.eDy = (float)tankScreenY;
    
    SetWorldTransform(hdc, &rotationTransform);
    
    // Draw the rectangle centered at the origin (since we've translated to tank position)
    Rectangle(hdc, 
              -TANK_WIDTH/2, 
              -TANK_HEIGHT/2,
              TANK_WIDTH/2, 
              TANK_HEIGHT/2);
              
    // Restore original transformation
    SetWorldTransform(hdc, &originalTransform);
    SetGraphicsMode(hdc, oldGraphicsMode);
    
    // Draw turret (a circle for now)
    if (enemy->hitFlashActive) {
        // White flash when hit
        turretBrush = CreateSolidBrush(RGB(255, 255, 255));  /* White flash when hit */
    } else {
        turretBrush = CreateSolidBrush(RGB(255, 0, 0)); /* Red for enemy turret */
    }
    SelectObject(hdc, turretBrush);
    
    Ellipse(hdc, 
            tankScreenX - TURRET_SIZE/2, 
            tankScreenY - TURRET_SIZE/2,
            tankScreenX + TURRET_SIZE/2, 
            tankScreenY + TURRET_SIZE/2);
    
    // Draw barrel
    // Calculate the endpoint of the barrel
    barrelEndX = enemy->x + cos(enemy->turretAngle) * BARREL_LENGTH;
    barrelEndY = enemy->y + sin(enemy->turretAngle) * BARREL_LENGTH;
    
    // Convert barrel endpoint to screen coordinates
    barrelScreenEndX = (int)(barrelEndX - gameState.cameraX);
    barrelScreenEndY = (int)(barrelEndY - gameState.cameraY);
    
    // Set barrel color, white if hit flash is active
    if (enemy->hitFlashActive) {
        barrelColor = RGB(255, 255, 255); /* White flash when hit */
    } else {
        barrelColor = RGB(255, 0, 0); /* Red for enemy barrel */
    }
    
    // Draw a line for the barrel
    barrelPen = CreatePen(PS_SOLID, BARREL_WIDTH, barrelColor);
    SelectObject(hdc, barrelPen);
    
    MoveToEx(hdc, tankScreenX, tankScreenY, NULL);
    LineTo(hdc, barrelScreenEndX, barrelScreenEndY);
    
    // Draw a tiny red circle at the end of the barrel if firing
    if (enemy->muzzleFlashActive) {
        // Yellow flash for enemies (standard VGA color)
        flashColor = RGB(255, 255, 0);
        
        flashBrush = CreateSolidBrush(flashColor);
        noPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0)); // No outline for clean circle
        
        SelectObject(hdc, flashBrush);
        SelectObject(hdc, noPen);
        
        // Draw a tiny circle at the end of the barrel
        Ellipse(hdc, 
               barrelScreenEndX - 2, 
               barrelScreenEndY - 2,
               barrelScreenEndX + 2, 
               barrelScreenEndY + 2);
               
        DeleteObject(flashBrush);
        DeleteObject(noPen);
    }
    
    // Health bars removed as requested
    
    // Clean up resources
    DeleteObject(tankBrush);
    DeleteObject(tankPen);
    DeleteObject(turretBrush);
    DeleteObject(barrelPen);
}

// Draw all enemy tanks
void DrawEnemyTanks(HDC hdc)
{
    int i;
    
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (gameState.enemies[i].active) {
            DrawEnemyTank(hdc, &gameState.enemies[i]);
        }
    }
}

// Count the number of active enemies
int CountActiveEnemies() 
{
    int count = 0;
    int i;
    
    for (i = 0; i < MAX_ENEMIES; i++) {
        // Only count active tanks that aren't in the process of exploding
        if (gameState.enemies[i].active && !gameState.enemies[i].exploding) {
            count++;
        }
    }
    
    return count;
}

// Update all enemy tanks
void UpdateEnemyTanks()
{
    int i;
    double distanceToPlayer;
    double angleToPlayer;
    double angleDiff; // Declare angleDiff at function scope
    EnemyTank *enemy;
    static DWORD lastSpawnTime = 0;
    DWORD currentTime;
    
    // Update active enemy count - only count enemies that are ready for battle
    // (CountActiveEnemies excludes exploding tanks)
    gameState.enemyCount = CountActiveEnemies();
    
    currentTime = GetTickCount();
    
    for (i = 0; i < MAX_ENEMIES; i++) {
        enemy = &gameState.enemies[i];
        
        // Handle exploding enemies
        if (enemy->exploding) {
            // Update explosion animation
            enemy->explosionTimer--;
            
            // If explosion animation is done, completely deactivate the enemy
            if (enemy->explosionTimer <= 0) {
                enemy->exploding = FALSE;
                enemy->active = FALSE;
            }
            
            // Skip the rest of the updates for this enemy while it's exploding
            continue;
        }
        
        // Skip inactive enemies
        if (!enemy->active) {
            continue;
        }
        
        // Update muzzle flash
        if (enemy->muzzleFlashActive) {
            enemy->muzzleFlashTimer--;
            if (enemy->muzzleFlashTimer <= 0) {
                enemy->muzzleFlashActive = FALSE;
            }
        }
        
        // Update hit flash
        if (enemy->hitFlashActive) {
            animationsActive = TRUE; // Keep redrawing while flash is active
            enemy->hitFlashTimer--;
            if (enemy->hitFlashTimer <= 0) {
                enemy->hitFlashActive = FALSE;
            }
        }
        
        // Skip AI updates if player is not active
        if (!gameState.playerActive) {
            continue;
        }
        
        // Calculate distance to player
        distanceToPlayer = CalculateDistance(
            enemy->x, enemy->y,
            gameState.tankX, gameState.tankY
        );
        
        // If player is within detection range
        if (distanceToPlayer < ENEMY_DETECTION_RANGE) {
            // Calculate angle to player
            angleToPlayer = atan2(
                gameState.tankY - enemy->y,
                gameState.tankX - enemy->x
            );
            
            // Rotate turret toward player
            // Gradually turn turret toward player (not instant)
            angleDiff = angleToPlayer - enemy->turretAngle;
            
            // Normalize angle difference to be between -PI and PI
            while (angleDiff > 3.14159) angleDiff -= 2 * 3.14159;
            while (angleDiff < -3.14159) angleDiff += 2 * 3.14159;
            
            // Gradually rotate turret
            if (fabs(angleDiff) < 0.1) {
                enemy->turretAngle = angleToPlayer; // Close enough to target
            } else if (angleDiff > 0) {
                enemy->turretAngle += 0.03; // Rotate clockwise
            } else {
                enemy->turretAngle -= 0.03; // Rotate counter-clockwise
            }
            
            // If turret is pointed at player (within a small margin), fire
            if (fabs(angleDiff) < 0.1) {
                FireEnemyProjectile(enemy);
            }
            
            // Move toward player if not too close
            if (distanceToPlayer > 150) {
                // Gradually rotate tank body toward player
                angleDiff = angleToPlayer - enemy->tankAngle;
                
                // Normalize angle difference
                while (angleDiff > 3.14159) angleDiff -= 2 * 3.14159;
                while (angleDiff < -3.14159) angleDiff += 2 * 3.14159;
                
                // Adjust tank angle
                if (fabs(angleDiff) < 0.1) {
                    enemy->tankAngle = angleToPlayer;
                } else if (angleDiff > 0) {
                    enemy->tankAngle += 0.02;
                } else {
                    enemy->tankAngle -= 0.02;
                }
                
                // Move forward
                enemy->x += cos(enemy->tankAngle) * ENEMY_MOVEMENT_SPEED;
                enemy->y += sin(enemy->tankAngle) * ENEMY_MOVEMENT_SPEED;
            }
        }
    }
    
    // Check if we need to spawn a new enemy to replace destroyed ones
    if (gameState.enemyCount < MAX_ENEMIES && 
        currentTime - lastSpawnTime > RESPAWN_DELAY) {
        
        // Try to spawn a new enemy - higher chance for more frequent encounters
        if (rand() % 150 == 0) { // 1/150 chance each frame (twice as frequent)
            // Pick a closer, but still off-screen location
            double spawnDistance = 300 + (rand() % 200); // 300-500 pixels away (closer to player)
            double spawnAngle = ((double)rand() / RAND_MAX) * 2 * 3.14159; // Random angle
            
            // Calculate spawn position relative to player
            double spawnX = gameState.tankX + cos(spawnAngle) * spawnDistance;
            double spawnY = gameState.tankY + sin(spawnAngle) * spawnDistance;
            
            // Spawn the enemy
            SpawnEnemyTank(spawnX, spawnY);
            lastSpawnTime = currentTime;
        }
    }
}

// Draw game background and roads
void DrawBackground(HDC hdc)
{
    int i;
    HBRUSH grassBrush;
    HPEN noPen;
    
    // Fill background with standard VGA green for grass
    grassBrush = CreateSolidBrush(RGB(0, 128, 0)); // Dark green (VGA color)
    noPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    
    SelectObject(hdc, grassBrush);
    SelectObject(hdc, noPen);
    
    Rectangle(hdc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Clean up grass resources
    DeleteObject(grassBrush);
    DeleteObject(noPen);
    
    // Draw all road segments
    for (i = 0; i < MAX_ROADS; i++) {
        if (gameState.roads[i].active) {
            DrawRoadSegment(hdc, &gameState.roads[i]);
        }
    }
}

// Convert screen coordinates to world coordinates
void ScreenToWorld(int screenX, int screenY, double* worldX, double* worldY)
{
    *worldX = screenX + gameState.cameraX;
    *worldY = screenY + gameState.cameraY;
}

// Convert world coordinates to screen coordinates
void WorldToScreen(double worldX, double worldY, int* screenX, int* screenY)
{
    *screenX = (int)(worldX - gameState.cameraX);
    *screenY = (int)(worldY - gameState.cameraY);
}

// Update game state
void UpdateGame()
{
    DWORD currentTime;
    DWORD deltaTime;
    double currentSpeed;
    POINT mousePos;
    HWND hwnd;
    double dx, dy;
    double mouseWorldX, mouseWorldY;
    int tankScreenX, tankScreenY;
    double oldTankX, oldTankY, oldTankAngle, oldTurretAngle;
    double oldCameraX, oldCameraY;
    static BOOL rKeyWasPressed = FALSE;
    BOOL rKeyIsPressed;
    
    BOOL mouseIsPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? TRUE : FALSE;
    
    // Always force redraw when game is paused
    if (gamePaused) {
        static int pauseTimer = 0;
        gameNeedsRedraw = TRUE;
        
        // Add a small delay before accepting input (prevents accidental clicks)
        pauseTimer++;
        if (pauseTimer < 10) {  // Reduced wait time to 1/3 second at 30fps for better responsiveness
            return;
        }
        
        // Check for mouse click to unpause
        if (mouseIsPressed) {
            gamePaused = FALSE;
            pauseTimer = 0;
        }
        
        // Check for space or enter key to unpause
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
            gamePaused = FALSE;
            pauseTimer = 0;
        }
        
        // Update mouse pressed state
        mouseWasPressed = mouseIsPressed;
        
        // Early return if still paused
        if (gamePaused) {
            return;
        }
    }
    
    // Remember old positions to detect movement
    oldTankX = gameState.tankX;
    oldTankY = gameState.tankY;
    oldTankAngle = gameState.tankAngle;
    oldTurretAngle = gameState.turretAngle;
    
    // Reset movement/animation flags
    playerHasMoved = FALSE;
    animationsActive = FALSE;
    
    currentTime = GetTickCount();
    deltaTime = currentTime - gameState.lastFrameTime;
    gameState.lastFrameTime = currentTime;
    
    // Determine current speed based on terrain
    currentSpeed = TANK_GRASS_SPEED; // Default to grass speed
    
    if (IsTankOnRoad()) {
        currentSpeed = TANK_SPEED; // Fast on roads
    }
    
    // Handle keyboard input for tank movement - only if player is active and not exploding
    if (gameState.playerActive && !gameState.playerExploding) {
        if (GetAsyncKeyState('W') & 0x8000)
        {
            // Move forward in the direction of the tank
            gameState.tankX += cos(gameState.tankAngle) * currentSpeed;
            gameState.tankY += sin(gameState.tankAngle) * currentSpeed;
            playerHasMoved = TRUE;
        }
        
        if (GetAsyncKeyState('S') & 0x8000)
        {
            // Move backward
            gameState.tankX -= cos(gameState.tankAngle) * currentSpeed;
            gameState.tankY -= sin(gameState.tankAngle) * currentSpeed;
            playerHasMoved = TRUE;
        }
        
        if (GetAsyncKeyState('A') & 0x8000)
        {
            // Rotate counter-clockwise
            gameState.tankAngle -= 0.05;
            playerHasMoved = TRUE;
        }
        
        if (GetAsyncKeyState('D') & 0x8000)
        {
            // Rotate clockwise
            gameState.tankAngle += 0.05;
            playerHasMoved = TRUE;
        }
    }
    
    // Check for mouse click to fire projectile
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
    {
        FireProjectile();
        animationsActive = TRUE;
    }
    
    // Check for R key to toggle radar mode
    rKeyIsPressed = (GetAsyncKeyState('R') & 0x8000) ? TRUE : FALSE;
    
    // Only toggle when the key is first pressed (not held)
    if (rKeyIsPressed && !rKeyWasPressed) {
        radarActive = !radarActive;  // Toggle radar mode
        animationsActive = TRUE;     // Force redraw
    }
    rKeyWasPressed = rKeyIsPressed;
    
    // Get mouse position for turret aiming - only if player is active and not exploding
    if (gameState.playerActive && !gameState.playerExploding) {
        GetCursorPos(&mousePos);
        
        // Convert screen coordinates to client coordinates
        hwnd = FindWindow(szWindowClass, NULL);
        if (hwnd)
        {
            ScreenToClient(hwnd, &mousePos);
            gameState.mouseX = mousePos.x;
            gameState.mouseY = mousePos.y;
            
            // Convert screen coordinates to world coordinates
            ScreenToWorld(gameState.mouseX, gameState.mouseY, &mouseWorldX, &mouseWorldY);
            
            // Calculate angle from tank to mouse in world coordinates
            dx = mouseWorldX - gameState.tankX;
            dy = mouseWorldY - gameState.tankY;
            gameState.turretAngle = atan2(dy, dx);
            
            // Check if turret angle changed significantly enough to require redraw
            if (fabs(gameState.turretAngle - oldTurretAngle) > 0.01) {
                playerHasMoved = TRUE;
            }
        }
    }
    
    // Check if active projectiles exist
    if (gameState.projectileCount > 0) {
        animationsActive = TRUE;
    }
    
    // Update all projectiles
    UpdateProjectiles();
    
    // Update enemy tanks
    UpdateEnemyTanks();
    
    // Check for active enemy animations and movement
    {
        int i;
        EnemyTank *enemy;
        double distanceToPlayer;
        
        for (i = 0; i < MAX_ENEMIES; i++) {
            enemy = &gameState.enemies[i];
            
            // Check for muzzle flash
            if (enemy->active && enemy->muzzleFlashActive) {
                animationsActive = TRUE;
            }
            
            // Check if exploding
            if (enemy->exploding) {
                animationsActive = TRUE;
            }
            
            // Check if enemy is moving (moving enemies are within detection range of player)
            if (enemy->active && !enemy->exploding) {
                distanceToPlayer = CalculateDistance(
                    enemy->x, enemy->y,
                    gameState.tankX, gameState.tankY
                );
                
                if (distanceToPlayer < ENEMY_DETECTION_RANGE) {
                    animationsActive = TRUE; // Enemy is moving or targeting player
                }
            }
        }
    }
    
    // Update player explosion and handle respawn
    if (gameState.playerExploding) {
        animationsActive = TRUE;
        gameState.playerExplosionTimer--;
        
        // If explosion animation is done, respawn player after delay
        if (gameState.playerExplosionTimer <= 0) {
            // Keep the BSOD screen displayed, don't auto-respawn
            // The respawn will be triggered by WM_KEYDOWN or WM_LBUTTONDOWN
            gameState.playerExplosionTimer = 1; // Keep at 1 to prevent auto-respawn
        }
    }
    
    // Update muzzle flash effect
    if (gameState.muzzleFlashActive)
    {
        animationsActive = TRUE;
        gameState.muzzleFlashTimer--;
        if (gameState.muzzleFlashTimer <= 0)
        {
            gameState.muzzleFlashActive = FALSE;
        }
    }
    
    // Update player hit flash effect
    if (gameState.playerHitFlashActive)
    {
        animationsActive = TRUE;
        gameState.playerHitFlashTimer--;
        if (gameState.playerHitFlashTimer <= 0)
        {
            gameState.playerHitFlashActive = FALSE;
        }
    }
    
    // Update camera position based on tank position
    // Convert tank world coordinates to screen coordinates
    WorldToScreen(gameState.tankX, gameState.tankY, &tankScreenX, &tankScreenY);
    
    // Track if camera movement happens
    oldCameraX = gameState.cameraX;
    oldCameraY = gameState.cameraY;
    
    // Check horizontal margins
    if (tankScreenX < SCROLL_MARGIN) {
        gameState.cameraX -= (SCROLL_MARGIN - tankScreenX);
    } else if (tankScreenX > SCREEN_WIDTH - SCROLL_MARGIN) {
        gameState.cameraX += (tankScreenX - (SCREEN_WIDTH - SCROLL_MARGIN));
    }
    
    // Check vertical margins
    if (tankScreenY < SCROLL_MARGIN) {
        gameState.cameraY -= (SCROLL_MARGIN - tankScreenY);
    } else if (tankScreenY > SCREEN_HEIGHT - SCROLL_MARGIN) {
        gameState.cameraY += (tankScreenY - (SCREEN_HEIGHT - SCROLL_MARGIN));
    }
    
    // Check if camera moved
    if (oldCameraX != gameState.cameraX || oldCameraY != gameState.cameraY) {
        playerHasMoved = TRUE;
    }
    
    // Generate new road segments around the camera position
    GenerateRoadsAroundPosition(
        gameState.cameraX + SCREEN_WIDTH/2, 
        gameState.cameraY + SCREEN_HEIGHT/2
    );
    
    // Set redraw flag if anything changed
    gameNeedsRedraw = playerHasMoved || animationsActive;
}

// Initialize double buffering
void InitDoubleBuffer(HWND hwnd)
{
    HDC hdc;
    RECT rect;
    
    // Clean up any existing buffer
    CleanupDoubleBuffer();
    
    // Get the client area size
    GetClientRect(hwnd, &rect);
    bufferWidth = rect.right;
    bufferHeight = rect.bottom;
    
    // Create a compatible DC for the screen
    hdc = GetDC(hwnd);
    hBufferDC = CreateCompatibleDC(hdc);
    
    // Create a compatible bitmap for the screen
    hBufferBitmap = CreateCompatibleBitmap(hdc, bufferWidth, bufferHeight);
    
    // Select the bitmap into the buffer DC
    SelectObject(hBufferDC, hBufferBitmap);
    
    // Release the device context
    ReleaseDC(hwnd, hdc);
}

// Resize the double buffer when the window size changes
void ResizeDoubleBuffer(HWND hwnd)
{
    // Just reinitialize the buffer
    InitDoubleBuffer(hwnd);
}

// Clean up double buffering resources
void CleanupDoubleBuffer(void)
{
    if (hBufferDC != NULL) {
        DeleteDC(hBufferDC);
        hBufferDC = NULL;
    }
    
    if (hBufferBitmap != NULL) {
        DeleteObject(hBufferBitmap);
        hBufferBitmap = NULL;
    }
}

// Draw the radar arrows and distance indicators to enemies
void DrawRadar(HDC hdc)
{
    int i;
    int activeEnemyCount;
    EnemyTank* enemy;
    double distance;
    double angleToEnemy;
    int tankScreenX, tankScreenY;
    int enemyScreenX, enemyScreenY;
    HPEN arrowPen;
    char distanceText[20];
    int arrowLength, arrowWidth;
    double arrowEndX, arrowEndY;
    double arrowLeftX, arrowLeftY, arrowRightX, arrowRightY;
    COLORREF arrowColor;
    
    /* Variables for use in else block */
    int baseRadius;
    int screenRadius;
    double offsetAngle;
    double screenAngle;
    double headAngle;
    double dirX;
    double dirY;
    double dirLength;
    
    // Only draw radar if player is active and radar mode is enabled
    if (!gameState.playerActive || !radarActive) {
        return;
    }
    
    // Get player tank screen position
    WorldToScreen(gameState.tankX, gameState.tankY, &tankScreenX, &tankScreenY);
    
    // Create a bright cyan pen for the arrows (3 pixels thick)
    arrowPen = CreatePen(PS_SOLID, 3, RGB(0, 255, 255));
    SelectObject(hdc, arrowPen);
    
    // Set text color to bright cyan
    SetTextColor(hdc, RGB(0, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    
    // First, count how many active enemies we have
    activeEnemyCount = 0;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (gameState.enemies[i].active && !gameState.enemies[i].exploding) {
            activeEnemyCount++;
        }
    }
    
    // Loop through all enemies
    for (i = 0; i < MAX_ENEMIES; i++) {
        
        enemy = &gameState.enemies[i];
        
        // Only show active enemies that aren't exploding
        if (!enemy->active || enemy->exploding) {
            continue;
        }
        
        // Create a different color for each enemy's arrow (cycle through colors)
        switch (i % 4) {
            case 0: arrowColor = RGB(0, 255, 255);   break; // Cyan
            case 1: arrowColor = RGB(255, 255, 0);   break; // Yellow
            case 2: arrowColor = RGB(0, 255, 0);     break; // Green
            case 3: arrowColor = RGB(255, 0, 255);   break; // Magenta
            default: arrowColor = RGB(255, 255, 255); break; // White
        }
        
        // Create a pen with the enemy's color
        DeleteObject(arrowPen); // Delete previous pen
        arrowPen = CreatePen(PS_SOLID, 3, arrowColor);
        SelectObject(hdc, arrowPen);
        
        // Set text color to match arrow color
        SetTextColor(hdc, arrowColor);
        
        // Calculate distance
        distance = CalculateDistance(gameState.tankX, gameState.tankY, enemy->x, enemy->y);
        
        // Get enemy screen position
        WorldToScreen(enemy->x, enemy->y, &enemyScreenX, &enemyScreenY);
        
        // Calculate angle from tank to enemy
        angleToEnemy = atan2(enemy->y - gameState.tankY, enemy->x - gameState.tankX);
        
        // Check if enemy is on screen - if so, just show distance text
        if (enemyScreenX >= 0 && enemyScreenX < SCREEN_WIDTH && 
            enemyScreenY >= 0 && enemyScreenY < SCREEN_HEIGHT) {
            // Format distance text with units and enemy ID
            sprintf(distanceText, "%d: %.0f m", i+1, distance);
            
            // Draw a small circle at enemy position
            Ellipse(hdc, 
                   enemyScreenX - 5, 
                   enemyScreenY - 5,
                   enemyScreenX + 5, 
                   enemyScreenY + 5);
            
            // Draw distance text above enemy
            TextOut(hdc, enemyScreenX - 15, enemyScreenY - 20, distanceText, strlen(distanceText));
        }
        // If enemy is off screen, draw an arrow pointing to them
        else {
            // Determine arrow direction and endpoint
            baseRadius = 80;  // Base distance from tank to arrow
            
            // Calculate the direction vector from tank to enemy
            dirX = enemy->x - gameState.tankX;
            dirY = enemy->y - gameState.tankY;
            
            // Normalize the direction vector
            dirLength = sqrt(dirX * dirX + dirY * dirY);
            dirX /= dirLength;
            dirY /= dirLength;
            
            // Calculate the angle (more accurate than using atan2 twice)
            screenAngle = atan2(dirY, dirX);
            
            // Add the tank ID as an offset to distribute multiple arrows in similar directions
            // For widely-spaced tanks this will have minimal effect
            // For closely-spaced tanks, this will spread out the arrows
            offsetAngle = (i * 0.2) - ((MAX_ENEMIES - 1) * 0.1);  // Distribute across ±0.3 radians
            screenAngle += offsetAngle;
            
            // Get the current enemy's index to adjust radius (closer = larger)
            screenRadius = baseRadius - (int)((distance > 1000) ? 0 : (1000 - distance) / 40);
            if (screenRadius < 60) screenRadius = 60;
            if (screenRadius > 110) screenRadius = 110;
            
            // Calculate arrow endpoint
            arrowEndX = tankScreenX + cos(screenAngle) * screenRadius;
            arrowEndY = tankScreenY + sin(screenAngle) * screenRadius;
            
            // Calculate arrowhead size and angle
            arrowLength = 15; // Length of arrowhead
            arrowWidth = 10;  // Width of arrowhead
            
            // Set head angle (about 30 degrees in radians)
            headAngle = 0.5;
            
            arrowLeftX = arrowEndX - arrowLength * cos(screenAngle - headAngle);
            arrowLeftY = arrowEndY - arrowLength * sin(screenAngle - headAngle);
            arrowRightX = arrowEndX - arrowLength * cos(screenAngle + headAngle);
            arrowRightY = arrowEndY - arrowLength * sin(screenAngle + headAngle);
            
            // Draw arrow line
            MoveToEx(hdc, tankScreenX, tankScreenY, NULL);
            LineTo(hdc, (int)arrowEndX, (int)arrowEndY);
            
            // Draw arrow head
            MoveToEx(hdc, (int)arrowEndX, (int)arrowEndY, NULL);
            LineTo(hdc, (int)arrowLeftX, (int)arrowLeftY);
            MoveToEx(hdc, (int)arrowEndX, (int)arrowEndY, NULL);
            LineTo(hdc, (int)arrowRightX, (int)arrowRightY);
            
            // Format and draw distance text with units and enemy ID
            sprintf(distanceText, "%d: %.0f m", i+1, distance);
            TextOut(hdc, (int)arrowEndX + 5, (int)arrowEndY - 5, distanceText, strlen(distanceText));
        }
    }
    
    // Clean up
    DeleteObject(arrowPen);
}

// Wait for vertical sync to reduce screen tearing
void WaitForVSync(void)
{
    // Simple timing-based approach for VSync
    // More precise methods exist using DirectX but this is simpler for this application
    static DWORD lastVSyncTime = 0;
    DWORD currentTime;
    DWORD timeToWait;
    const DWORD VSYNC_INTERVAL = 33; // ~30 Hz (1000ms / 30fps) for less CPU usage
    
    currentTime = GetTickCount();
    timeToWait = 0;
    
    // Calculate time to wait for next vsync interval
    if (lastVSyncTime != 0) {
        DWORD timeSinceLastVSync = currentTime - lastVSyncTime;
        
        if (timeSinceLastVSync < VSYNC_INTERVAL) {
            timeToWait = VSYNC_INTERVAL - timeSinceLastVSync;
        }
    }
    
    // Wait if needed
    if (timeToWait > 0 && timeToWait < VSYNC_INTERVAL) {
        Sleep(timeToWait);
    }
    
    // Update last vsync time
    lastVSyncTime = GetTickCount();
}

// Update the window title with CPU info and game stats
void UpdateWindowTitle(HWND hWnd)
{
    char titleBuffer[150];
    char cpuArchStr[64] = "";
    SYSTEM_INFO sysInfo;
    
    // Get CPU information
    GetSystemInfo(&sysInfo);
    
    // Determine CPU architecture using arch.h
    if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_UNKNOWN) {
        lstrcpy(cpuArchStr, PROCESSOR_ARCHITECTURE_STR_UNKNOWN);
    } else if (sysInfo.wProcessorArchitecture < sizeof(ProcessorArchitectureNames)/sizeof(ProcessorArchitectureNames[0])) {
        lstrcpy(cpuArchStr, ProcessorArchitectureNames[sysInfo.wProcessorArchitecture]);
    } else {
        lstrcpy(cpuArchStr, PROCESSOR_ARCHITECTURE_STR_UNKNOWN);
    }
    
    // We only want to display the architecture, not the processor type
    
    // Format the title with CPU architecture and kill count
    sprintf(titleBuffer, "%s [%s] - Enemies Destroyed: %d", 
            szAppName, cpuArchStr, currentSessionKills);
    
    // Set the window title if window handle is valid
    if (hWnd) {
        SetWindowText(hWnd, titleBuffer);
    }
}

// Draw the pause screen with instructions
void DrawPauseScreen(HDC hdc)
{
    RECT rect;
    HFONT hFont;
    HFONT oldFont;
    int lineHeight = 30;
    int y = SCREEN_HEIGHT / 3;
    HBRUSH bgBrush;
    
    // Set transparent background for text
    SetBkMode(hdc, TRANSPARENT);
    
    // Add a semi-transparent overlay instead of solid background
    // The game map is already drawn in the background at this point
    rect.left = 0;
    rect.top = 0;
    rect.right = SCREEN_WIDTH;
    rect.bottom = SCREEN_HEIGHT;
    
    // No background overlay - this allows the game map with grass, road and tanks
    // to be fully visible behind the text
    // The text will be visible because we're using SetBkMode(TRANSPARENT) above
    
    // Create a font for instructions - make it bold so it stands out
    hFont = CreateFont(24, 0, 0, 0, FW_BOLD, 0, 0, 0, 
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    
    oldFont = SelectObject(hdc, hFont);
    
    // Draw title in yellow - larger font for the title
    SelectObject(hdc, oldFont); // Restore original font
    DeleteObject(hFont); // Delete the old font
    
    // Create a larger font for the title
    hFont = CreateFont(48, 0, 0, 0, FW_BOLD, 0, 0, 0, 
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    oldFont = SelectObject(hdc, hFont); // Store the original font again
    
    SetTextColor(hdc, RGB(255, 255, 0)); // Yellow title
    rect.left = 0;
    rect.top = y;
    rect.right = SCREEN_WIDTH;
    rect.bottom = y + lineHeight * 2;
    DrawText(hdc, "TANKZ", -1, &rect, DT_CENTER);
    
    // Switch back to normal font for instructions
    SelectObject(hdc, oldFont); // Restore original font
    DeleteObject(hFont); // Delete the title font
    
    // Create new font for instructions
    hFont = CreateFont(24, 0, 0, 0, FW_BOLD, 0, 0, 0, 
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    oldFont = SelectObject(hdc, hFont); // Store original font again
    
    y += lineHeight * 2;
    
    // Draw game instructions - all in white
    SetTextColor(hdc, RGB(255, 255, 255)); // White text for instructions
    rect.top = y;
    rect.bottom = y + lineHeight;
    DrawText(hdc, "Use WASD keys to move the tank", -1, &rect, DT_CENTER);
    
    y += lineHeight;
    rect.top = y;
    rect.bottom = y + lineHeight;
    DrawText(hdc, "Use mouse to aim turret", -1, &rect, DT_CENTER);
    
    y += lineHeight;
    rect.top = y;
    rect.bottom = y + lineHeight;
    DrawText(hdc, "Left-click to fire projectiles", -1, &rect, DT_CENTER);
    
    y += lineHeight;
    rect.top = y;
    rect.bottom = y + lineHeight;
    DrawText(hdc, "Press R to toggle radar", -1, &rect, DT_CENTER);
    
    y += lineHeight;
    rect.top = y;
    rect.bottom = y + lineHeight;
    DrawText(hdc, "Press ESC to pause game", -1, &rect, DT_CENTER);
    
    y += lineHeight * 2;
    
    // Start text in yellow and blinking effect
    SetTextColor(hdc, RGB(255, 255, 0)); // Yellow text for the start prompt
    rect.top = y;
    rect.bottom = y + lineHeight;
    DrawText(hdc, "CLICK MOUSE BUTTON OR PRESS SPACE TO START", -1, &rect, DT_CENTER);
    
    // Cleanup
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

// Draw a crosshair at the mouse position for better aiming
void DrawCrosshair(HDC hdc)
{
    int x = gameState.mouseX;
    int y = gameState.mouseY;
    int size = 10; // Size of the crosshair lines
    HPEN crosshairPen;
    HPEN oldPen;
    HBRUSH hBrush;
    
    // Create a bright red pen for the crosshair
    crosshairPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    oldPen = SelectObject(hdc, crosshairPen);
    
    // Draw horizontal line
    MoveToEx(hdc, x - size, y, NULL);
    LineTo(hdc, x + size, y);
    
    // Draw vertical line
    MoveToEx(hdc, x, y - size, NULL);
    LineTo(hdc, x, y + size);
    
    // Draw a small circle in the center
    hBrush = GetStockObject(NULL_BRUSH);
    SelectObject(hdc, hBrush);
    
    Ellipse(hdc, x - 3, y - 3, x + 3, y + 3);
    
    // Clean up
    SelectObject(hdc, oldPen);
    DeleteObject(crosshairPen);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC         hdc;
    PAINTSTRUCT ps;
    RECT        rect;
    
    switch (message)
    {
    case WM_CREATE:
        // Initialize double buffer
        InitDoubleBuffer(hwnd);
        
        // Set timer for game updates (33ms ~= 30 FPS for less CPU usage)
        SetTimer(hwnd, 1, 33, NULL);
        
        // Initialize the window title with kill count
        currentSessionKills = 0;
        radarActive = FALSE;
        
        // Make sure game starts paused
        gamePaused = TRUE;
        gameNeedsRedraw = TRUE;
        mouseWasPressed = FALSE;
        
        UpdateWindowTitle(hwnd);
        
        // Force initial repaint to show pause screen immediately
        InvalidateRect(hwnd, NULL, FALSE);
        
        // Set initial cursor to crosshair
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return 0;
    
    case WM_SIZE:
        // Resize buffer when window size changes
        ResizeDoubleBuffer(hwnd);
        return 0;
        
    case WM_TIMER:
        // Update game state
        UpdateGame();
        
        // When paused, always redraw to show pause screen
        if (gamePaused || gameNeedsRedraw) {
            InvalidateRect(hwnd, NULL, FALSE);  // FALSE to prevent background erasing (reduce flicker)
        } else {
            // If nothing is happening, sleep to reduce CPU usage
            Sleep(10); // Sleep for 10ms when idle
        }
        
        return 0;
        
    case WM_PAINT:
        {
            char statusText[100];
            HFONT gameOverFont;
            HFONT oldFont;
            RECT gameOverRect;
            HPEN whitePen, oldPen; 
            HBRUSH whiteBrush, oldBrush, nullBrush;
            int i;
            
            hdc = BeginPaint(hwnd, &ps);
            
            // Clear the buffer with black
            GetClientRect(hwnd, &rect);
            FillRect(hBufferDC, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            
            // Draw normal game elements
            DrawBackground(hBufferDC);
            DrawProjectiles(hBufferDC);
            DrawEnemyTanks(hBufferDC); // Draw enemies first (so player appears on top)
            DrawTank(hBufferDC);
            DrawRadar(hBufferDC);      // Draw radar arrows last (on top of everything)
            
            // Set up text properties for any text that might be displayed
            SetTextColor(hBufferDC, RGB(255, 255, 255));
            SetBkMode(hBufferDC, TRANSPARENT);
            
            // Display Game Over message as a Windows NT BSOD
            if (!gameState.playerActive && gameState.playerExploding) {
                RECT rect;
                HFONT consoleFont, oldFont;
                char bsodText[30][128]; /* Array to hold multiple lines of BSOD text */
                int i, yPos;
                
                /* Create full blue background for BSOD */
                rect.left = 0;
                rect.top = 0;
                rect.right = SCREEN_WIDTH;
                rect.bottom = SCREEN_HEIGHT;
                
                /* Fill with classic BSOD blue color */
                FillRect(hBufferDC, &rect, CreateSolidBrush(RGB(0, 0, 170)));
                
                /* Create console-like font for BSOD text */
                consoleFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, "Courier New");
                oldFont = SelectObject(hBufferDC, consoleFont);
                
                /* White text on blue background */
                SetTextColor(hBufferDC, RGB(255, 255, 255));
                SetBkMode(hBufferDC, TRANSPARENT);
                
                /* Prepare BSOD text content - reduced for smaller screen */
                lstrcpy(bsodText[0], "*** STOP: 0x0000000A (0xDEADC0DE,0x00000000,0x00000000,0x00000000)");
                lstrcpy(bsodText[1], "GAME OVER - IRQL_NOT_LESS_OR_EQUAL");
                lstrcpy(bsodText[2], "");
                lstrcpy(bsodText[3], "The system detected an invalid or unaligned access to a memory location.");
                
                /* Format score into technical-looking hex values */
                wsprintf(bsodText[4], "Tank error at address 0x%08X, kills=0x%08X, health=%d", 
                         (int)(gameState.tankX + gameState.tankY), gameState.enemiesDestroyed, gameState.tankHealth);
                
                lstrcpy(bsodText[5], "");
                lstrcpy(bsodText[6], "If this is the first time you've seen this error screen, restart your game.");
                lstrcpy(bsodText[7], "Check for viruses and remove any newly installed enemy tanks.");
                
                lstrcpy(bsodText[8], "");
                lstrcpy(bsodText[9], "Technical information:");
                
                /* Generate random hex dumps that look like memory addresses */
                wsprintf(bsodText[10], "*** PROCESS_NAME: TANKZ.EXE  PID: 0x%04X", GetCurrentProcessId());
                wsprintf(bsodText[11], "*** MEMORY_MANAGEMENT: 0x%08X  STACK_OVERFLOW: 0x%08X", 
                         0xBAADF00D, 0xDEADBEEF);
                
                {
                    char hexDigits[] = "0123456789ABCDEF";
                    int j;
                    int offset;
                    char *dumpLine;
                    
                    /* Only show 3 memory dump lines instead of 5 */
                    for (i = 12; i < 15; i++) {
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
                
                lstrcpy(bsodText[15], "");
                lstrcpy(bsodText[16], "* Press any key or click to restart");
                lstrcpy(bsodText[17], "* Press ESC or Q to quit");
                
                /* Display all the text lines */
                yPos = 30;
                for (i = 0; i < 18; i++) {
                    TextOut(hBufferDC, 30, yPos, bsodText[i], lstrlen(bsodText[i]));
                    yPos += 20;
                }
                
                /* Clean up */
                SelectObject(hBufferDC, oldFont);
                DeleteObject(consoleFont);
            }
            
            // Draw player health bar in top-left corner
            if (gameState.playerActive) {
                RECT healthRect;
                int i;
                int healthBarWidth = 15;  // Width of each health segment
                int healthBarHeight = 10; // Height of health bar
                int healthBarSpacing = 2; // Space between health segments
                int healthBarY = 10;      // Y position from top
                int healthBarX = 10;      // X position from left
                
                // Set up the white pen and brush for health bar
                whitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
                oldPen = SelectObject(hBufferDC, whitePen);
                oldBrush = SelectObject(hBufferDC, whiteBrush);
                
                // Draw each health segment as a small white rectangle
                for (i = 0; i < MAX_HEALTH; i++) {
                    // Draw filled rectangle if player has this health point
                    if (i < gameState.tankHealth) {
                        Rectangle(hBufferDC, 
                                 healthBarX + i * (healthBarWidth + healthBarSpacing), 
                                 healthBarY,
                                 healthBarX + i * (healthBarWidth + healthBarSpacing) + healthBarWidth, 
                                 healthBarY + healthBarHeight);
                    } 
                    // Draw outline for missing health (hollow rectangle)
                    else {
                        nullBrush = GetStockObject(NULL_BRUSH);
                        SelectObject(hBufferDC, nullBrush);
                        Rectangle(hBufferDC, 
                                 healthBarX + i * (healthBarWidth + healthBarSpacing), 
                                 healthBarY,
                                 healthBarX + i * (healthBarWidth + healthBarSpacing) + healthBarWidth, 
                                 healthBarY + healthBarHeight);
                        SelectObject(hBufferDC, whiteBrush); // Restore white brush
                    }
                }
                
                // Cleanup
                SelectObject(hBufferDC, oldPen);
                SelectObject(hBufferDC, oldBrush);
                DeleteObject(whitePen);
                DeleteObject(whiteBrush);
            }
            
            // If paused, draw pause screen over everything else
            if (gamePaused) {
                DrawPauseScreen(hBufferDC);
            }
            
            // Always draw crosshair last so it's on top of everything
            DrawCrosshair(hBufferDC);
            
            // Wait for vertical sync to reduce tearing
            WaitForVSync();
            
            // Copy the buffer to the screen all at once (this reduces flicker)
            BitBlt(hdc, 0, 0, bufferWidth, bufferHeight, hBufferDC, 0, 0, SRCCOPY);
            
            EndPaint(hwnd, &ps);
        }
        return 0;
        
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        CleanupDoubleBuffer();
        PostQuitMessage(0);
        return 0;
        
    case WM_KEYUP:
        // Use key release to track when all keys are released after death
        if (gameState.inputLockActive) {
            // Check if no keys are pressed anymore - check common game control keys
            if (!(GetAsyncKeyState(VK_UP) & 0x8000) && 
                !(GetAsyncKeyState(VK_DOWN) & 0x8000) && 
                !(GetAsyncKeyState(VK_LEFT) & 0x8000) && 
                !(GetAsyncKeyState(VK_RIGHT) & 0x8000) &&
                !(GetAsyncKeyState(VK_SPACE) & 0x8000) &&
                !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                // All keys released, disable the input lock
                gameState.inputLockActive = FALSE;
            }
        }
        return 0;
        
    case WM_KEYDOWN:
        // Check if player is on BSOD screen (dead) and needs respawn
        if (!gameState.playerActive && gameState.playerExploding) {
            // Ignore keypresses if input lock is active (prevents accidental restart)
            if (gameState.inputLockActive) {
                return 0;
            }
            
            // Handle ESC or Q to quit during BSOD screen
            if (wParam == VK_ESCAPE || wParam == 'Q') {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            
            // Any other key will restart after BSOD screen
            RespawnPlayerTank();
            return 0;
        }
        
        // Handle ESC key to pause the game during normal gameplay
        if (wParam == VK_ESCAPE) {
            gamePaused = !gamePaused;
            gameNeedsRedraw = TRUE;
            
            // Reset mouse pressed state when pausing
            if (gamePaused) {
                mouseWasPressed = FALSE;
            }
        }
        
        // Handle Q key to quit during normal gameplay
        if (wParam == 'Q') {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        return 0;
            
    case WM_MOUSEMOVE:
        // Store mouse position for custom crosshair drawing
        gameState.mouseX = LOWORD(lParam);
        gameState.mouseY = HIWORD(lParam);
        // Force redraw when mouse moves
        gameNeedsRedraw = TRUE;
        return 0;
        
    case WM_LBUTTONDOWN:
        // Set to TRUE for tracking mouseup events
        mouseWasPressed = TRUE;
        
        // Check if player is on BSOD screen (dead) and needs respawn
        if (!gameState.playerActive && gameState.playerExploding) {
            // Ignore mouse clicks if input lock is active (prevents accidental restart)
            if (gameState.inputLockActive) {
                return 0;
            }
            
            // Mouse click will restart after BSOD screen
            RespawnPlayerTank();
            return 0;
        }
        return 0;
        
    case WM_LBUTTONUP:
        // Check if player is on BSOD screen (dead) and needs respawn
        if (!gameState.playerActive && gameState.playerExploding) {
            // If input lock is active and mouse was pressed and now released,
            // disable the input lock to allow further clicks to restart
            if (gameState.inputLockActive && mouseWasPressed) {
                gameState.inputLockActive = FALSE;
            }
        }
        
        // Reset tracking for next click
        mouseWasPressed = FALSE;
        return 0;
        
    case WM_SETCURSOR:
        // Ensure crosshair cursor is used when mouse is in the client area
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            return TRUE;
        }
        break;
    }
    
    return DefWindowProc(hwnd, message, wParam, lParam);
}