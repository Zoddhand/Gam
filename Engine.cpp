#include "Engine.h"
#include "Spikes.h"
#include "Menu.h"
#include "GameOver.h"
#include "Archer.h"
#include "Arrow.h"
#include "ArrowTrap.h"
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

    const double targetFps = 60.0; // fallback target
    const double targetMs = 1000.0 / targetFps;

    Uint32 lastTick = SDL_GetTicks();
    while (engine.running)
    {
        Uint32 frameStart = SDL_GetTicks();

        engine.handleEvents();
        engine.update();
        engine.render();

        Uint32 frameEnd = SDL_GetTicks();
        double elapsed = double(frameEnd - frameStart);
        if (elapsed < targetMs) {
            SDL_Delay((Uint32)(targetMs - elapsed));
        }
    }

    return 0;
}

// --------------------------------------------------

Engine* gEngine = nullptr; // define global pointer

Engine::Engine()
{
    // register global pointer
    gEngine = this;

    // Initialize video + audio subsystems
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);

    window = SDL_CreateWindow("Platformer", SCREEN_W * 2, SCREEN_H * 2, SDL_WINDOW_BORDERLESS);
    /* Move to 3rd monitor */
    int displayCount = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&displayCount);

    if (displayCount > 2) { // need at least 3 monitors
        SDL_Rect bounds;
        SDL_GetDisplayBounds(displays[2], &bounds);

        SDL_SetWindowPosition(
            window,
            bounds.x,
            bounds.y
        );
    }
    else {
        SDL_Log("Monitor 3 not available (found %d displays)", displayCount);
    }

    // Request VSync via hint before creating renderer
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    // Create renderer (let driver decide flags). Many backends respect the hint.
    renderer = SDL_CreateRenderer(window, nullptr);

    if (!renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
    } else {
        SDL_Log("Renderer created");
    }

    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderScale(renderer, VIEW_SCALE, VIEW_SCALE);

    float sx, sy;
    SDL_GetRenderScale(renderer, &sx, &sy);
    SDL_Log("Render scale = %f x %f", sx, sy);

    // Try to open first controller (if present)
    if (!controller.open(0)) {
        SDL_Log("No controller opened (none present)");
    } else {
        SDL_Log("Controller opened");
    }

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
        sound->loadWav("clang", "Assets/Sound/clang.wav");
        sound->loadWav("arrow", "Assets/Sound/arrow.wav");
        sound->loadWav("arrow_impact", "Assets/Sound/arrow_impact.wav");
        sound->loadWav("monster", "Assets/Sound/monster.wav");
        sound->loadWav("orc_death", "Assets/Sound/orc_death.wav");
        sound->loadWav("arrow_empty", "Assets/Sound/arrow_empty.wav");
        sound->playMusic("music", true, 32);
    }

    // Create menu
    menu = new Menu(renderer, VIEW_SCALE);
    inMenu = true;

    // Create game over screen
    gameOver = new GameOver(renderer, VIEW_SCALE);
}

Engine::~Engine()
{
    cleanupObjects();
    if (menu) delete menu;
    if (hud) delete hud;
    if (gameOver) delete gameOver;
    if (sound) {
        // clear global pointer first
        gSound = nullptr;
        sound->shutdown(); delete sound; }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    // clear global engine pointer
    gEngine = nullptr;
}

// --------------------------------------------------

void Engine::cleanupObjects()
{
    delete player; player = nullptr;
    for (auto* o : orc) delete o; orc.clear();
    for (auto* a : archers) delete a; archers.clear();
    for (auto* f : fallT) delete f; fallT.clear();
    for (auto* o : objects) delete o; objects.clear();
    for (auto* p : projectiles) delete p; projectiles.clear();
}

// --------------------------------------------------

int Engine::getNextLevelID(int dir)
{
    // Convert 1-based level ID to 0-based index for grid math
    int idx = currentLevelID - 1;
    if (idx < 0) return -1;

    int r = idx / GRID_COLS;
    int c = idx % GRID_COLS;

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
    // update controller polling every frame
    controller.update();
    auto cs = controller.getState();

    // If in game over, route input only to gameOver UI
    if (inGameOver && gameOver) {
        if (cs.connected) gameOver->handleInput(cs);
        else gameOver->handleInput(keys);
        return;
    }

    // Only forward input to player if movement is allowed
    if (!inMenu) {
        if (player && !transitioning) {
            // Prefer controller only if it is actively providing input; otherwise keyboard.
            bool controllerActive = cs.connected && (cs.left || cs.right || cs.up || cs.down || cs.jump || cs.attack);
            if (controllerActive) {
                player->input(cs);
            } else {
                if (player) player->input(keys);
            }
        }
    } else {
        if (menu) {
            // If controller connected, forward controller state to menu, otherwise keyboard
            if (cs.connected) menu->handleInput(cs);
            else menu->handleInput(keys);
        }
    }

	if (keys[SDL_SCANCODE_ESCAPE]) {
        running = false;
    }
    if (keys[SDL_SCANCODE_5])
    {
		loadLevel(48); // level ID 45 is a test room
    }
}

// --------------------------------------------------
int t1 = 0;
int t2 = 0;
void Engine::update()
{
    // If hitstop active, decrement and skip gameplay updates (but allow input and rendering)
    if (hitstopTicks > 0) {
        --hitstopTicks;
        if (inMenu) {
            if (menu) menu->update();
            int sel = menu->consumeSelection();
            if (sel != -1) {
                if (sel == 0) { // Play
                    inMenu = false;
                }
            }
            if (sound) sound->update();
            return;
        }

        // While hitstop is active, still update sound
        if (sound) sound->update();
        return;
    }

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
            if (!inGameOver) {
                // Update all game objects
                for (auto* r : orc)
                    r->aiUpdate(*player, map);

                for (auto* a : archers)
                    a->aiUpdate(*player, map, projectiles);

                for (auto* f : fallT)
                {
                    f->checkTrigger(*player, map);
                    f->update(map);
                    for (auto* r : orc)
                        f->checkTrigger(*r, map);
                    for (auto* a : archers)
                        f->checkTrigger(*a, map);
                }

                for (auto* o : objects)
                {
                    o->update(*player, map);
                    for (auto* r : orc)
                        o->update(*r, map);
                    for (auto* a : archers)
                        o->update(*a, map);
                }
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
    if (player->obj.alive && !inGameOver) {
        // Preserve previous X to determine movement direction for collision resolution
        float prevPlayerX = player->obj.x;
        player->update(map);

        // Prevent player from walking through orcs: if player's hitbox intersects an orc,
        // push the player out horizontally to the side of the orc and stop horizontal velocity.
        for (auto* e : orc) {
            if (!e || !e->obj.alive) continue;

            SDL_FRect pr = player->getRect();
            SDL_FRect er = e->getRect();

            if (SDL_HasRectIntersectionFloat(&pr, &er)) {
                // If player moved right into the orc, snap player to left side
                if (player->obj.x > prevPlayerX) {
                    player->obj.x = e->obj.x - player->obj.tileWidth;
                }
                // If player moved left into the orc, snap player to right side
                else if (player->obj.x < prevPlayerX) {
                    player->obj.x = e->obj.x + e->obj.tileWidth;
                }
                else {
                    // No horizontal movement this frame — nudge based on centers
                    float pCenter = pr.x + pr.w * 0.5f;
                    float eCenter = er.x + er.w * 0.5f;
                    if (pCenter < eCenter)
                        player->obj.x = e->obj.x - player->obj.tileWidth;
                    else
                        player->obj.x = e->obj.x + e->obj.tileWidth;
                }

                player->obj.velx = 0.0f;
                // Update prevPlayerX so multiple overlapping orcs resolve correctly in a single frame
                prevPlayerX = player->obj.x;
            }
        }
        // Also prevent walking through archers using same resolution logic
        for (auto* a : archers) {
            if (!a || !a->obj.alive) continue;

            SDL_FRect pr = player->getRect();
            SDL_FRect ar = a->getRect();

            if (SDL_HasRectIntersectionFloat(&pr, &ar)) {
                if (player->obj.x > prevPlayerX) {
                    player->obj.x = a->obj.x - player->obj.tileWidth;
                }
                else if (player->obj.x < prevPlayerX) {
                    player->obj.x = a->obj.x + a->obj.tileWidth;
                }
                else {
                    float pCenter = pr.x + pr.w * 0.5f;
                    float aCenter = ar.x + ar.w * 0.5f;
                    if (pCenter < aCenter)
                        player->obj.x = a->obj.x - player->obj.tileWidth;
                    else
                        player->obj.x = a->obj.x + a->obj.tileWidth;
                }

                player->obj.velx = 0.0f;
                prevPlayerX = player->obj.x;
            }
        }
    }
    else
    {
        // Enter game over state (do not immediately respawn)
        if (!inGameOver) {
            inGameOver = true;
            if (sound) {
                sound->playSfx("death");
                // Ensure any low-health heartbeat SFX is stopped when entering game over
                sound->stopSfx("heartbeat");
            }
        }
    }

    // update projectiles
    for (auto it = projectiles.begin(); it != projectiles.end(); ) {
        Arrow* a = *it;
        a->update(map);
        // simple player collision
        if (player) {
            SDL_FRect ar = a->getRect();

            // If player is attacking, check attack hitbox against projectile and destroy arrow
            if (player->obj.attacking) {
                SDL_FRect atk = player->getAttackRect();
                if (SDL_HasRectIntersectionFloat(&atk, &ar)) {
                    // Destroy arrow and play feedback
                    a->alive = false;
                    if (gSound) gSound->playSfx("arrow_impact");
                    // trigger small hitstop for arrow parry
                    if (gEngine) gEngine->triggerHitstop(4);
                }
            }

            // If arrow still alive, check collision with player body
            if (a->alive) {
                SDL_FRect pr = player->getRect();
                if (SDL_HasRectIntersectionFloat(&ar, &pr)) {
                    player->takeDamage(15.0f, ar.x + ar.w*0.5f, 3, 6, 30, 2.5f, -4.0f);
                    a->alive = false;
                    // small hitstop when player is hit by arrow
                    if (gEngine) gEngine->triggerHitstop(6);
                }
            }
        }

        if (!a->alive) { delete a; it = projectiles.erase(it); }
        else ++it;
    }

    // If in game over, check for selection
    if (inGameOver && gameOver) {
        gameOver->update();
        int sel = gameOver->consumeSelection();
        if (sel != -1) {
            if (sel == 0) {
                // Restart: reload the starting level and return to menu
                inGameOver = false;
                currentLevelID = 45;
                loadLevel(currentLevelID);
                if (player) player->obj.health = player->obj.maxHealth;
                if (sound) sound->stopSfx("heartbeat");
                inMenu = true;
            } else if (sel == 1) {
                // Quit
                running = false;
            }
        }
    }

    if (player->obj.health <= 39 && sound && !inGameOver)
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

    if (inGameOver && gameOver) {
        // Draw last frame of level under overlay
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        map.draw(renderer, camera.x, camera.y);
        for (auto* o : orc)
            o->draw(renderer, camera.x, camera.y);
        for (auto* f : fallT)
            f->draw(renderer, camera.x, camera.y);
        for (auto* o : objects)
            o->draw(renderer, camera.x, camera.y, map);
        for (auto* a : archers)
            a->draw(renderer, camera.x, camera.y);
        for (auto* p : projectiles)
            p->draw(renderer, camera.x, camera.y);
        if (player) player->draw(renderer, camera.x, camera.y);
        if (hud && player) hud->draw(renderer, player->obj.health, player->obj.maxHealth);

        gameOver->render(renderer);
        SDL_RenderPresent(renderer);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 31, 14, 28, 255);
    SDL_RenderClear(renderer);
    
    map.draw(renderer, camera.x, camera.y);
    for (auto* o : orc)
        o->draw(renderer, camera.x, camera.y);

    // ---- draw traps / objects ----
    for (auto* f : fallT)
        f->draw(renderer, camera.x, camera.y);
    
    for (auto* o : objects)
        o->draw(renderer, camera.x, camera.y, map);

    for (auto* a : archers)
        a->draw(renderer, camera.x, camera.y);

    for (auto* p : projectiles)
        p->draw(renderer, camera.x, camera.y);

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
                px, py, 40, true
            ));
            break;

        case Map::SPAWN_FALLINGTRAP:
            fallT.push_back(new FallingTrap(renderer, s.x, s.y));
            break;

        case Map::SPAWN_SPIKES:
            objects.push_back(new Spikes(s.x, s.y, s.tileIndex));
            break;

        case Map::SPAWN_ARCHER:
            archers.push_back(new Archer(renderer, "Assets/Sprites/archer.png", 12, 16, px, py, 10));
            break;
        case Map::SPAWN_ARROWTRAP_LEFT:
        case Map::SPAWN_ARROWTRAP_RIGHT:
            objects.push_back(new ArrowTrap(renderer, s.x, s.y, s.tileIndex));
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

// New: trigger a hitstop from gameplay code
void Engine::triggerHitstop(int ticks)
{
    // Clamp to a reasonable range
    if (ticks < 1) ticks = 1;
    if (ticks > 60) ticks = 60;
    hitstopTicks = ticks;
}