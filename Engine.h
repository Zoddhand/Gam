#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include "Map.h"
#include "Camera.h"
#include "Player.h"
#include "Orc.h"
#include "MapObject.h"
#include "FallingTrap.h"
#include "Hud.h"
#include "Sound.h"

#define LEFT  96
#define UP    97
#define RIGHT 98
#define DOWN  99

class Engine {
public:
    Engine();
    ~Engine();

    void handleEvents();
    void update();
    void render();

    void loadLevel(int levelID);
    void setPlayerFacingFromEntry(int entryTile);
    int getNextLevelID(int direction);

private:
    void cleanupObjects();

    const uint32_t SCREEN_W = 320;
    const uint32_t SCREEN_H = 240;
    const uint32_t TILE_SIZE = 16;
    float VIEW_SCALE = 1.5f;

public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = true;

    Map map;
    Camera camera;
    Player* player = nullptr;
    std::vector<Orc*> orc;
    std::vector<MapObject*> objects;
    std::vector<FallingTrap*> fallT;
    Hud* hud = nullptr;
    Sound* sound = nullptr;
	// ----------------------------------------

    // ---------------- Level Transition ----------------
    bool transitioning = false;
    int currentLevelID = 45;
    int pendingLevelID = -1;
    int entryDirection = -1;
    int lastStartPosX = 0;
    int lastStartPosY = 0;

    float transitionTimer = 0.0f;
    const float TRANSITION_DURATION = 1.5f;
    SDL_FRect transitionRect{ 0,0,float(SCREEN_W),float(SCREEN_H) };

    static constexpr int GRID_ROWS = 10;
    static constexpr int GRID_COLS = 10;

    int levelGrid[GRID_ROWS][GRID_COLS] = {
        { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9},
        {10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
        {20, 21, 22, 23, 24, 25, 26, 27, 28, 29},
        {30, 31, 32, 33, 34, 35, 36, 37, 38, 39},
        {40, 41, 42, 43, 44, 45, 46, 47, 48, 49},
        {50, 51, 52, 53, 54, 55, 56, 57, 58, 59},
        {60, 61, 62, 63, 64, 65, 66, 67, 68, 69},
        {70, 71, 72, 73, 74, 75, 76, 77, 78, 79},
        {80, 81, 82, 83, 84, 85, 86, 87, 88, 89},
        {90, 91, 92, 93, 94, 95, 96, 97, 98, 99}
    };
};
