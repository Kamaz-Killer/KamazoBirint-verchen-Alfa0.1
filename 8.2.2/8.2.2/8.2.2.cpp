#define _CRT_SECURE_NO_WARNINGS
#include "framework.h"
#include "8.2.2.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <windows.h>
#include <queue>

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

enum GameState {
    MENU,
    DIFFICULTY_SELECT,
    PLAYING,
    GAME_OVER,
    WIN_SCREEN
};

GameState gameState = MENU;
int selectedDifficulty = 1;
float currentFPS = 0.0f;

int ROOM_WIDTH = 15;
int ROOM_HEIGHT = 10;
int ROOMS_X = 3;
int ROOMS_Y = 3;
int TOTAL_WIDTH;
int TOTAL_HEIGHT;
#define CELL_SIZE 32
#define UI_WIDTH 240
#define MARGIN 10

#define EMPTY 0
#define PLAYER 1
#define WALL 2
#define GOLD 3
#define DOOR 4
#define HEALTH 5
#define TRAP 6
#define UNEXPLORED 7
#define KEY 8
#define EXIT_DOOR 9
#define FREE_DOOR 10

int* map = nullptr;
bool* explored = nullptr;
int currentRoomX = 1, currentRoomY = 1;
int steps = 0;
int gold = 0;
int health = 100;
int keys = 0;
int visitedRooms = 1;
int totalRooms = 0;
bool uiExpanded = true;
bool godMode = false;

HDC hdcBuffer = nullptr;
HBITMAP hbmBuffer = nullptr;
int bufferWidth = 0;
int bufferHeight = 0;

void GenerateMap();
void GenerateRoom(int roomX, int roomY);
void EnsurePlayerPath();
void ConnectRooms();
void EnsureAllRoomsConnected();
void PlaceDoors();
void PlaceDoorsInRoom(int roomX, int roomY);
void PlaceGoldAndItems();
void SpawnPlayer();
void ChangeRoom(int dx, int dy);
POINT FindPlayer();
void DrawMap(HDC hdc);
void DrawUI(HDC hdc);
void DrawMenu(HDC hdc);
void DrawDifficultySelect(HDC hdc);
void DrawGameOver(HDC hdc);
void DrawWinScreen(HDC hdc);
void MovePlayer(int dx, int dy);
void DamagePlayer(int damage);
void HealPlayer(int amount);
void GenerateRuins(int startX, int startY);
void CreatePathToExit(int roomX, int roomY);
void StartNewGame(int difficulty);
void UpdateFPS();
void CleanupGame();
void CreateBuffer(int width, int height);
void DeleteBuffer();
void VisitRoom(int roomX, int roomY);
void RemoveDoorInBothRooms(int x, int y, int dx, int dy);

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    srand((unsigned int)time(NULL));

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MY822, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MY822));
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CleanupGame();
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MY822));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MY822);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    int uiWidth = uiExpanded ? UI_WIDTH : 50;
    int windowWidth = TOTAL_WIDTH * CELL_SIZE + uiWidth + MARGIN * 2;
    int windowHeight = TOTAL_HEIGHT * CELL_SIZE + MARGIN * 2;

    HWND hWnd = CreateWindowW(szWindowClass, L"Лабиринт", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    CreateBuffer(windowWidth, windowHeight);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

void CreateBuffer(int width, int height)
{
    if (hdcBuffer)
    {
        DeleteBuffer();
    }

    HDC hdc = GetDC(nullptr);
    hdcBuffer = CreateCompatibleDC(hdc);
    hbmBuffer = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(hdcBuffer, hbmBuffer);
    bufferWidth = width;
    bufferHeight = height;
    ReleaseDC(nullptr, hdc);
}

void DeleteBuffer()
{
    if (hbmBuffer)
    {
        DeleteObject(hbmBuffer);
        hbmBuffer = nullptr;
    }
    if (hdcBuffer)
    {
        DeleteDC(hdcBuffer);
        hdcBuffer = nullptr;
    }
}

void CleanupGame()
{
    if (map) delete[] map;
    if (explored) delete[] explored;
    DeleteBuffer();
    map = nullptr;
    explored = nullptr;
}

void StartNewGame(int difficulty)
{
    CleanupGame();

    if (difficulty == 1)
    {
        ROOMS_X = 3;
        ROOMS_Y = 3;
    }
    else if (difficulty == 2)
    {
        ROOMS_X = 4;
        ROOMS_Y = 4;
    }
    else
    {
        ROOMS_X = 5;
        ROOMS_Y = 5;
    }

    TOTAL_WIDTH = ROOM_WIDTH * ROOMS_X;
    TOTAL_HEIGHT = ROOM_HEIGHT * ROOMS_Y;
    totalRooms = ROOMS_X * ROOMS_Y;

    map = new int[TOTAL_HEIGHT * TOTAL_WIDTH];
    explored = new bool[ROOMS_Y * ROOMS_X];

    currentRoomX = 1;
    currentRoomY = 1;
    steps = 0;
    gold = 0;
    health = 100;
    keys = 0;
    visitedRooms = 1;
    godMode = false;

    GenerateMap();
    gameState = PLAYING;
}

void GenerateMap()
{
    for (int y = 0; y < TOTAL_HEIGHT; y++)
        for (int x = 0; x < TOTAL_WIDTH; x++)
            map[y * TOTAL_WIDTH + x] = EMPTY;

    for (int y = 0; y < ROOMS_Y; y++)
        for (int x = 0; x < ROOMS_X; x++)
            explored[y * ROOMS_X + x] = false;

    explored[currentRoomY * ROOMS_X + currentRoomX] = true;
    visitedRooms = 1;

    for (int roomY = 0; roomY < ROOMS_Y; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X; roomX++)
        {
            GenerateRoom(roomX, roomY);
        }
    }

    ConnectRooms();
    EnsureAllRoomsConnected();
    PlaceDoors();
    EnsurePlayerPath();
    PlaceGoldAndItems();
    SpawnPlayer();

    int exitX = (ROOMS_X - 1) * ROOM_WIDTH + ROOM_WIDTH / 2;
    int exitY = (ROOMS_Y - 1) * ROOM_HEIGHT + ROOM_HEIGHT / 2;
    map[exitY * TOTAL_WIDTH + exitX] = EXIT_DOOR;
}

void GenerateRoom(int roomX, int roomY)
{
    int startX = roomX * ROOM_WIDTH;
    int startY = roomY * ROOM_HEIGHT;

    for (int y = startY; y < startY + ROOM_HEIGHT; y++)
    {
        for (int x = startX; x < startX + ROOM_WIDTH; x++)
        {
            if (y == startY || y == startY + ROOM_HEIGHT - 1 ||
                x == startX || x == startX + ROOM_WIDTH - 1)
            {
                map[y * TOTAL_WIDTH + x] = WALL;
            }
        }
    }

    GenerateRuins(startX, startY);
}

void EnsurePlayerPath()
{
    int startX = currentRoomX * ROOM_WIDTH;
    int startY = currentRoomY * ROOM_HEIGHT;

    CreatePathToExit(currentRoomX, currentRoomY);

    int doorPositions[4][2] = {
        {startX + ROOM_WIDTH / 2, startY},
        {startX + ROOM_WIDTH / 2, startY + ROOM_HEIGHT - 1},
        {startX, startY + ROOM_HEIGHT / 2},
        {startX + ROOM_WIDTH - 1, startY + ROOM_HEIGHT / 2}
    };

    for (int i = 0; i < 4; i++)
    {
        int x = doorPositions[i][0];
        int y = doorPositions[i][1];

        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dx = -1; dx <= 1; dx++)
            {
                int nx = x + dx;
                int ny = y + dy;

                if (nx >= 0 && nx < TOTAL_WIDTH && ny >= 0 && ny < TOTAL_HEIGHT)
                {
                    int cellType = map[ny * TOTAL_WIDTH + nx];
                    if (cellType == WALL && !(nx == x && ny == y))
                    {
                        map[ny * TOTAL_WIDTH + nx] = EMPTY;
                    }
                }
            }
        }
    }
}

void CreatePathToExit(int roomX, int roomY)
{
    int startX = roomX * ROOM_WIDTH + ROOM_WIDTH / 2;
    int startY = roomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;

    int doors[4][3] = {
        {startX, roomY * ROOM_HEIGHT, 0},
        {startX, roomY * ROOM_HEIGHT + ROOM_HEIGHT - 1, 1},
        {roomX * ROOM_WIDTH, startY, 2},
        {roomX * ROOM_WIDTH + ROOM_WIDTH - 1, startY, 3}
    };

    for (int i = 0; i < 4; i++)
    {
        int targetX = doors[i][0];
        int targetY = doors[i][1];

        int currentX = startX;
        int currentY = startY;

        while (currentX != targetX || currentY != targetY)
        {
            if (map[currentY * TOTAL_WIDTH + currentX] == WALL)
                map[currentY * TOTAL_WIDTH + currentX] = EMPTY;

            if (currentX < targetX) currentX++;
            else if (currentX > targetX) currentX--;
            else if (currentY < targetY) currentY++;
            else if (currentY > targetY) currentY--;
        }

        if (map[targetY * TOTAL_WIDTH + targetX] == WALL)
            map[targetY * TOTAL_WIDTH + targetX] = EMPTY;
    }
}

void GenerateRuins(int startX, int startY)
{
    int numWalls = rand() % 5 + 3;

    for (int w = 0; w < numWalls; w++)
    {
        int wallLength = rand() % 4 + 2;
        int startWallX = startX + 2 + rand() % (ROOM_WIDTH - 5);
        int startWallY = startY + 2 + rand() % (ROOM_HEIGHT - 5);
        int direction = rand() % 2;

        for (int l = 0; l < wallLength; l++)
        {
            int x = startWallX + (direction == 0 ? l : 0);
            int y = startWallY + (direction == 1 ? l : 0);

            if (x >= startX + 1 && x < startX + ROOM_WIDTH - 1 &&
                y >= startY + 1 && y < startY + ROOM_HEIGHT - 1)
            {
                if (rand() % 100 < 80)
                {
                    map[y * TOTAL_WIDTH + x] = WALL;
                }
            }
        }
    }

    int debrisCount = rand() % 8 + 5;
    for (int d = 0; d < debrisCount; d++)
    {
        int x = startX + 1 + rand() % (ROOM_WIDTH - 2);
        int y = startY + 1 + rand() % (ROOM_HEIGHT - 2);

        if (map[y * TOTAL_WIDTH + x] == EMPTY)
        {
            map[y * TOTAL_WIDTH + x] = WALL;
        }
    }
}

void ConnectRooms()
{
    for (int roomY = 0; roomY < ROOMS_Y; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X - 1; roomX++)
        {
            if (rand() % 100 < 85)
            {
                int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
                int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH - 1;

                for (int y = doorY - 1; y <= doorY + 1; y++)
                {
                    for (int x = doorX; x <= doorX + 2; x++)
                    {
                        if (y >= 0 && y < TOTAL_HEIGHT && x >= 0 && x < TOTAL_WIDTH)
                        {
                            map[y * TOTAL_WIDTH + x] = EMPTY;
                        }
                    }
                }
            }
        }
    }

    for (int roomY = 0; roomY < ROOMS_Y - 1; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X; roomX++)
        {
            if (rand() % 100 < 85)
            {
                int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH / 2;
                int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT - 1;

                for (int y = doorY; y <= doorY + 2; y++)
                {
                    for (int x = doorX - 1; x <= doorX + 1; x++)
                    {
                        if (y >= 0 && y < TOTAL_HEIGHT && x >= 0 && x < TOTAL_WIDTH)
                        {
                            map[y * TOTAL_WIDTH + x] = EMPTY;
                        }
                    }
                }
            }
        }
    }
}

void EnsureAllRoomsConnected()
{
    bool* visited = new bool[ROOMS_X * ROOMS_Y];
    for (int i = 0; i < ROOMS_X * ROOMS_Y; i++) visited[i] = false;

    std::queue<int> q;
    q.push(currentRoomY * ROOMS_X + currentRoomX);
    visited[currentRoomY * ROOMS_X + currentRoomX] = true;

    while (!q.empty())
    {
        int idx = q.front();
        q.pop();
        int roomY = idx / ROOMS_X;
        int roomX = idx % ROOMS_X;

        int neighbors[4][2] = {
            {roomX + 1, roomY},
            {roomX - 1, roomY},
            {roomX, roomY + 1},
            {roomX, roomY - 1}
        };

        for (int i = 0; i < 4; i++)
        {
            int nX = neighbors[i][0];
            int nY = neighbors[i][1];

            if (nX >= 0 && nX < ROOMS_X && nY >= 0 && nY < ROOMS_Y)
            {
                int nIdx = nY * ROOMS_X + nX;
                if (!visited[nIdx])
                {
                    visited[nIdx] = true;
                    q.push(nIdx);

                    // Создаем соединение
                    if (nX > roomX) // Правая комната
                    {
                        int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
                        int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH - 1;

                        for (int y = doorY - 1; y <= doorY + 1; y++)
                        {
                            for (int x = doorX; x <= doorX + 2; x++)
                            {
                                if (y >= 0 && y < TOTAL_HEIGHT && x >= 0 && x < TOTAL_WIDTH)
                                {
                                    if (map[y * TOTAL_WIDTH + x] == WALL)
                                        map[y * TOTAL_WIDTH + x] = EMPTY;
                                }
                            }
                        }
                    }
                    else if (nX < roomX) // Левая комната
                    {
                        int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
                        int doorX = roomX * ROOM_WIDTH;

                        for (int y = doorY - 1; y <= doorY + 1; y++)
                        {
                            for (int x = doorX - 2; x <= doorX + 1; x++)
                            {
                                if (y >= 0 && y < TOTAL_HEIGHT && x >= 0 && x < TOTAL_WIDTH)
                                {
                                    if (map[y * TOTAL_WIDTH + x] == WALL)
                                        map[y * TOTAL_WIDTH + x] = EMPTY;
                                }
                            }
                        }
                    }
                    else if (nY > roomY) // Нижняя комната
                    {
                        int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH / 2;
                        int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT - 1;

                        for (int y = doorY; y <= doorY + 2; y++)
                        {
                            for (int x = doorX - 1; x <= doorX + 1; x++)
                            {
                                if (y >= 0 && y < TOTAL_HEIGHT && x >= 0 && x < TOTAL_WIDTH)
                                {
                                    if (map[y * TOTAL_WIDTH + x] == WALL)
                                        map[y * TOTAL_WIDTH + x] = EMPTY;
                                }
                            }
                        }
                    }
                    else if (nY < roomY) // Верхняя комната
                    {
                        int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH / 2;
                        int doorY = roomY * ROOM_HEIGHT;

                        for (int y = doorY - 2; y <= doorY + 1; y++)
                        {
                            for (int x = doorX - 1; x <= doorX + 1; x++)
                            {
                                if (y >= 0 && y < TOTAL_HEIGHT && x >= 0 && x < TOTAL_WIDTH)
                                {
                                    if (map[y * TOTAL_WIDTH + x] == WALL)
                                        map[y * TOTAL_WIDTH + x] = EMPTY;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    delete[] visited;
}

void PlaceDoors()
{
    for (int roomY = 0; roomY < ROOMS_Y; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X; roomX++)
        {
            PlaceDoorsInRoom(roomX, roomY);
        }
    }

    // Гарантируем минимум одну свободную дверь в каждой комнате
    for (int roomY = 0; roomY < ROOMS_Y; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X; roomX++)
        {
            int startX = roomX * ROOM_WIDTH;
            int startY = roomY * ROOM_HEIGHT;

            bool hasFree = false;
            std::vector<std::pair<int, int>> doorCells;

            // Собираем все ячейки дверей в комнате
            for (int y = startY; y < startY + ROOM_HEIGHT; y++)
            {
                for (int x = startX; x < startX + ROOM_WIDTH; x++)
                {
                    if (map[y * TOTAL_WIDTH + x] == FREE_DOOR)
                    {
                        hasFree = true;
                        break;
                    }
                    if (map[y * TOTAL_WIDTH + x] == DOOR)
                    {
                        doorCells.push_back({ x, y });
                    }
                }
                if (hasFree) break;
            }

            // Если нет свободной двери, заменяем одну
            if (!hasFree && doorCells.size() > 0)
            {
                int idx = rand() % doorCells.size();
                int x = doorCells[idx].first;
                int y = doorCells[idx].second;
                map[y * TOTAL_WIDTH + x] = FREE_DOOR;
            }
        }
    }
}

void PlaceDoorsInRoom(int roomX, int roomY)
{
    int startX = roomX * ROOM_WIDTH;
    int startY = roomY * ROOM_HEIGHT;

    // Правая дверь (вертикальная 1x3)
    if (roomX < ROOMS_X - 1)
    {
        int doorX = startX + ROOM_WIDTH - 1;
        int doorY = startY + ROOM_HEIGHT / 2;
        int doorType = (rand() % 100 < 60) ? DOOR : FREE_DOOR;

        for (int dy = -1; dy <= 1; dy++)
        {
            int y = doorY + dy;
            if (y >= 0 && y < TOTAL_HEIGHT)
            {
                if (map[y * TOTAL_WIDTH + doorX] == EMPTY)
                    map[y * TOTAL_WIDTH + doorX] = doorType;
            }
        }
    }

    // Нижняя дверь (горизонтальная 3x1)
    if (roomY < ROOMS_Y - 1)
    {
        int doorX = startX + ROOM_WIDTH / 2;
        int doorY = startY + ROOM_HEIGHT - 1;
        int doorType = (rand() % 100 < 60) ? DOOR : FREE_DOOR;

        for (int dx = -1; dx <= 1; dx++)
        {
            int x = doorX + dx;
            if (x >= 0 && x < TOTAL_WIDTH)
            {
                if (map[doorY * TOTAL_WIDTH + x] == EMPTY)
                    map[doorY * TOTAL_WIDTH + x] = doorType;
            }
        }
    }

    // Верхняя дверь (горизонтальная 3x1)
    if (roomY > 0)
    {
        int doorX = startX + ROOM_WIDTH / 2;
        int doorY = startY;
        int doorType = (rand() % 100 < 60) ? DOOR : FREE_DOOR;

        for (int dx = -1; dx <= 1; dx++)
        {
            int x = doorX + dx;
            if (x >= 0 && x < TOTAL_WIDTH)
            {
                if (map[doorY * TOTAL_WIDTH + x] == EMPTY)
                    map[doorY * TOTAL_WIDTH + x] = doorType;
            }
        }
    }

    // Левая дверь (вертикальная 1x3)
    if (roomX > 0)
    {
        int doorX = startX;
        int doorY = startY + ROOM_HEIGHT / 2;
        int doorType = (rand() % 100 < 60) ? DOOR : FREE_DOOR;

        for (int dy = -1; dy <= 1; dy++)
        {
            int y = doorY + dy;
            if (y >= 0 && y < TOTAL_HEIGHT)
            {
                if (map[y * TOTAL_WIDTH + doorX] == EMPTY)
                    map[y * TOTAL_WIDTH + doorX] = doorType;
            }
        }
    }
}

void PlaceGoldAndItems()
{
    int healthCount = 8;
    if (selectedDifficulty == 2) healthCount = 5;
    if (selectedDifficulty == 3) healthCount = 3;

    for (int i = 0; i < 30; i++)
    {
        int roomX = rand() % ROOMS_X;
        int roomY = rand() % ROOMS_Y;
        int x = roomX * ROOM_WIDTH + 2 + rand() % (ROOM_WIDTH - 4);
        int y = roomY * ROOM_HEIGHT + 2 + rand() % (ROOM_HEIGHT - 4);

        if (map[y * TOTAL_WIDTH + x] == EMPTY)
            map[y * TOTAL_WIDTH + x] = GOLD;
    }

    for (int i = 0; i < healthCount; i++)
    {
        int roomX = rand() % ROOMS_X;
        int roomY = rand() % ROOMS_Y;
        int x = roomX * ROOM_WIDTH + 2 + rand() % (ROOM_WIDTH - 4);
        int y = roomY * ROOM_HEIGHT + 2 + rand() % (ROOM_HEIGHT - 4);

        if (map[y * TOTAL_WIDTH + x] == EMPTY)
            map[y * TOTAL_WIDTH + x] = HEALTH;
    }

    for (int i = 0; i < 12; i++)
    {
        int roomX = rand() % ROOMS_X;
        int roomY = rand() % ROOMS_Y;
        int x = roomX * ROOM_WIDTH + 2 + rand() % (ROOM_WIDTH - 4);
        int y = roomY * ROOM_HEIGHT + 2 + rand() % (ROOM_HEIGHT - 4);

        if (map[y * TOTAL_WIDTH + x] == EMPTY)
            map[y * TOTAL_WIDTH + x] = TRAP;
    }

    int keyCount = 3 + rand() % 3;
    for (int i = 0; i < keyCount; i++)
    {
        int roomX = rand() % ROOMS_X;
        int roomY = rand() % ROOMS_Y;
        int x = roomX * ROOM_WIDTH + 2 + rand() % (ROOM_WIDTH - 4);
        int y = roomY * ROOM_HEIGHT + 2 + rand() % (ROOM_HEIGHT - 4);

        if (map[y * TOTAL_WIDTH + x] == EMPTY)
            map[y * TOTAL_WIDTH + x] = KEY;
    }
}

void SpawnPlayer()
{
    int spawnX = currentRoomX * ROOM_WIDTH + ROOM_WIDTH / 2;
    int spawnY = currentRoomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;

    for (int radius = 0; radius < 3; radius++)
    {
        for (int dy = -radius; dy <= radius; dy++)
        {
            for (int dx = -radius; dx <= radius; dx++)
            {
                int x = spawnX + dx;
                int y = spawnY + dy;

                if (x >= 0 && x < TOTAL_WIDTH && y >= 0 && y < TOTAL_HEIGHT)
                {
                    if (map[y * TOTAL_WIDTH + x] == EMPTY)
                    {
                        map[y * TOTAL_WIDTH + x] = PLAYER;
                        return;
                    }
                }
            }
        }
    }

    map[spawnY * TOTAL_WIDTH + spawnX] = PLAYER;
}

void VisitRoom(int roomX, int roomY)
{
    if (roomX >= 0 && roomX < ROOMS_X && roomY >= 0 && roomY < ROOMS_Y)
    {
        if (!explored[roomY * ROOMS_X + roomX])
        {
            explored[roomY * ROOMS_X + roomX] = true;
            visitedRooms++;
        }
    }
}

void RemoveDoorInBothRooms(int x, int y, int dx, int dy)
{
    // Удаляем дверь в текущей клетке
    map[y * TOTAL_WIDTH + x] = EMPTY;

    // Удаляем дверь в соседней комнате
    int neighborX = x + dx;
    int neighborY = y + dy;

    if (neighborX >= 0 && neighborX < TOTAL_WIDTH && neighborY >= 0 && neighborY < TOTAL_HEIGHT)
    {
        if (map[neighborY * TOTAL_WIDTH + neighborX] == DOOR)
        {
            map[neighborY * TOTAL_WIDTH + neighborX] = EMPTY;
        }
    }

    // Удаляем все смежные ячейки двери в соседней комнате (3x1 или 1x3)
    if (dx != 0) // Вертикальная дверь, ищем соседей по Y
    {
        for (int dy2 = -1; dy2 <= 1; dy2++)
        {
            int ny = y + dy2;
            if (ny >= 0 && ny < TOTAL_HEIGHT && map[ny * TOTAL_WIDTH + neighborX] == DOOR)
            {
                map[ny * TOTAL_WIDTH + neighborX] = EMPTY;
            }
        }
    }
    else // Горизонтальная дверь, ищем соседей по X
    {
        for (int dx2 = -1; dx2 <= 1; dx2++)
        {
            int nx = x + dx2;
            if (nx >= 0 && nx < TOTAL_WIDTH && map[neighborY * TOTAL_WIDTH + nx] == DOOR)
            {
                map[neighborY * TOTAL_WIDTH + nx] = EMPTY;
            }
        }
    }
}

void ChangeRoom(int dx, int dy)
{
    int newRoomX = currentRoomX + dx;
    int newRoomY = currentRoomY + dy;

    if (newRoomX >= 0 && newRoomX < ROOMS_X &&
        newRoomY >= 0 && newRoomY < ROOMS_Y)
    {
        currentRoomX = newRoomX;
        currentRoomY = newRoomY;

        VisitRoom(currentRoomX, currentRoomY);

        POINT playerPos = FindPlayer();
        if (playerPos.x != -1 && playerPos.y != -1)
        {
            map[playerPos.y * TOTAL_WIDTH + playerPos.x] = EMPTY;
        }

        int newPlayerX, newPlayerY;

        if (dx == 1)
        {
            newPlayerX = newRoomX * ROOM_WIDTH + 1;
            newPlayerY = newRoomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
        }
        else if (dx == -1)
        {
            newPlayerX = newRoomX * ROOM_WIDTH + ROOM_WIDTH - 2;
            newPlayerY = newRoomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
        }
        else if (dy == 1)
        {
            newPlayerX = newRoomX * ROOM_WIDTH + ROOM_WIDTH / 2;
            newPlayerY = newRoomY * ROOM_HEIGHT + 1;
        }
        else
        {
            newPlayerX = newRoomX * ROOM_WIDTH + ROOM_WIDTH / 2;
            newPlayerY = newRoomY * ROOM_HEIGHT + ROOM_HEIGHT - 2;
        }

        map[newPlayerY * TOTAL_WIDTH + newPlayerX] = PLAYER;
    }
}

POINT FindPlayer()
{
    POINT playerPos = { -1, -1 };
    for (int y = 0; y < TOTAL_HEIGHT; y++)
    {
        for (int x = 0; x < TOTAL_WIDTH; x++)
        {
            if (map[y * TOTAL_WIDTH + x] == PLAYER)
            {
                playerPos.x = x;
                playerPos.y = y;
                return playerPos;
            }
        }
    }
    return playerPos;
}

void DrawMap(HDC hdc)
{
    HBRUSH brushes[11];
    brushes[EMPTY] = CreateSolidBrush(RGB(30, 30, 40));
    brushes[PLAYER] = CreateSolidBrush(RGB(0, 255, 0));
    brushes[WALL] = CreateSolidBrush(RGB(100, 70, 50));
    brushes[GOLD] = CreateSolidBrush(RGB(255, 215, 0));
    brushes[DOOR] = CreateSolidBrush(RGB(160, 120, 80));
    brushes[HEALTH] = CreateSolidBrush(RGB(255, 50, 50));
    brushes[TRAP] = CreateSolidBrush(RGB(180, 0, 180));
    brushes[UNEXPLORED] = CreateSolidBrush(RGB(20, 20, 25));
    brushes[KEY] = CreateSolidBrush(RGB(200, 200, 0));
    brushes[EXIT_DOOR] = CreateSolidBrush(RGB(0, 200, 200));
    brushes[FREE_DOOR] = CreateSolidBrush(RGB(100, 150, 160));

    HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // Вычисляем смещение камеры
    int cameraOffsetX = 0;
    int cameraOffsetY = 0;

    if (ROOMS_X > 1)
    {
        int mapViewWidth = TOTAL_WIDTH * CELL_SIZE;
        int screenWidth = bufferWidth - UI_WIDTH - MARGIN * 2;

        if (mapViewWidth > screenWidth)
        {
            int roomWidth = ROOM_WIDTH * CELL_SIZE;
            int cameraX = currentRoomX * roomWidth;
            int maxOffset = mapViewWidth - screenWidth;

            if (cameraX + roomWidth > screenWidth)
            {
                cameraOffsetX = cameraX + roomWidth - screenWidth + CELL_SIZE * 2;
            }

            if (cameraOffsetX < 0) cameraOffsetX = 0;
            if (cameraOffsetX > maxOffset) cameraOffsetX = maxOffset;
        }
    }

    if (ROOMS_Y > 1)
    {
        int mapViewHeight = TOTAL_HEIGHT * CELL_SIZE;
        int screenHeight = bufferHeight - MARGIN * 2;

        if (mapViewHeight > screenHeight)
        {
            int roomHeight = ROOM_HEIGHT * CELL_SIZE;
            int cameraY = currentRoomY * roomHeight;
            int maxOffset = mapViewHeight - screenHeight;

            if (cameraY + roomHeight > screenHeight)
            {
                cameraOffsetY = cameraY + roomHeight - screenHeight + CELL_SIZE * 2;
            }

            if (cameraOffsetY < 0) cameraOffsetY = 0;
            if (cameraOffsetY > maxOffset) cameraOffsetY = maxOffset;
        }
    }

    for (int y = 0; y < TOTAL_HEIGHT; y++)
    {
        for (int x = 0; x < TOTAL_WIDTH; x++)
        {
            int roomX = x / ROOM_WIDTH;
            int roomY = y / ROOM_HEIGHT;

            int cellType = map[y * TOTAL_WIDTH + x];
            bool isCurrentRoom = (roomX == currentRoomX && roomY == currentRoomY);

            if (!explored[roomY * ROOMS_X + roomX] && !isCurrentRoom)
            {
                cellType = UNEXPLORED;
            }

            int x1 = MARGIN + x * CELL_SIZE - cameraOffsetX;
            int y1 = MARGIN + y * CELL_SIZE - cameraOffsetY;
            int x2 = x1 + CELL_SIZE;
            int y2 = y1 + CELL_SIZE;

            // Пропускаем ячейки за пределами экрана
            if (x2 < MARGIN || x1 > bufferWidth - UI_WIDTH || y2 < MARGIN || y1 > bufferHeight)
                continue;

            RECT cellRect = { x1, y1, x2, y2 };
            FillRect(hdc, &cellRect, brushes[cellType]);

            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 60));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

            MoveToEx(hdc, x1, y1, NULL);
            LineTo(hdc, x2, y1);
            LineTo(hdc, x2, y2);
            LineTo(hdc, x1, y2);
            LineTo(hdc, x1, y1);

            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            if (cellType != UNEXPLORED)
            {
                wchar_t symbol = L' ';

                switch (cellType)
                {
                case PLAYER: symbol = L'@'; break;
                case WALL: symbol = L'#'; break;
                case GOLD: symbol = L'$'; break;
                case DOOR: symbol = L'+'; break;
                case FREE_DOOR: symbol = L'-'; break;
                case HEALTH: symbol = L'♥'; break;
                case TRAP: symbol = L'^'; break;
                case KEY: symbol = L'K'; break;
                case EXIT_DOOR: symbol = L'E'; break;
                }

                if (symbol != L' ')
                {
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(255, 255, 255));

                    SIZE textSize;
                    GetTextExtentPoint32(hdc, &symbol, 1, &textSize);
                    TextOut(hdc, x1 + (CELL_SIZE - textSize.cx) / 2,
                        y1 + (CELL_SIZE - textSize.cy) / 2,
                        &symbol, 1);
                }
            }
            else
            {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(100, 100, 100));

                wchar_t symbol = L'?';
                SIZE textSize;
                GetTextExtentPoint32(hdc, &symbol, 1, &textSize);
                TextOut(hdc, x1 + (CELL_SIZE - textSize.cx) / 2,
                    y1 + (CELL_SIZE - textSize.cy) / 2,
                    &symbol, 1);
            }
        }
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    for (int i = 0; i < 11; i++)
    {
        DeleteObject(brushes[i]);
    }
}

void DrawUI(HDC hdc)
{
    int mapWidth = TOTAL_WIDTH * CELL_SIZE;
    int uiX = mapWidth + MARGIN * 2;
    int uiY = MARGIN;

    if (!uiExpanded)
    {
        RECT buttonRect = { uiX, uiY, uiX + 30, uiY + 30 };
        HBRUSH hBrush = CreateSolidBrush(RGB(100, 100, 150));
        FillRect(hdc, &buttonRect, hBrush);
        DeleteObject(hBrush);

        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, buttonRect.left, buttonRect.top, buttonRect.right, buttonRect.bottom);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        HFONT hFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        TextOut(hdc, uiX + 8, uiY + 5, L"I", 1);

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        return;
    }

    HFONT hFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hSmallFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT uiRect = { uiX - 10, uiY - 10, uiX + UI_WIDTH, bufferHeight };
    HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 245));
    FillRect(hdc, &uiRect, hBrush);
    DeleteObject(hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(100, 100, 150));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, uiRect.left, uiRect.top, uiRect.right, uiRect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);

    SetTextColor(hdc, RGB(50, 50, 150));
    TextOut(hdc, uiX, uiY, L"СТАТИСТИКА", 11);

    uiY += 30;
    SetTextColor(hdc, RGB(200, 50, 50));

    std::wstring healthStr = L"HP: " + std::to_wstring(health) + L"/100";
    if (godMode) healthStr += L" [GOD]";
    TextOut(hdc, uiX, uiY, healthStr.c_str(), (int)healthStr.length());

    int healthBarWidth = 130;
    int currentHealthWidth = (health * healthBarWidth) / 100;

    RECT healthBarBg = { uiX, uiY + 20, uiX + healthBarWidth, uiY + 30 };
    RECT healthBarFg = { uiX, uiY + 20, uiX + currentHealthWidth, uiY + 30 };

    HBRUSH hRedBrush = CreateSolidBrush(RGB(255, 50, 50));
    HBRUSH hGreenBrush = CreateSolidBrush(RGB(50, 255, 100));

    FillRect(hdc, &healthBarBg, hRedBrush);
    FillRect(hdc, &healthBarFg, hGreenBrush);

    DeleteObject(hRedBrush);
    DeleteObject(hGreenBrush);

    uiY += 45;
    SetTextColor(hdc, RGB(30, 30, 30));
    SelectObject(hdc, hSmallFont);

    std::wstring goldStr = L"Золото: " + std::to_wstring(gold);
    TextOut(hdc, uiX, uiY, goldStr.c_str(), (int)goldStr.length());

    uiY += 25;
    std::wstring keysStr = L"Ключи: " + std::to_wstring(keys);
    TextOut(hdc, uiX, uiY, keysStr.c_str(), (int)keysStr.length());

    uiY += 25;
    std::wstring stepsStr = L"Шаги: " + std::to_wstring(steps);
    TextOut(hdc, uiX, uiY, stepsStr.c_str(), (int)stepsStr.length());

    uiY += 25;
    std::wstring roomStr = L"Комната: [" + std::to_wstring(currentRoomX + 1) +
        L"," + std::to_wstring(currentRoomY + 1) + L"]";
    TextOut(hdc, uiX, uiY, roomStr.c_str(), (int)roomStr.length());

    uiY += 25;
    std::wstring visitedStr = L"Комнат: " + std::to_wstring(visitedRooms) +
        L"/" + std::to_wstring(totalRooms);
    TextOut(hdc, uiX, uiY, visitedStr.c_str(), (int)visitedStr.length());

    uiY += 40;
    std::wstring fpsStr = L"FPS: " + std::to_wstring((int)currentFPS);
    TextOut(hdc, uiX, uiY, fpsStr.c_str(), (int)fpsStr.length());

    uiY += 45;
    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(50, 50, 100));
    TextOut(hdc, uiX, uiY, L"ЛЕГЕНДА", 7);

    uiY += 25;
    SetTextColor(hdc, RGB(30, 30, 30));
    SelectObject(hdc, hSmallFont);

    TextOut(hdc, uiX, uiY, L"@ - Вы", 6);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"$ - Золото", 10);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"K - Ключ", 8);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"+ - Дверь (ключ)", 16);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"- - Дверь (свободна)", 20);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"♥ - Лечение", 12);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"^ - Ловушка", 11);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"E - Выход", 9);

    uiY += 30;
    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(50, 50, 100));
    TextOut(hdc, uiX, uiY, L"УПРАВЛЕНИЕ", 10);

    uiY += 25;
    SetTextColor(hdc, RGB(30, 30, 30));
    SelectObject(hdc, hSmallFont);

    TextOut(hdc, uiX, uiY, L"<-> Движение", 12);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"I - Меню", 8);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"G - Режим Бога", 15);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"R - Новая игра", 14);
    uiY += 18;
    TextOut(hdc, uiX, uiY, L"ESC - Выход", 11);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hSmallFont);
}

void DrawMenu(HDC hdc)
{
    HFONT hTitleFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hFont = CreateFont(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");

    HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);

    RECT bgRect = { 0, 0, 960, 680 };
    HBRUSH hBgBrush = CreateSolidBrush(RGB(20, 20, 40));
    FillRect(hdc, &bgRect, hBgBrush);
    DeleteObject(hBgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 255, 100));

    wchar_t title[] = L"Лабиринт";
    SIZE titleSize;
    GetTextExtentPoint32(hdc, title, (int)wcslen(title), &titleSize);
    TextOut(hdc, (960 - titleSize.cx) / 2, 50, title, (int)wcslen(title));

    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(200, 200, 200));

    wchar_t subtitle[] = L"Побегите от Зло_Трактора!";
    GetTextExtentPoint32(hdc, subtitle, (int)wcslen(subtitle), &titleSize);
    TextOut(hdc, (960 - titleSize.cx) / 2, 150, subtitle, (int)wcslen(subtitle));

    SetTextColor(hdc, RGB(100, 200, 255));
    wchar_t pressKey[] = L"Нажмите ENTER для выбора сложности";
    GetTextExtentPoint32(hdc, pressKey, (int)wcslen(pressKey), &titleSize);
    TextOut(hdc, (960 - titleSize.cx) / 2, 350, pressKey, (int)wcslen(pressKey));

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hTitleFont);
}

void DrawDifficultySelect(HDC hdc)
{
    HFONT hTitleFont = CreateFont(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hFont = CreateFont(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");

    HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);

    RECT bgRect = { 0, 0, 960, 680 };
    HBRUSH hBgBrush = CreateSolidBrush(RGB(20, 20, 40));
    FillRect(hdc, &bgRect, hBgBrush);
    DeleteObject(hBgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 255, 100));

    wchar_t title[] = L"Выберите сложность";
    SIZE titleSize;
    GetTextExtentPoint32(hdc, title, (int)wcslen(title), &titleSize);
    TextOut(hdc, (960 - titleSize.cx) / 2, 50, title, (int)wcslen(title));

    SelectObject(hdc, hFont);

    int difficulties[3][2] = {
        {200, 200},
        {200, 300},
        {200, 400}
    };

    const wchar_t* diffNames[] = { L"1. ЛЕГКО (3x3)", L"2. НОРМАЛЬНО (4x4)", L"3. СЛОЖНО (5x5)" };
    const wchar_t* diffDescs[] = {
        L"Много хилок, мало ловушек",
        L"Сбалансировано",
        L"Мало хилок, много ловушек"
    };

    for (int i = 0; i < 3; i++)
    {
        if (i + 1 == selectedDifficulty)
            SetTextColor(hdc, RGB(255, 100, 100));
        else
            SetTextColor(hdc, RGB(200, 200, 200));

        TextOut(hdc, difficulties[i][0], difficulties[i][1], diffNames[i], (int)wcslen(diffNames[i]));

        SetTextColor(hdc, RGB(150, 150, 150));
        TextOut(hdc, difficulties[i][0] + 40, difficulties[i][1] + 40, diffDescs[i], (int)wcslen(diffDescs[i]));
    }

    SetTextColor(hdc, RGB(100, 200, 255));
    SelectObject(hdc, hTitleFont);
    wchar_t hint[] = L"Нажмите ENTER для старта";
    GetTextExtentPoint32(hdc, hint, (int)wcslen(hint), &titleSize);
    TextOut(hdc, (960 - titleSize.cx) / 2, 550, hint, (int)wcslen(hint));

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hTitleFont);
}

void DrawGameOver(HDC hdc)
{
    HFONT hFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hSmallFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT bgRect = { 0, 0, 960, 680 };
    HBRUSH hBgBrush = CreateSolidBrush(RGB(40, 20, 20));
    FillRect(hdc, &bgRect, hBgBrush);
    DeleteObject(hBgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 50, 50));

    wchar_t gameOverText[] = L"ВОДА НА ТРАКТОРЕ!";
    SIZE textSize;
    GetTextExtentPoint32(hdc, gameOverText, (int)wcslen(gameOverText), &textSize);
    TextOut(hdc, (960 - textSize.cx) / 2, 150, gameOverText, (int)wcslen(gameOverText));

    SelectObject(hdc, hSmallFont);
    SetTextColor(hdc, RGB(200, 200, 200));

    std::wstring stats = L"Золото собрано: " + std::to_wstring(gold) +
        L" | Шагов сделано: " + std::to_wstring(steps);
    GetTextExtentPoint32(hdc, stats.c_str(), (int)stats.length(), &textSize);
    TextOut(hdc, (960 - textSize.cx) / 2, 300, stats.c_str(), (int)stats.length());

    SetTextColor(hdc, RGB(100, 200, 255));
    wchar_t continueText[] = L"Нажмите ENTER для меню или R для новой игры";
    GetTextExtentPoint32(hdc, continueText, (int)wcslen(continueText), &textSize);
    TextOut(hdc, (960 - textSize.cx) / 2, 400, continueText, (int)wcslen(continueText));

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hSmallFont);
}

void DrawWinScreen(HDC hdc)
{
    HFONT hFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hSmallFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT bgRect = { 0, 0, 960, 680 };
    HBRUSH hBgBrush = CreateSolidBrush(RGB(20, 40, 20));
    FillRect(hdc, &bgRect, hBgBrush);
    DeleteObject(hBgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(50, 255, 100));

    wchar_t winText[] = L"ВЫ УБЕЖАЛИ ОТ ЗЛО_ТРАКТОРА!";
    SIZE textSize;
    GetTextExtentPoint32(hdc, winText, (int)wcslen(winText), &textSize);
    TextOut(hdc, (960 - textSize.cx) / 2, 100, winText, (int)wcslen(winText));

    SelectObject(hdc, hSmallFont);
    SetTextColor(hdc, RGB(200, 200, 200));

    std::wstring stats = L"Золото: " + std::to_wstring(gold) + L" | Шаги: " + std::to_wstring(steps);
    GetTextExtentPoint32(hdc, stats.c_str(), (int)stats.length(), &textSize);
    TextOut(hdc, (960 - textSize.cx) / 2, 250, stats.c_str(), (int)stats.length());

    std::wstring roomsStats = L"Исследовано комнат: " + std::to_wstring(visitedRooms) + L"/" + std::to_wstring(totalRooms);
    GetTextExtentPoint32(hdc, roomsStats.c_str(), (int)roomsStats.length(), &textSize);
    TextOut(hdc, (960 - textSize.cx) / 2, 300, roomsStats.c_str(), (int)roomsStats.length());

    SetTextColor(hdc, RGB(100, 200, 255));
    wchar_t continueText[] = L"Нажмите ENTER для меню";
    GetTextExtentPoint32(hdc, continueText, (int)wcslen(continueText), &textSize);
    TextOut(hdc, (960 - textSize.cx) / 2, 450, continueText, (int)wcslen(continueText));

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hSmallFont);
}

void MovePlayer(int dx, int dy)
{
    POINT playerPos = FindPlayer();
    if (playerPos.x == -1 || playerPos.y == -1)
        return;

    int newX = playerPos.x + dx;
    int newY = playerPos.y + dy;

    if (newX < 0 || newX >= TOTAL_WIDTH || newY < 0 || newY >= TOTAL_HEIGHT)
        return;

    // Посетить комнату при входе в любую клетку
    int newRoomX = newX / ROOM_WIDTH;
    int newRoomY = newY / ROOM_HEIGHT;
    VisitRoom(newRoomX, newRoomY);

    int targetCell = map[newY * TOTAL_WIDTH + newX];

    int roomX = 0, roomY = 0;
    int dxRoom = 0, dyRoom = 0;

    switch (targetCell)
    {
    case EMPTY:
        map[playerPos.y * TOTAL_WIDTH + playerPos.x] = EMPTY;
        map[newY * TOTAL_WIDTH + newX] = PLAYER;
        steps++;
        break;

    case GOLD:
        map[playerPos.y * TOTAL_WIDTH + playerPos.x] = EMPTY;
        map[newY * TOTAL_WIDTH + newX] = PLAYER;
        gold += 5 + rand() % 15;
        steps++;
        break;

    case KEY:
        map[playerPos.y * TOTAL_WIDTH + playerPos.x] = EMPTY;
        map[newY * TOTAL_WIDTH + newX] = PLAYER;
        keys++;
        steps++;
        break;

    case WALL:
        if (!godMode)
            DamagePlayer(3);
        steps++;
        break;

    case FREE_DOOR:
        map[playerPos.y * TOTAL_WIDTH + playerPos.x] = EMPTY;
        roomX = newX / ROOM_WIDTH;
        roomY = newY / ROOM_HEIGHT;

        if (roomX != currentRoomX || roomY != currentRoomY)
        {
            dxRoom = roomX - currentRoomX;
            dyRoom = roomY - currentRoomY;
            RemoveDoorInBothRooms(newX, newY, dxRoom * ROOM_WIDTH, dyRoom * ROOM_HEIGHT);
            ChangeRoom(dxRoom, dyRoom);
        }
        else
        {
            map[newY * TOTAL_WIDTH + newX] = PLAYER;
        }
        steps++;
        break;

    case DOOR:
        if (keys > 0 || godMode)
        {
            if (!godMode) keys--;
            map[playerPos.y * TOTAL_WIDTH + playerPos.x] = EMPTY;
            roomX = newX / ROOM_WIDTH;
            roomY = newY / ROOM_HEIGHT;

            if (roomX != currentRoomX || roomY != currentRoomY)
            {
                dxRoom = roomX - currentRoomX;
                dyRoom = roomY - currentRoomY;
                RemoveDoorInBothRooms(newX, newY, dxRoom * ROOM_WIDTH, dyRoom * ROOM_HEIGHT);
                ChangeRoom(dxRoom, dyRoom);
            }
            else
            {
                map[newY * TOTAL_WIDTH + newX] = PLAYER;
            }
        }
        steps++;
        break;

    case EXIT_DOOR:
        gameState = WIN_SCREEN;
        steps++;
        break;

    case HEALTH:
        map[playerPos.y * TOTAL_WIDTH + playerPos.x] = EMPTY;
        map[newY * TOTAL_WIDTH + newX] = PLAYER;
        HealPlayer(25);
        steps++;
        break;

    case TRAP:
        map[playerPos.y * TOTAL_WIDTH + playerPos.x] = EMPTY;
        map[newY * TOTAL_WIDTH + newX] = PLAYER;
        if (!godMode)
            DamagePlayer(20);
        steps++;
        break;
    }

    if (health <= 0 && !godMode)
    {
        gameState = GAME_OVER;
    }
}

void DamagePlayer(int damage)
{
    health -= damage;
    if (health < 0) health = 0;
}

void HealPlayer(int amount)
{
    health += amount;
    if (health > 100) health = 100;
}

void UpdateFPS()
{
    static DWORD lastTime = GetTickCount();
    static int frames = 0;

    DWORD currentTime = GetTickCount();
    frames++;

    if (currentTime - lastTime >= 1000)
    {
        currentFPS = (float)frames;
        frames = 0;
        lastTime = currentTime;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (gameState == PLAYING && hdcBuffer)
        {
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            if (bufferWidth != rcClient.right || bufferHeight != rcClient.bottom)
            {
                CreateBuffer(rcClient.right, rcClient.bottom);
            }

            // Очищаем буфер
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdcBuffer, &rcClient, hBrush);
            DeleteObject(hBrush);

            DrawMap(hdcBuffer);
            DrawUI(hdcBuffer);

            BitBlt(hdc, 0, 0, bufferWidth, bufferHeight, hdcBuffer, 0, 0, SRCCOPY);
        }
        else
        {
            switch (gameState)
            {
            case MENU:
                DrawMenu(hdc);
                break;
            case DIFFICULTY_SELECT:
                DrawDifficultySelect(hdc);
                break;
            case GAME_OVER:
                DrawGameOver(hdc);
                break;
            case WIN_SCREEN:
                DrawWinScreen(hdc);
                break;
            }
        }

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_KEYDOWN:
    {
        switch (gameState)
        {
        case MENU:
            if (wParam == VK_RETURN)
            {
                selectedDifficulty = 1;
                gameState = DIFFICULTY_SELECT;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;

        case DIFFICULTY_SELECT:
            if (wParam == VK_UP && selectedDifficulty > 1)
            {
                selectedDifficulty--;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (wParam == VK_DOWN && selectedDifficulty < 3)
            {
                selectedDifficulty++;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (wParam == VK_RETURN)
            {
                StartNewGame(selectedDifficulty);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;

        case PLAYING:
        {
            switch (wParam)
            {
            case VK_LEFT:
                MovePlayer(-1, 0);
                InvalidateRect(hWnd, NULL, FALSE);
                break;

            case VK_RIGHT:
                MovePlayer(1, 0);
                InvalidateRect(hWnd, NULL, FALSE);
                break;

            case VK_UP:
                MovePlayer(0, -1);
                InvalidateRect(hWnd, NULL, FALSE);
                break;

            case VK_DOWN:
                MovePlayer(0, 1);
                InvalidateRect(hWnd, NULL, FALSE);
                break;

            case 'I':
                uiExpanded = !uiExpanded;
                InvalidateRect(hWnd, NULL, FALSE);
                break;

            case 'G':
                godMode = !godMode;
                if (godMode) health = 100;
                InvalidateRect(hWnd, NULL, FALSE);
                break;

            case 'R':
                gameState = DIFFICULTY_SELECT;
                selectedDifficulty = 1;
                InvalidateRect(hWnd, NULL, FALSE);
                break;

            case VK_ESCAPE:
                gameState = MENU;
                InvalidateRect(hWnd, NULL, FALSE);
                break;
            }
        }
        break;

        case GAME_OVER:
            if (wParam == VK_RETURN)
            {
                gameState = MENU;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (wParam == 'R')
            {
                gameState = DIFFICULTY_SELECT;
                selectedDifficulty = 1;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;

        case WIN_SCREEN:
            if (wParam == VK_RETURN)
            {
                gameState = MENU;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
    }
    break;

    case WM_TIMER:
        UpdateFPS();
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    case WM_CREATE:
        SetTimer(hWnd, 1, 16, NULL);
        break;

    case WM_SIZE:
    {
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        if (gameState == PLAYING)
        {
            CreateBuffer(rcClient.right, rcClient.bottom);
        }
    }
    break;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
