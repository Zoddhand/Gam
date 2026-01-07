#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include "Map.h"
#include "Camera.h"
#include "Player.h"
#include "Orc.h"
#include "MapObject.h"
#include "WorldObject.h"
#include "FallingTrap.h"

class Engine {
public:
    Engine();
    ~Engine();
private:


    const uint32_t SCREEN_W = 320;
    const uint32_t SCREEN_H = 240;
	const uint32_t TILE_SIZE = 16;
    float VIEW_SCALE = 1.5f;

public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = true;

    void handleEvents();
    void update();
    void render();

    Map map;
    Camera camera;
    Player* player = nullptr;
    FallingTrap* fall = nullptr;
    std::vector<Orc*> orc; // store multiple enemies
    std::vector<MapObject*> objects; // traps, chests, etc.
    std::vector<FallingTrap*> fallT; // traps, chests, etc.
};
