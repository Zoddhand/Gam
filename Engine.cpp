#include "Engine.h"
#include "Spikes.h"
#include "FallingTrap.h"
#include "Orc.h"
#include <SDL3/SDL.h>
#include <iostream>

// --------------------------- Main ---------------------------

int main(int, char**) {
    Engine engine;
    while (engine.running) {
        engine.handleEvents();
        engine.update();
        engine.render();
        SDL_Delay(16);
    }
    return 0;
}

// ------------------------ Engine ---------------------------

Engine::Engine() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("SDL3 Platformer",SCREEN_W * 2, SCREEN_H * 2, SDL_WINDOW_BORDERLESS);
    renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H,SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderScale(renderer, VIEW_SCALE, VIEW_SCALE);

    float sx = 0.0f, sy = 0.0f;
    SDL_GetRenderScale(renderer, &sx, &sy);
    SDL_Log("Render scale = %f x %f", sx, sy);

    // Load tileset first so we can read object spawn data from the map
    if (!map.loadTileset(renderer, "assets/Tiles/tileset.png")) {
        SDL_Log("Failed to load tileset!");
    }

    // Spawn gameobjects from the map's object layer (tile values indicate what to spawn)
    player = nullptr;
    auto spawns = map.getObjectSpawns();
    for (const auto& s : spawns) {
        float px = float(s.x * Map::TILE_SIZE);
        float py = float(s.y * Map::TILE_SIZE);

        switch (s.tileIndex) {
        case Map::SPAWN_PLAYER:
            if (!player) {
                player = new Player(renderer, "Assets/Sprites/player.png", 12, 16);
                player->obj.x = px;
                player->obj.y = py;
            } else {
                // multiple player spawns: ignore extras or log / choose earliest
            }
            break;

        case Map::SPAWN_ORC:
            orc.push_back(new Orc(renderer, "Assets/Sprites/enemy.png", 12, 16, px, py));
            break;

        case Map::SPAWN_FALLINGTRAP:
            // FallingTrap ctor expects tile coords in this codebase
            fallT.push_back(new FallingTrap(renderer, s.x, s.y));
            break;

        case Map::SPAWN_SPIKES:
            objects.push_back(new Spikes(s.x, s.y, s.tileIndex));
            break;

        default:
            break;
        }
    }

    // Fallback: if map did not provide a player spawn, create a default player
    if (!player) {
        player = new Player(renderer, "Assets/Sprites/player.png", 12, 16);
    }

    // (Optional) keep any manual spawns you still want — currently everything is map-driven
}

Engine::~Engine() {
    delete player;
    for (Orc* o : orc)
        delete o;
    for (MapObject* obj : objects)
        delete obj;
    for (FallingTrap* f : fallT)
        delete f;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Engine::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) running = false;
    }

    const bool* keys = SDL_GetKeyboardState(nullptr);
    player->input(keys);
    if (keys[SDL_SCANCODE_3]) {
        VIEW_SCALE = 2.0f;
        SDL_SetRenderScale(renderer, VIEW_SCALE, VIEW_SCALE);

    }
    else if (keys[SDL_SCANCODE_2]) {
        VIEW_SCALE = 1.5f;    SDL_SetRenderScale(renderer, VIEW_SCALE, VIEW_SCALE);

    }
    else if (keys[SDL_SCANCODE_1]) {
        VIEW_SCALE = 1.0f;
        SDL_SetRenderScale(renderer, VIEW_SCALE, VIEW_SCALE);
    }
}

void Engine::update() {
    if(!player->obj.alive)
        running = false;
    player->update(map);
    for (FallingTrap* f : fallT)
    {
        f->checkTrigger(*player);
        f->update(map);
    }
    for (Orc* o : orc)
        o->aiUpdate(*player, map);

    // update map objects
    for (MapObject* obj : objects)
    {
        obj->update(*player, map);
        for (Orc* o : orc)
            obj->update(*o, map);
    }

    camera.update(player->obj.x, player->obj.y, map.width, SCREEN_W, map.height, SCREEN_H, TILE_SIZE, VIEW_SCALE);
}

void Engine::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    map.draw(renderer, camera.x, camera.y);

    // draw objects (e.g. traps/chests) between map and actors or above map as you prefer
    for (MapObject* obj : objects)
        obj->draw(renderer, camera.x, camera.y, map);

    player->draw(renderer, camera.x, camera.y);
    for (FallingTrap* f : fallT)
        f->draw(renderer, camera.x, camera.y);
    for (Orc* o : orc)
        o->draw(renderer, camera.x, camera.y);
    SDL_RenderPresent(renderer);
}
