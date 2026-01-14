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
#include "GameOver.h"
#include "Background.h"
#include <unordered_set>

class Menu;
class Archer;
class Arrow;
class Potion; // forward

#define LEFT  26
#define UP    27
#define RIGHT 28
#define DOWN  29
#define WRAP  25
#define WRAPU  7
#define WRAPD  8
#define PORT  25

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

    // Trigger a short global hitstop (freeze) measured in update ticks
    void triggerHitstop(int ticks = 6);

    // Door persistence API
    void markDoorOpened(int levelID, int tx, int ty);
    bool isDoorOpened(int levelID, int tx, int ty) const;

    // Key persistence API
    void markKeyCollected(int levelID, int tx, int ty);
    bool isKeyCollected(int levelID, int tx, int ty) const;

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
    std::vector<Archer*> archers;
    std::vector<MapObject*> objects;
    std::vector<FallingTrap*> fallT;
    std::vector<Arrow*> projectiles;
    std::vector<Potion*> potions; // managed potions
    Hud* hud = nullptr;
    Sound* sound = nullptr;
    Menu* menu = nullptr;
    bool inMenu = true;
    // Debug: draw attack hitboxes
    bool debugDrawAttackRects = true;

    // Controller instance (single controller for now)
    Controller controller;

    // Game over screen
    GameOver* gameOver = nullptr;
    bool inGameOver = false;

    // Backgrounds
    std::vector<Background*> backgrounds;

    // ----------------------------------------

    // ---------------- Level Transition ----------------
    bool transitioning = false;
    int currentLevelID = 22;
    int pendingLevelID = -1;
    int entryDirection = -1;
    int lastStartPosX = 0;
    int lastStartPosY = 0;
    int lastCheckpointLevel = -1; // level of last touched checkpoint
    // When true, loadLevel will prefer the saved checkpoint position (used only when respawning after death)
    bool respawnFromCheckpoint = false;
    bool playerLastFacing = false;
    // When true, ignore held Up input until the player releases Up once. Used to avoid
    // immediately wrapping back when arriving at a destination from a WRAPU/WRAPD transition.
    bool ignoreWrapUp = false;

    float transitionTimer = 0.0f;
    const float TRANSITION_DURATION = 0.1f;
    SDL_FRect transitionRect{ 0,0,float(SCREEN_W),float(SCREEN_H) };

    static constexpr int GRID_ROWS = 10;
    static constexpr int GRID_COLS = 10;

    int levelGrid[GRID_ROWS][GRID_COLS] = {
        {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10},
        { 11, 12, 13, 14, 15, 16, 17, 18, 19, 20},
        { 21, 22, 23, 24, 25, 26, 27, 28, 29, 30},
        { 31, 32, 33, 34, 35, 36, 37, 38, 39, 40},
        { 41, 42, 43, 44, 45, 46, 47, 48, 49, 50},
        { 51, 52, 53, 54, 55, 56, 57, 58, 59, 60},
        { 61, 62, 63, 64, 65, 66, 67, 68, 69, 70},
        { 71, 72, 73, 74, 75, 76, 77, 78, 79, 80},
        { 81, 82, 83, 84, 85, 86, 87, 88, 89, 90},
        { 91, 92, 93, 94, 95, 96, 97, 98, 99,100}
    };

    // Global hitstop timer (measured in update ticks). When >0, most gameplay updates are frozen.
    int hitstopTicks = 0;

    // Debug key edge detection to avoid repeated triggers while key held
    bool debugNextPressedLastFrame = false;
    bool debugPrevPressedLastFrame = false;

private:
    // persistent set of opened doors across level loads
    std::unordered_set<uint64_t> openedDoors;

    // persistent set of collected keys across level loads
    std::unordered_set<uint64_t> collectedKeys;
};

// Global pointer to the active Engine instance (used to trigger hitstop from gameplay code)
extern Engine* gEngine;
