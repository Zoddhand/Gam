#include "Engine.h"
#include "Spikes.h"
#include "Menu.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <ctime>




int main(int argc, char* argv[])
{
    // Call once at program start
    srand(static_cast<unsigned>(time(nullptr)));

    Engine engine;

    // Load starting room
    engine.loadLevel(engine.currentLevelID);

    while (engine.running)
    {
        engine.handleEvents();
        engine.update();
        engine.render();
        SDL_Delay(16); // ~60 FPS
    }

    return 0;
}

// --------------------------------------------------

Engine::Engine()
{
    // Initialize video + audio subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
    }

    window = SDL_CreateWindow("Platformer", SCREEN_W * 2, SCREEN_H * 2, SDL_WINDOW_BORDERLESS);
    renderer = SDL_CreateRenderer(window, nullptr);

    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderScale(renderer, VIEW_SCALE, VIEW_SCALE);

    float sx, sy;
    SDL_GetRenderScale(renderer, &sx, &sy);
    SDL_Log("Render scale = %f x %f", sx, sy);

    // Create HUD
    hud = new Hud(renderer, "Assets/Sprites/heart.png");

    // Create Sound manager
    sound = new Sound();
    // expose global pointer immediately so gameplay code can use it
    gSound = sound;
    SDL_Log("Engine: created sound instance %p and assigned to gSound", (void*)sound);

    if (!sound->init()) {
        SDL_Log("Warning: sound init failed");
        // clear global if init failed
        gSound = nullptr;
    }
    else {
        // Load some example files (put your WAVs in Assets/Sound)
        sound->loadWav("music", "Assets/Sound/MOONLIGHT.wav");
        sound->loadWav("hit", "Assets/Sound/Female Hit 1.wav");
        sound->loadWav("orc_hit", "Assets/Sound/Orc Hit 1.wav");
        sound->loadWav("step", "Assets/Sound/step.wav");
        sound->loadWav("jump1", "Assets/Sound/jump1.wav");
        sound->loadWav("jump2", "Assets/Sound/jump2.wav");
        sound->loadWav("attack1", "Assets/Sound/attack1.wav");
        sound->loadWav("attack2", "Assets/Sound/attack2.wav");
        sound->loadWav("attack3", "Assets/Sound/attack3.wav");
        sound->loadWav("death", "Assets/Sound/death.wav");
        sound->loadWav("heartbeat", "Assets/Sound/heartbeat.wav");
        sound->loadWav("fallTrapRelease", "Assets/Sound/fallTrapRelease.wav");
        sound->loadWav("fallTrapLand", "Assets/Sound/fallTrapLand.wav");
        sound->playMusic("music", true, 32);
    }

    // Create menu
    menu = new Menu(renderer, VIEW_SCALE);
    inMenu = true;
}

Engine::~Engine()
{
    cleanupObjects();
    if (menu) delete menu;
    if (hud) delete hud;
    if (sound) {
        // clear global pointer first
        gSound = nullptr;
        sound->shutdown(); delete sound; }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// --------------------------------------------------

void Engine::cleanupObjects()
{
    delete player; player = nullptr;
    for (auto* o : orc) delete o; orc.clear();
    for (auto* f : fallT) delete f; fallT.clear();
    for (auto* o : objects) delete o; objects.clear();
}

// --------------------------------------------------

int Engine::getNextLevelID(int dir)
{
    int r = currentLevelID / GRID_COLS;
    int c = currentLevelID % GRID_COLS;

    if (dir == LEFT)  c--;
    if (dir == RIGHT) c++;
    if (dir == UP)    r--;
    if (dir == DOWN)  r++;

    if (r < 0 || r >= GRID_ROWS || c < 0 || c >= GRID_COLS)
        return -1;

    return levelGrid[r][c];
}

// --------------------------------------------------

void Engine::handleEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
        if (e.type == SDL_EVENT_QUIT)
            running = false;

    const bool* keys = SDL_GetKeyboardState(nullptr);
    // Only forward input to player if movement is allowed
    if (!inMenu) {
        if (player && !transitioning) player->input(keys);
    } else {
        if (menu) menu->handleInput(keys);
    }
}

// --------------------------------------------------
int t1 = 0;
int t2 = 0;
void Engine::update()
{
    if (inMenu) {
        if (menu) menu->update();
        int sel = menu->consumeSelection();
        if (sel != -1) {
            if (sel == 0) { // Play
                inMenu = false;
            }
        }
        // still update sound so logs/streams remain functional
        if (sound) sound->update();
        return;
    }
    sound->stopMusic();
    if (!transitioning && player)
    {
        int tx = int(player->obj.x) / TILE_SIZE;
        int ty = int(player->obj.y) / TILE_SIZE;

        if (tx >= 0 && ty >= 0 && tx < map.width && ty < map.height)
        {
            int t = map.spawn[ty * map.width + tx];
            if (t == LEFT || t == RIGHT || t == UP || t == DOWN)
            {
                pendingLevelID = getNextLevelID(t);
                if (pendingLevelID != -1)
                {
                    entryDirection = t;
                    transitioning = true;
                    transitionTimer = TRANSITION_DURATION;
                    // Prevent player from moving during transition
                    if (player) player->obj.canMove = false;
                }
            }
            for (auto* r : orc)
                r->aiUpdate(*player, map);

            for (auto* f : fallT)
            {
                f->checkTrigger(*player);
                f->update(map);
                for (auto* r : orc)
                    f->checkTrigger(*r);
            }

            for (auto* o : objects)
            {
                o->update(*player, map);
                for (auto* r : orc)
                    o->update(*r, map);
            }
        }
    }

    if (transitioning)
    {
        transitionTimer -= 0.016f;
        if (transitionTimer <= 0.0f)
        {
            loadLevel(pendingLevelID);
            transitioning = false;
        }
    }

    if (!player) return;
    if (player->obj.alive)
        player->update(map);
    else
    {
        player->obj.alive = true;
        if (lastStartPosX != 0)
        {
           t1 = lastStartPosX;
           t2 = lastStartPosY;
        }
        currentLevelID = 45;
        loadLevel(currentLevelID);
		player->obj.health = player->obj.maxHealth;
		sound->stopSfx("heartbeat");
		inMenu = true;
    }

    if (player->obj.health <= 39)
        sound->playSfx("heartbeat");

    camera.update(player->obj.x, player->obj.y,
        map.width, SCREEN_W,
        map.height, SCREEN_H,
        TILE_SIZE, VIEW_SCALE);

    // update sound streams
    if (sound) sound->update();
}

// --------------------------------------------------

void Engine::render()
{
    if (inMenu) {
        if (menu) menu->render(renderer);
        SDL_RenderPresent(renderer);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    map.draw(renderer, camera.x, camera.y);
    for (auto* o : orc)
        o->draw(renderer, camera.x, camera.y);

    // ---- draw traps / objects ----
    for (auto* f : fallT)
        f->draw(renderer, camera.x, camera.y);
    
    for (auto* o : objects)
        o->draw(renderer, camera.x, camera.y, map);

    if (player) player->draw(renderer, camera.x, camera.y);

    // Draw HUD last so it's on top
    if (hud && player) hud->draw(renderer, player->obj.health, player->obj.maxHealth);

    if (transitioning)
    {
        float a = 255.0f * (transitionTimer / TRANSITION_DURATION);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)a);
        SDL_RenderFillRect(renderer, &transitionRect);
    }

    SDL_RenderPresent(renderer);
}

// --------------------------------------------------

void Engine::loadLevel(int levelID)
{
    // Preserve player health across loads
    float savedHealth = 100.0f;
    float savedMaxHealth = 100.0f;
    if (player) {
        savedHealth = player->obj.health;
        savedMaxHealth = player->obj.maxHealth;
    }

    // ----------------------------------------
    // Cleanup previous level
    // ----------------------------------------
    cleanupObjects();

    char name[8];
    snprintf(name, sizeof(name), "%03d", levelID);

    // ----------------------------------------
    // Load map data
    // ----------------------------------------
    map.loadCSV("Maps/" + std::string(name) + "_Tile Layer 1.csv");
    map.loadSpawnCSV("Maps/" + std::string(name) + "_Spawn Layer.csv");
    // load collision layer if present      
    map.loadColCSV("Maps/" + std::string(name) + "_Collision Layer.csv");
    map.loadTileset(renderer, "assets/Tiles/tileset.png");

    // ----------------------------------------
    // Determine PLAYER spawn position
    // ----------------------------------------
    float spawnX = 0.0f;
    float spawnY = 0.0f;
    bool placed = false;

    int portalToFind = -1;
    if (entryDirection == LEFT)  portalToFind = RIGHT;
    if (entryDirection == RIGHT) portalToFind = LEFT;
    if (entryDirection == UP)    portalToFind = DOWN;
    if (entryDirection == DOWN)  portalToFind = UP;

    // 1️⃣ Spawn relative to opposite portal
    if (portalToFind != -1)
    {
        for (int y = 0; y < map.height && !placed; y++)
        {
            for (int x = 0; x < map.width && !placed; x++)
            {
                if (map.spawn[y * map.width + x] == portalToFind)
                {
                    spawnX = float(x * TILE_SIZE);
                    spawnY = float(y * TILE_SIZE);

                    // Offset INTO the room
                    if (portalToFind == LEFT)  spawnX += TILE_SIZE;
                    if (portalToFind == RIGHT) spawnX -= TILE_SIZE;
                    if (portalToFind == UP)    spawnY += TILE_SIZE;
                    if (portalToFind == DOWN)  spawnY -= TILE_SIZE;

                    placed = true;

                    SDL_Log("Portal %d at (%d,%d) -> spawn (%f,%f)",
                        portalToFind, x, y, spawnX, spawnY);
                }
            }
        }
    }

    // 2️⃣ Fallback to SPAWN_PLAYER
    if (!placed)
    {
        for (int y = 0; y < map.height && !placed; y++)
        {
            for (int x = 0; x < map.width && !placed; x++)
            {
                if (map.spawn[y * map.width + x] == Map::SPAWN_PLAYER)
                {
                    spawnX = float(x * TILE_SIZE);
                    spawnY = float(y * TILE_SIZE);
                    placed = true;
                }
            }
        }
    }
    
    SDL_Log("Spawning player at (%f,%f)", spawnX, spawnY);

    // ----------------------------------------
    // Create PLAYER (ENGINE-OWNED)
    // ----------------------------------------
    const char* spritePath =
        (rand() % 2 == 0)
        ? "Assets/Sprites/player.png"
        : "Assets/Sprites/swordsman.png";
    
    player = new Player(renderer, "Assets/Sprites/swordsman.png", 12, 16);
    player->obj.x = spawnX;
    player->obj.y = spawnY;
    player->obj.canMove = true;

    // Restore persisted health if available
    player->obj.health = savedHealth;
    player->obj.maxHealth = savedMaxHealth;

    setPlayerFacingFromEntry(entryDirection);

    // ----------------------------------------
    // Spawn MAP OBJECTS (NOT PLAYER)
    // ----------------------------------------
    auto spawns = map.getObjectSpawns();
    for (const auto& s : spawns)
    {
        float px = float(s.x * Map::TILE_SIZE);
        float py = float(s.y * Map::TILE_SIZE);

        switch (s.tileIndex)
        {
        case Map::SPAWN_PLAYER:
            // 🚫 Player already handled
            break;

        case Map::SPAWN_ORC:
            orc.push_back(new Orc(renderer,"Assets/Sprites/orc.png",12, 16,px, py , 20));
            break;

        case Map::SPAWN_SKELETON:
            orc.push_back(new Orc(
                renderer,
                "Assets/Sprites/Skeleton.png",
                12, 16,
                px, py, 40
            ));
            break;

        case Map::SPAWN_FALLINGTRAP:
            fallT.push_back(new FallingTrap(renderer, s.x, s.y));
            break;

        case Map::SPAWN_SPIKES:
            objects.push_back(new Spikes(s.x, s.y, s.tileIndex));
            break;
        }
    }

    // ----------------------------------------
    // Finalize level state
    // ----------------------------------------
    currentLevelID = levelID;
    entryDirection = -1;
    lastStartPosX = spawnX;
    lastStartPosY = spawnY;
}


void Engine::setPlayerFacingFromEntry(int entryTile)
{
    switch (entryTile)
    {
    case 96: // came from LEFT
        player->obj.facing = false;
        break;

    case 98: // came from RIGHT
        player->obj.facing = true;
        break;

    case 97: // came from UP
        // optional – keep previous or face down
        break;

    case 99: // came from DOWN
        // optional – keep previous or face up
        break;

    default:
        break;
    }
}