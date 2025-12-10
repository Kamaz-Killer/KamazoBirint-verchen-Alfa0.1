#define _CRT_SECURE_NO_WARNINGS
#include "framework.h"
#include "8.2.2.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>

#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

// Игровые константы и переменные
#define ROOM_WIDTH 15      // Уменьшил для лучшего обзора
#define ROOM_HEIGHT 10     // Уменьшил для лучшего обзора
#define ROOMS_X 3
#define ROOMS_Y 3
#define TOTAL_WIDTH (ROOM_WIDTH * ROOMS_X)
#define TOTAL_HEIGHT (ROOM_HEIGHT * ROOMS_Y)
#define CELL_SIZE 32       // Увеличил размер пикселей

// Коды ячеек:
#define EMPTY 0
#define PLAYER 1
#define WALL 2
#define GOLD 3
#define DOOR 4
#define HEALTH 5
#define TRAP 6
#define UNEXPLORED 7       // Для непосещенных комнат

int map[TOTAL_HEIGHT][TOTAL_WIDTH];
bool explored[ROOMS_Y][ROOMS_X] = { false }; // Отслеживаем посещенные комнаты
int currentRoomX = 1, currentRoomY = 1;
int steps = 0;
int gold = 0;
int health = 100;
int visitedRooms = 1; // Счетчик посещенных комнат

// Прототипы функций
void GenerateMap();
void GenerateRoom(int roomX, int roomY);
void EnsurePlayerPath();
void ConnectRooms();
void PlaceDoors();
void PlaceGoldAndItems();
void SpawnPlayer();
void ChangeRoom(int dx, int dy);
POINT FindPlayer();
void DrawMap(HDC hdc);
void DrawUI(HDC hdc);
void MovePlayer(int dx, int dy);
void SaveGame();
void LoadGame();
void DamagePlayer(int damage);
void HealPlayer(int amount);
void GenerateRuins(int startX, int startY);
void CreatePathToExit(int roomX, int roomY);

// Основные функции Windows
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

    // Инициализация случайных чисел
    srand((unsigned int)time(NULL));

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MY822, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    // Генерация начальной карты
    GenerateMap();

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

    // Рассчитываем размер окна
    int windowWidth = TOTAL_WIDTH * CELL_SIZE + 220;  // + место для UI
    int windowHeight = TOTAL_HEIGHT * CELL_SIZE + 50;

    HWND hWnd = CreateWindowW(szWindowClass, L"Рогалик-приключение", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

void GenerateMap()
{
    // Очистка карты
    for (int y = 0; y < TOTAL_HEIGHT; y++)
        for (int x = 0; x < TOTAL_WIDTH; x++)
            map[y][x] = EMPTY;

    // Сбрасываем информацию о посещенных комнатах
    for (int y = 0; y < ROOMS_Y; y++)
        for (int x = 0; x < ROOMS_X; x++)
            explored[y][x] = false;

    explored[currentRoomY][currentRoomX] = true;
    visitedRooms = 1;

    // Генерация комнат
    for (int roomY = 0; roomY < ROOMS_Y; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X; roomX++)
        {
            GenerateRoom(roomX, roomY);
        }
    }

    // Соединение комнат коридорами
    ConnectRooms();

    // Размещение дверей
    PlaceDoors();

    // Обеспечиваем проходимость начальной комнаты
    EnsurePlayerPath();

    // Размещение предметов
    PlaceGoldAndItems();

    // Размещение игрока
    SpawnPlayer();
}

void GenerateRoom(int roomX, int roomY)
{
    int startX = roomX * ROOM_WIDTH;
    int startY = roomY * ROOM_HEIGHT;

    // Внешние стены комнаты
    for (int y = startY; y < startY + ROOM_HEIGHT; y++)
    {
        for (int x = startX; x < startX + ROOM_WIDTH; x++)
        {
            if (y == startY || y == startY + ROOM_HEIGHT - 1 ||
                x == startX || x == startX + ROOM_WIDTH - 1)
            {
                map[y][x] = WALL;
            }
        }
    }

    // Генерация руин в комнате
    GenerateRuins(startX, startY);
}

void EnsurePlayerPath()
{
    int startX = currentRoomX * ROOM_WIDTH;
    int startY = currentRoomY * ROOM_HEIGHT;

    // Гарантируем, что от дверей есть путь
    CreatePathToExit(currentRoomX, currentRoomY);

    // Проверяем и очищаем область вокруг дверей
    int doorPositions[4][2] = {
        {startX + ROOM_WIDTH / 2, startY},                    // Верхняя дверь
        {startX + ROOM_WIDTH / 2, startY + ROOM_HEIGHT - 1},  // Нижняя дверь
        {startX, startY + ROOM_HEIGHT / 2},                   // Левая дверь
        {startX + ROOM_WIDTH - 1, startY + ROOM_HEIGHT / 2}   // Правая дверь
    };

    for (int i = 0; i < 4; i++)
    {
        int x = doorPositions[i][0];
        int y = doorPositions[i][1];

        // Очищаем 3x3 область вокруг двери
        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dx = -1; dx <= 1; dx++)
            {
                int nx = x + dx;
                int ny = y + dy;

                if (nx >= 0 && nx < TOTAL_WIDTH && ny >= 0 && ny < TOTAL_HEIGHT)
                {
                    if (map[ny][nx] == WALL && !(nx == x && ny == y))
                    {
                        map[ny][nx] = EMPTY;
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

    // Создаем пути от центра ко всем дверям
    int doors[4][3] = {
        {startX, roomY * ROOM_HEIGHT, 0},                     // Вверх
        {startX, roomY * ROOM_HEIGHT + ROOM_HEIGHT - 1, 1},   // Вниз
        {roomX * ROOM_WIDTH, startY, 2},                      // Влево
        {roomX * ROOM_WIDTH + ROOM_WIDTH - 1, startY, 3}      // Вправо
    };

    for (int i = 0; i < 4; i++)
    {
        int targetX = doors[i][0];
        int targetY = doors[i][1];
        int direction = doors[i][2];

        int currentX = startX;
        int currentY = startY;

        // Прямой путь к двери
        while (currentX != targetX || currentY != targetY)
        {
            // Очищаем текущую позицию
            if (map[currentY][currentX] == WALL)
                map[currentY][currentX] = EMPTY;

            // Двигаемся к цели
            if (currentX < targetX) currentX++;
            else if (currentX > targetX) currentX--;
            else if (currentY < targetY) currentY++;
            else if (currentY > targetY) currentY--;
        }

        // Очищаем саму дверь
        if (map[targetY][targetX] == WALL)
            map[targetY][targetX] = EMPTY;
    }
}

void GenerateRuins(int startX, int startY)
{
    // Создаем несколько разрушенных стен
    int numWalls = rand() % 5 + 3; // От 3 до 7 стен

    for (int w = 0; w < numWalls; w++)
    {
        int wallLength = rand() % 4 + 2; // Длина стены 2-5 клеток
        int startWallX = startX + 2 + rand() % (ROOM_WIDTH - 5);
        int startWallY = startY + 2 + rand() % (ROOM_HEIGHT - 5);
        int direction = rand() % 2; // 0 - горизонтальная, 1 - вертикальная

        for (int l = 0; l < wallLength; l++)
        {
            int x = startWallX + (direction == 0 ? l : 0);
            int y = startWallY + (direction == 1 ? l : 0);

            // Проверяем границы комнаты
            if (x >= startX + 1 && x < startX + ROOM_WIDTH - 1 &&
                y >= startY + 1 && y < startY + ROOM_HEIGHT - 1)
            {
                // Делаем стену несплошной (пропускаем некоторые клетки)
                if (rand() % 100 < 80) // 80% шанс поставить стену
                {
                    map[y][x] = WALL;
                }
            }
        }
    }

    // Добавляем отдельные обломки
    int debrisCount = rand() % 8 + 5;
    for (int d = 0; d < debrisCount; d++)
    {
        int x = startX + 1 + rand() % (ROOM_WIDTH - 2);
        int y = startY + 1 + rand() % (ROOM_HEIGHT - 2);

        if (map[y][x] == EMPTY)
        {
            map[y][x] = WALL;
        }
    }
}

void ConnectRooms()
{
    // Горизонтальные соединения
    for (int roomY = 0; roomY < ROOMS_Y; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X - 1; roomX++)
        {
            if (rand() % 100 < 90) // 90% шанс соединения комнат
            {
                int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
                int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH - 1;

                // Создаем проход
                for (int y = doorY - 1; y <= doorY + 1; y++)
                {
                    for (int x = doorX; x <= doorX + 2; x++)
                    {
                        if (y >= 0 && y < TOTAL_HEIGHT && x >= 0 && x < TOTAL_WIDTH)
                        {
                            map[y][x] = EMPTY;
                        }
                    }
                }
            }
        }
    }

    // Вертикальные соединения
    for (int roomY = 0; roomY < ROOMS_Y - 1; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X; roomX++)
        {
            if (rand() % 100 < 90) // 90% шанс соединения комнат
            {
                int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH / 2;
                int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT - 1;

                // Создаем проход
                for (int y = doorY; y <= doorY + 2; y++)
                {
                    for (int x = doorX - 1; x <= doorX + 1; x++)
                    {
                        if (y >= 0 && y < TOTAL_HEIGHT && x >= 0 && x < TOTAL_WIDTH)
                        {
                            map[y][x] = EMPTY;
                        }
                    }
                }
            }
        }
    }
}

void PlaceDoors()
{
    for (int roomY = 0; roomY < ROOMS_Y; roomY++)
    {
        for (int roomX = 0; roomX < ROOMS_X; roomX++)
        {
            // Правая дверь
            if (roomX < ROOMS_X - 1)
            {
                int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH - 1;
                int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
                map[doorY][doorX] = DOOR;
            }

            // Нижняя дверь
            if (roomY < ROOMS_Y - 1)
            {
                int doorX = roomX * ROOM_WIDTH + ROOM_WIDTH / 2;
                int doorY = roomY * ROOM_HEIGHT + ROOM_HEIGHT - 1;
                map[doorY][doorX] = DOOR;
            }
        }
    }
}

void PlaceGoldAndItems()
{
    // Размещаем золото
    for (int i = 0; i < 30; i++)
    {
        int roomX = rand() % ROOMS_X;
        int roomY = rand() % ROOMS_Y;
        int x = roomX * ROOM_WIDTH + 2 + rand() % (ROOM_WIDTH - 4);
        int y = roomY * ROOM_HEIGHT + 2 + rand() % (ROOM_HEIGHT - 4);

        if (map[y][x] == EMPTY)
            map[y][x] = GOLD;
    }

    // Размещаем аптечки
    for (int i = 0; i < 8; i++)
    {
        int roomX = rand() % ROOMS_X;
        int roomY = rand() % ROOMS_Y;
        int x = roomX * ROOM_WIDTH + 2 + rand() % (ROOM_WIDTH - 4);
        int y = roomY * ROOM_HEIGHT + 2 + rand() % (ROOM_HEIGHT - 4);

        if (map[y][x] == EMPTY)
            map[y][x] = HEALTH;
    }

    // Размещаем ловушки
    for (int i = 0; i < 12; i++)
    {
        int roomX = rand() % ROOMS_X;
        int roomY = rand() % ROOMS_Y;
        int x = roomX * ROOM_WIDTH + 2 + rand() % (ROOM_WIDTH - 4);
        int y = roomY * ROOM_HEIGHT + 2 + rand() % (ROOM_HEIGHT - 4);

        if (map[y][x] == EMPTY)
            map[y][x] = TRAP;
    }
}

void SpawnPlayer()
{
    // Центральная позиция в текущей комнате
    int spawnX = currentRoomX * ROOM_WIDTH + ROOM_WIDTH / 2;
    int spawnY = currentRoomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;

    // Ищем ближайшую пустую клетку
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
                    if (map[y][x] == EMPTY)
                    {
                        map[y][x] = PLAYER;
                        return;
                    }
                }
            }
        }
    }

    // Если не нашли пустую клетку, очищаем центр
    map[spawnY][spawnX] = PLAYER;
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

        // Отмечаем комнату как посещенную
        if (!explored[currentRoomY][currentRoomX])
        {
            explored[currentRoomY][currentRoomX] = true;
            visitedRooms++;
        }

        // Перемещаем игрока в новую комнату
        POINT playerPos = FindPlayer();
        if (playerPos.x != -1 && playerPos.y != -1)
        {
            map[playerPos.y][playerPos.x] = EMPTY;
        }

        // Помещаем игрока у двери в новой комнате
        int newPlayerX, newPlayerY;

        if (dx == 1) // Перешли вправо
        {
            newPlayerX = newRoomX * ROOM_WIDTH + 1;
            newPlayerY = newRoomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
        }
        else if (dx == -1) // Перешли влево
        {
            newPlayerX = newRoomX * ROOM_WIDTH + ROOM_WIDTH - 2;
            newPlayerY = newRoomY * ROOM_HEIGHT + ROOM_HEIGHT / 2;
        }
        else if (dy == 1) // Перешли вниз
        {
            newPlayerX = newRoomX * ROOM_WIDTH + ROOM_WIDTH / 2;
            newPlayerY = newRoomY * ROOM_HEIGHT + 1;
        }
        else // Перешли вверх
        {
            newPlayerX = newRoomX * ROOM_WIDTH + ROOM_WIDTH / 2;
            newPlayerY = newRoomY * ROOM_HEIGHT + ROOM_HEIGHT - 2;
        }

        map[newPlayerY][newPlayerX] = PLAYER;
    }
}

POINT FindPlayer()
{
    POINT playerPos = { -1, -1 };
    for (int y = 0; y < TOTAL_HEIGHT; y++)
    {
        for (int x = 0; x < TOTAL_WIDTH; x++)
        {
            if (map[y][x] == PLAYER)
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
    // Определяем кисти для разных типов клеток
    HBRUSH brushes[8];
    brushes[EMPTY] = CreateSolidBrush(RGB(30, 30, 40));     // Темно-синий фон
    brushes[PLAYER] = CreateSolidBrush(RGB(0, 255, 0));     // Зеленый игрок
    brushes[WALL] = CreateSolidBrush(RGB(100, 70, 50));     // Коричневые стены
    brushes[GOLD] = CreateSolidBrush(RGB(255, 215, 0));     // Золото
    brushes[DOOR] = CreateSolidBrush(RGB(160, 120, 80));    // Деревянные двери
    brushes[HEALTH] = CreateSolidBrush(RGB(255, 50, 50));   // Красное здоровье
    brushes[TRAP] = CreateSolidBrush(RGB(180, 0, 180));     // Фиолетовые ловушки
    brushes[UNEXPLORED] = CreateSolidBrush(RGB(20, 20, 25)); // Темнее для непосещенных

    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    for (int y = 0; y < TOTAL_HEIGHT; y++)
    {
        for (int x = 0; x < TOTAL_WIDTH; x++)
        {
            // Определяем, в какой комнате находится клетка
            int roomX = x / ROOM_WIDTH;
            int roomY = y / ROOM_HEIGHT;

            // Если комната не посещена, рисуем как непосещенную
            int cellType = map[y][x];
            bool isCurrentRoom = (roomX == currentRoomX && roomY == currentRoomY);

            if (!explored[roomY][roomX] && !isCurrentRoom)
            {
                cellType = UNEXPLORED;
            }

            // Определяем границы клетки
            int x1 = x * CELL_SIZE;
            int y1 = y * CELL_SIZE;
            int x2 = x1 + CELL_SIZE;
            int y2 = y1 + CELL_SIZE;

            // Заливаем клетку цветом
            RECT cellRect = { x1, y1, x2, y2 };
            FillRect(hdc, &cellRect, brushes[cellType]);

            // Рисуем границу клетки
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 60));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

            MoveToEx(hdc, x1, y1, NULL);
            LineTo(hdc, x2, y1);
            LineTo(hdc, x2, y2);
            LineTo(hdc, x1, y2);
            LineTo(hdc, x1, y1);

            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            // Символы для клеток
            if (cellType != UNEXPLORED)
            {
                wchar_t symbol = L' ';

                switch (cellType)
                {
                case PLAYER: symbol = L'@'; break;
                case WALL: symbol = L'#'; break;
                case GOLD: symbol = L'$'; break;
                case DOOR: symbol = L'+'; break;
                case HEALTH: symbol = L'♥'; break;
                case TRAP: symbol = L'^'; break;
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
                // Для непосещенных комнат - знак вопроса
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

    // Восстанавливаем старый шрифт
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    // Удаляем кисти
    for (int i = 0; i < 8; i++)
    {
        DeleteObject(brushes[i]);
    }
}

void DrawUI(HDC hdc)
{
    int uiX = TOTAL_WIDTH * CELL_SIZE + 10;
    int uiY = 10;

    // Создаем шрифт для UI
    HFONT hFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // Рисуем панель информации с фоном
    RECT uiRect = { uiX - 10, uiY - 10, uiX + 200, uiY + 280 };
    HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 245));
    FillRect(hdc, &uiRect, hBrush);
    DeleteObject(hBrush);

    // Рамка для UI
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(100, 100, 150));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, uiRect.left, uiRect.top, uiRect.right, uiRect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);

    // Заголовок
    SetTextColor(hdc, RGB(50, 50, 150));
    TextOut(hdc, uiX, uiY, L"=== ИГРОВАЯ СТАТИСТИКА ===", 26);

    // Здоровье
    uiY += 40;
    SetTextColor(hdc, RGB(200, 50, 50));

    std::wstring healthStr = L"Здоровье: " + std::to_wstring(health) + L"/100";
    TextOut(hdc, uiX, uiY, healthStr.c_str(), (int)healthStr.length());

    // Полоска здоровья
    int healthBarWidth = 150;
    int currentHealthWidth = (health * healthBarWidth) / 100;

    RECT healthBarBg = { uiX, uiY + 25, uiX + healthBarWidth, uiY + 40 };
    RECT healthBarFg = { uiX, uiY + 25, uiX + currentHealthWidth, uiY + 40 };

    HBRUSH hRedBrush = CreateSolidBrush(RGB(255, 50, 50));
    HBRUSH hGreenBrush = CreateSolidBrush(RGB(50, 255, 100));

    FillRect(hdc, &healthBarBg, hRedBrush);
    FillRect(hdc, &healthBarFg, hGreenBrush);

    DeleteObject(hRedBrush);
    DeleteObject(hGreenBrush);

    // Остальная статистика
    uiY += 60;
    SetTextColor(hdc, RGB(30, 30, 30));

    std::wstring goldStr = L"Золото: " + std::to_wstring(gold);
    TextOut(hdc, uiX, uiY, goldStr.c_str(), (int)goldStr.length());

    uiY += 30;
    std::wstring stepsStr = L"Шаги: " + std::to_wstring(steps);
    TextOut(hdc, uiX, uiY, stepsStr.c_str(), (int)stepsStr.length());

    uiY += 30;
    std::wstring roomStr = L"Комната: [" + std::to_wstring(currentRoomX + 1) +
        L"," + std::to_wstring(currentRoomY + 1) + L"]";
    TextOut(hdc, uiX, uiY, roomStr.c_str(), (int)roomStr.length());

    uiY += 30;
    std::wstring visitedStr = L"Исследовано: " + std::to_wstring(visitedRooms) +
        L"/" + std::to_wstring(ROOMS_X * ROOMS_Y);
    TextOut(hdc, uiX, uiY, visitedStr.c_str(), (int)visitedStr.length());

    // Управление
    uiY += 50;
    SetTextColor(hdc, RGB(80, 80, 80));
    TextOut(hdc, uiX, uiY, L"=== УПРАВЛЕНИЕ ===", 17);

    uiY += 30;
    TextOut(hdc, uiX, uiY, L"Стрелки - движение", 19);

    uiY += 25;
    TextOut(hdc, uiX, uiY, L"S - сохранить", 13);

    uiY += 25;
    TextOut(hdc, uiX, uiY, L"F - загрузить", 14);

    uiY += 25;
    TextOut(hdc, uiX, uiY, L"R - новая игра", 14);

    uiY += 25;
    TextOut(hdc, uiX, uiY, L"ESC - выход", 12);

    // Легенда
    uiY += 50;
    SetTextColor(hdc, RGB(50, 50, 100));
    TextOut(hdc, uiX, uiY, L"=== ЛЕГЕНДА ===", 15);

    uiY += 25;
    SetTextColor(hdc, RGB(0, 200, 0));
    TextOut(hdc, uiX, uiY, L"@ - Вы", 5);

    uiY += 20;
    SetTextColor(hdc, RGB(200, 160, 0));
    TextOut(hdc, uiX, uiY, L"$ - Золото", 10);

    uiY += 20;
    SetTextColor(hdc, RGB(200, 50, 50));
    TextOut(hdc, uiX, uiY, L"♥ - Здоровье", 12);

    uiY += 20;
    SetTextColor(hdc, RGB(180, 0, 180));
    TextOut(hdc, uiX, uiY, L"^ - Ловушка", 11);

    uiY += 20;
    SetTextColor(hdc, RGB(160, 120, 80));
    TextOut(hdc, uiX, uiY, L"+ - Дверь", 9);

    // Восстанавливаем старый шрифт
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void MovePlayer(int dx, int dy)
{
    POINT playerPos = FindPlayer();
    if (playerPos.x == -1 || playerPos.y == -1)
        return;

    int newX = playerPos.x + dx;
    int newY = playerPos.y + dy;

    // Проверяем выход за границы
    if (newX < 0 || newX >= TOTAL_WIDTH || newY < 0 || newY >= TOTAL_HEIGHT)
        return;

    // Проверяем клетку назначения
    int targetCell = map[newY][newX];

    // Объявляем переменные ДО switch
    int roomX = 0, roomY = 0;
    int dxRoom = 0, dyRoom = 0;

    switch (targetCell)
    {
    case EMPTY:
        map[playerPos.y][playerPos.x] = EMPTY;
        map[newY][newX] = PLAYER;
        steps++;
        break;

    case GOLD:
        map[playerPos.y][playerPos.x] = EMPTY;
        map[newY][newX] = PLAYER;
        gold += 5 + rand() % 15;
        steps++;
        break;

    case WALL:
        DamagePlayer(3);
        steps++;
        break;

    case DOOR:
        // Определяем направление перехода
        roomX = newX / ROOM_WIDTH;
        roomY = newY / ROOM_HEIGHT;

        if (roomX != currentRoomX || roomY != currentRoomY)
        {
            // Меняем комнату
            dxRoom = roomX - currentRoomX;
            dyRoom = roomY - currentRoomY;
            ChangeRoom(dxRoom, dyRoom);
        }
        else
        {
            // В пределах той же комнаты - просто перемещаемся
            map[playerPos.y][playerPos.x] = EMPTY;
            map[newY][newX] = PLAYER;
        }
        steps++;
        break;

    case HEALTH:
        map[playerPos.y][playerPos.x] = EMPTY;
        map[newY][newX] = PLAYER;
        HealPlayer(25);
        steps++;
        break;

    case TRAP:
        map[playerPos.y][playerPos.x] = EMPTY;
        map[newY][newX] = PLAYER;
        DamagePlayer(20);
        steps++;
        break;
    }

    // Проверяем смерть игрока
    if (health <= 0)
    {
        MessageBox(NULL, L"Вы погибли! Начинаем новую игру.", L"Game Over", MB_OK);
        health = 100;
        gold = 0;
        steps = 0;
        GenerateMap();
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

void SaveGame()
{
    FILE* file = fopen("savegame.dat", "wb");
    if (file)
    {
        // Сохраняем карту
        size_t mapSize = (size_t)TOTAL_HEIGHT * (size_t)TOTAL_WIDTH;
        fwrite(map, sizeof(int), mapSize, file);

        // Сохраняем информацию о посещенных комнатах
        size_t exploredSize = (size_t)ROOMS_Y * (size_t)ROOMS_X;
        fwrite(explored, sizeof(bool), exploredSize, file);

        // Сохраняем игровые переменные
        fwrite(&currentRoomX, sizeof(int), 1, file);
        fwrite(&currentRoomY, sizeof(int), 1, file);
        fwrite(&steps, sizeof(int), 1, file);
        fwrite(&gold, sizeof(int), 1, file);
        fwrite(&health, sizeof(int), 1, file);
        fwrite(&visitedRooms, sizeof(int), 1, file);

        fclose(file);

        MessageBox(NULL, L"Игра сохранена!", L"Сохранение", MB_OK);
    }
}

void LoadGame()
{
    FILE* file = fopen("savegame.dat", "rb");
    if (file)
    {
        // Загружаем карту
        size_t mapSize = (size_t)TOTAL_HEIGHT * (size_t)TOTAL_WIDTH;
        fread(map, sizeof(int), mapSize, file);

        // Загружаем информацию о посещенных комнатах
        size_t exploredSize = (size_t)ROOMS_Y * (size_t)ROOMS_X;
        fread(explored, sizeof(bool), exploredSize, file);

        // Загружаем игровые переменные
        fread(&currentRoomX, sizeof(int), 1, file);
        fread(&currentRoomY, sizeof(int), 1, file);
        fread(&steps, sizeof(int), 1, file);
        fread(&gold, sizeof(int), 1, file);
        fread(&health, sizeof(int), 1, file);
        fread(&visitedRooms, sizeof(int), 1, file);

        fclose(file);

        MessageBox(NULL, L"Игра загружена!", L"Загрузка", MB_OK);
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

        // Рисуем карту и интерфейс
        DrawMap(hdc);
        DrawUI(hdc);

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case VK_LEFT:
            MovePlayer(-1, 0);
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case VK_RIGHT:
            MovePlayer(1, 0);
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case VK_UP:
            MovePlayer(0, -1);
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case VK_DOWN:
            MovePlayer(0, 1);
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case 'S':
            SaveGame();
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case 'F':
            LoadGame();
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case 'R':
            health = 100;
            gold = 0;
            steps = 0;
            currentRoomX = 1;
            currentRoomY = 1;
            GenerateMap();
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}