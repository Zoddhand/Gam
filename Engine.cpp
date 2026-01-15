#include "Engine.h"
#include "Spikes.h"
#include "Menu.h"
#include "GameOver.h"
#include "Archer.h"
#include "Arrow.h"
#include "ArrowTrap.h"
#include "Background.h"
#include "Door.h"
#include "Potion.h"
#include "Checkpoint.h"
// #include "Water.h"
#include "PressurePlate.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include "fallingplatform.h"
// MapObject used for simple animated tiles like water
#include "MapObject.h"
#include "Crate.h"
#include <algorithm>


static uint64_t packDoorKey(int levelID, int tx, int ty) {
    // pack three ints into 64-bit key: [levelID:16 bits][tx:24 bits][ty:24 bits]
    uint64_t key = (uint64_t(levelID & 0xFFFF) << 48) | (uint64_t(tx & 0xFFFFFF) << 24) | uint64_t(ty & 0xFFFFFF);
    return key;
}

// pack for keys uses same scheme
static uint64_t packKeyKey(int levelID, int tx, int ty) {
    return packDoorKey(levelID, tx, ty);
}

// Engine door persistence API
void Engine::markDoorOpened(int levelID, int tx, int ty)
{
    uint64_t key = packDoorKey(levelID, tx, ty);
    openedDoors.insert(key);
}

bool Engine::isDoorOpened(int levelID, int tx, int ty) const
{
    uint64_t key = packDoorKey(levelID, tx, ty);
    return openedDoors.find(key) != openedDoors.end();
}

// Key persistence API
void Engine::markKeyCollected(int levelID, int tx, int ty) {
    uint64_t key = packKeyKey(levelID, tx, ty);
    collectedKeys.insert(key);
}

bool Engine::isKeyCollected(int levelID, int tx, int ty) const {
    uint64_t key = packKeyKey(levelID, tx, ty);
    return collectedKeys.find(key) != collectedKeys.end();
}

Engine* gEngine = nullptr; // define global pointer

Engine::Engine()
{
    // register global pointer
    gEngine = this;

    // Initialize video + audio subsystems
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
    SDL_SetRenderVSync(renderer, 1);  // enable
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
        sound->loadWav("orc_laugh", "Assets/Sound/orc_laugh.wav");
        sound->loadWav("fall_plat", "Assets/Sound/fall_plat.wav");
        sound->loadWav("lock_open", "Assets/Sound/lock_open.wav");
        sound->loadWav("lock_fail", "Assets/Sound/lock_fail.wav");
        sound->loadWav("door_open", "Assets/Sound/door_open.wav");
        sound->playMusic("music", true, 32);
    }

    // Create menu
    menu = new Menu(renderer, VIEW_SCALE);
    inMenu = true;

    // Create game over screen
    gameOver = new GameOver(renderer, VIEW_SCALE);

    // Create backgrounds and map level ids
    auto fileExists = [](const std::string& path) -> bool {
        std::ifstream f(path);
        return f.good();
    };

    // Sky background (levels 22..25)
    std::string skyFirst = "Assets/Backgrounds/Sky/1.png";
    if (fileExists(skyFirst)) {
        Background* sky = new Background();
        sky->load(renderer, "Assets/Backgrounds/Sky", 7);
        // assign levels 22,23,24,25 to sky
        for(int i = 0; i < 40; i ++)
            sky->addLevel(i);
        backgrounds.push_back(sky);
    } else {
        SDL_Log("Background: skipping missing directory Assets/Backgrounds/Sky");
    }

    // example: separate background for level 45
    std::string otherFirst = "Assets/Backgrounds/Other/1.png";
    if (fileExists(otherFirst)) {
        Background* other = new Background();
        other->load(renderer, "Assets/Backgrounds/Other", 7);
        other->addLevel(45);
        backgrounds.push_back(other);
    } else {
        SDL_Log("Background: skipping missing directory Assets/Backgrounds/Other");
    }

    // Create InfoText (used for in-world hints). Use BoldPixels.ttf as requested.
    infoText = new InfoText(renderer, "Assets/Fonts/BoldPixels.ttf", 16);
    // No explicit trigger rect or fade is set here — InfoText uses spawn tile id (default 5)
    // Set preferred sizes for certain icons (e.g. keyboard icons that are 32x16)
    infoText->setIconPreferredSize("Assets/Icons/jump_kb.png", 32, 16);
    infoText->setIconPreferredSize("Assets/Icons/attack_kb.png", 32, 16);
    infoText->setIconPreferredSize("Assets/Icons/special_kb.png", 32, 16);
    infoText->setIconPreferredSize("Assets/Icons/dodge_kb.png", 32, 16);
    infoText->setText("");
}

Engine::~Engine()
{
    cleanupObjects();
    if (menu) delete menu;
    if (hud) delete hud;
    if (gameOver) delete gameOver;
    if (infoText) { delete infoText; infoText = nullptr; }
    if (sound) {
        // clear global pointer first
        gSound = nullptr;
        sound->shutdown(); delete sound; }

    // cleanup backgrounds
    for (auto* b : backgrounds) delete b;
    backgrounds.clear();

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

    if (dir == WRAP) {
        // ...0 <-> ...1 warp
        int last = currentLevelID % 10;

        if (last == 0) {
            // 10->11, 20->21, 30->31, ...
            // (end of row: col 9 -> next row col 0)
            r += 1;
            c = 0;
        }
        else if (last == 1) {
            // 11->10, 21->20, 31->30, ...
            // (start of row: col 0 -> prev row col 9)
            r -= 1;
            c = GRID_COLS - 1;
        }
        else {
            return -1; // WRAP only defined for ...0 and ...1
        }
    }

    if (r < 0 || r >= GRID_ROWS || c < 0 || c >= GRID_COLS)
        return -1;

    return levelGrid[r][c];
}


// --------------------------------------------------

void Engine::handleEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            running = false;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN) {
            // keyboard activity detected
            lastKeyboardUseTicks = SDL_GetTicks();
            if (infoText) infoText->setLastInputIsController(false);
        }
        else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            // controller button pressed
            lastControllerUseTicks = SDL_GetTicks();
            if (infoText) infoText->setLastInputIsController(true);
        }
        else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
            // controller axis moved (stick motion)
            lastControllerUseTicks = SDL_GetTicks();
            if (infoText) infoText->setLastInputIsController(true);
        }
    }

    const bool* keys = SDL_GetKeyboardState(nullptr);
    // update controller polling every frame
    controller.update();
    auto cs = controller.getState();

    // Track last-used input device: update InfoText only when an input is detected
    if (infoText) {
        bool keyboardInput = false;
        if (keys) {
            keyboardInput = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] ||
                            keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_J] || keys[SDL_SCANCODE_K] ||
                            keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_DOWN];
        }
        bool controllerInput = cs.connected && (cs.left || cs.right || cs.up || cs.down || cs.jump || cs.attack || cs.attackCharged);
        uint32_t now = SDL_GetTicks();
        if (keyboardInput) {
            lastKeyboardUseTicks = now;
            infoText->setLastInputIsController(false);
        }
        if (controllerInput) {
            lastControllerUseTicks = now;
            infoText->setLastInputIsController(true);
        }
        // If neither produced input this frame, prefer the device with the most recent timestamp
        if (!keyboardInput && !controllerInput) {
            if (lastControllerUseTicks > lastKeyboardUseTicks) infoText->setLastInputIsController(true);
            else infoText->setLastInputIsController(false);
        }
    }

    // If we are currently ignoring held Up input (to avoid immediate re-wrapping),
    // clear the flag once Up is released on both keyboard and controller.
    if (inMenu == false && gEngine && gEngine->ignoreWrapUp) {
        bool upHeld = false;
        if (keys && (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])) upHeld = true;
        if (cs.connected && cs.up) upHeld = true;
        if (!upHeld) gEngine->ignoreWrapUp = false;
    }

    // If in game over, route input only to gameOver UI
    if (inGameOver && gameOver) {
        if (cs.connected) gameOver->handleInput(cs);
        else gameOver->handleInput(keys);
        return;
    }

    // Only forward input to player if movement is allowed
    if (!inMenu) {
        if (player && !transitioning) {
            // If K pressed on keyboard, always prioritize keyboard so charge starts
            if (keys && keys[SDL_SCANCODE_K]) {
                player->input(keys);
            }
            else {
                // Prefer controller only if it is actively providing input; otherwise keyboard.
                bool keyboardActive = false;
                if (keys) {
                    keyboardActive = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] ||
                                     keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_J] || keys[SDL_SCANCODE_K];
                }
                bool controllerActive = cs.connected && !keyboardActive && (cs.left || cs.right || cs.up || cs.down || cs.jump || cs.attack || cs.attackCharged);
                if (controllerActive) {
                    player->input(cs);
                } else {
                    if (player) player->input(keys);
                }
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
    if (keys[SDL_SCANCODE_1])
    {
        loadLevel(22);
    }
    if (keys[SDL_SCANCODE_8])
    {
		player->hasKey = true;
    }
    // Debug: next level (edge-detected so holding key doesn't skip multiple)
    bool nextKey = keys[SDL_SCANCODE_5];
    if (nextKey && !debugNextPressedLastFrame) {
        int newLevel = currentLevelID + 1;
        if (newLevel > 100) newLevel = 100;
        if (newLevel != currentLevelID) loadLevel(newLevel);
		std::cout << "Debug: loading next level " << newLevel << "\n";
    }
    debugNextPressedLastFrame = nextKey;

    // Debug: previous level (edge-detected)
    bool prevKey = keys[SDL_SCANCODE_4];
    if (prevKey && !debugPrevPressedLastFrame) {
        int newLevel = currentLevelID - 1;
        if (newLevel < 1) newLevel = 1;
        if (newLevel != currentLevelID) loadLevel(newLevel);
        std::cout << "Debug: loading previous level " << newLevel << "\n";
    }
    debugPrevPressedLastFrame = prevKey;
}

// --------------------------------------------------
void Engine::update()
{
    if (currentLevelID == 39)
		sound->playSfx("orc_laugh");
    playerLastFacing = player->obj.facing;
    // advance autonomous background scrolling (fixed step)
    const float bgDt = 1.0f / 60.0f;
    for (auto* b : backgrounds) if (b) b->update(bgDt);

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
	if (currentLevelID >= 39)
        sound->stopMusic();
    if (!transitioning && player)
    {
        int tx = int(player->obj.x) / TILE_SIZE;
        int ty = int(player->obj.y) / TILE_SIZE;

        if (tx >= 0 && ty >= 0 && tx < map.width && ty < map.height)
        {
            int t = map.spawn[ty * map.width + tx];
            // Handle vertical wrap-down (go +10)
            if (t == WRAPD) {
                // Require pressing UP to trigger vertical wrap (player must press up while standing on tile)
                const bool* keys = SDL_GetKeyboardState(nullptr);
                auto cs = controller.getState();
                bool upPressed = false;
                if (keys && (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])) upPressed = true;
                if (cs.connected && cs.up) upPressed = true;
                if (upPressed && !ignoreWrapUp) {
                    int newLevel = currentLevelID + 10;
                    if (newLevel > 100) newLevel = 100;
                    if (newLevel != currentLevelID) {
                        pendingLevelID = newLevel;
                        // Mark that we entered via a downward wrap so the destination
                        // level will search for the opposite WRAPU tile when placing
                        // the player.
                        entryDirection = WRAPD;
                        transitioning = true;
                        // Ignore held Up until it is released to avoid immediately wrapping back
                        ignoreWrapUp = true;
                        transitionTimer = TRANSITION_DURATION;
                        if (player) player->obj.canMove = false;
                    }
                }
            }
            // Handle vertical wrap-up (go -10) — requires pressing up
            else if (t == WRAPU) {
                const bool* keys = SDL_GetKeyboardState(nullptr);
                auto cs = controller.getState();
                bool upPressed = false;
                if (keys && (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])) upPressed = true;
                if (cs.connected && cs.up) upPressed = true;
                if (upPressed && !ignoreWrapUp) {
                    int newLevel = currentLevelID - 10;
                    if (newLevel < 1) newLevel = 1;
                    if (newLevel != currentLevelID) {
                        pendingLevelID = newLevel;
                        // Mark that we entered via an upward wrap so the destination
                        // level will search for the opposite WRAPD tile when placing
                        // the player.
                        entryDirection = WRAPU;
                        transitioning = true;
                        // Ignore held Up until it is released to avoid immediately wrapping back
                        ignoreWrapUp = true;
                        transitionTimer = TRANSITION_DURATION;
                        if (player) player->obj.canMove = false;
                    }
                }
            }
            else if (t == LEFT || t == RIGHT || t == UP || t == DOWN || t == WRAP)
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
        // Preserve previous X/Y to determine movement direction for collision resolution
        float prevPlayerX = player->obj.x;
        float prevPlayerY = player->obj.y;
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
                        player->obj.x = e->obj.x + player->obj.tileWidth;
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
                    float aCenter = ar.x + ar.w + 0.5f;
                    if (pCenter < aCenter)
                        player->obj.x = a->obj.x - player->obj.tileWidth;
                    else
                        player->obj.x = a->obj.x + player->obj.tileWidth;
                }

                player->obj.velx = 0.0f;
                prevPlayerX = player->obj.x;
            }
        }

        // Prevent passing through closed doors: resolve collisions against Door MapObjects
        for (auto* mo : objects) {
            if (!mo) continue;
            // only consider Door instances
            Door* d = dynamic_cast<Door*>(mo);
            if (!d || !d->active) continue;
            if (d->isOpen()) continue; // open doors are passable

            SDL_FRect pr = player->getRect();
            SDL_FRect dr = d->getRect();
            if (SDL_HasRectIntersectionFloat(&pr, &dr)) {
                if (player->obj.x > prevPlayerX) {
                    // moved right into door
                    player->obj.x = dr.x - player->obj.tileWidth;
                } else if (player->obj.x < prevPlayerX) {
                    // moved left into door
                    player->obj.x = dr.x + dr.w;
                } else {
                    float pCenter = pr.x + pr.w * 0.5f;
                    float dCenter = dr.x + dr.w * 0.5f;
                    if (pCenter < dCenter)
                        player->obj.x = dr.x - player->obj.tileWidth;
                    else
                        player->obj.x = dr.x + dr.w;
                }
                player->obj.velx = 0.0f;
                prevPlayerX = player->obj.x;
            }
        }

        // Prevent passing through crates and allow pushing them by walking into them
        // Allow landing on thin pressure plates (2px) so player can stand slightly above ground.
        for (auto* mo : objects) {
            if (!mo || !mo->active) continue;
            PressurePlate* pp = dynamic_cast<PressurePlate*>(mo);
            if (!pp) continue;

            // hitbox top (anchored 2px above the tile bottom)
            SDL_FRect pr = player->getRect();
            SDL_FRect plateRect = pp->getRect();
            float plateTop = plateRect.y + (plateRect.h - 2.0f); // y position of the 2px trigger

            // quick horizontal overlap check
            float overlapX = std::min(pr.x + pr.w, plateRect.x + plateRect.w) - std::max(pr.x, plateRect.x);
            if (overlapX <= 0) continue;

            // require downward crossing of the plate's top to count as landing
            float prevBottom = prevPlayerY + player->obj.tileHeight;
            float currBottom = pr.y + pr.h;
            // Only snap if player was strictly above plate previously and now crosses its top while moving down
            if (prevBottom < plateTop && currBottom >= plateTop && player->obj.vely >= 0.0f) {
                player->obj.y = plateTop - player->obj.tileHeight;
                player->obj.vely = 0.0f;
                player->obj.onGround = true;
                prevPlayerX = player->obj.x;
            }
        }

        for (auto* mo : objects) {
            if (!mo || !mo->active) continue;
            Crate* c = dynamic_cast<Crate*>(mo);
            if (!c) continue;

            SDL_FRect pr = player->getRect();
            SDL_FRect cr = c->getRect();
            if (!SDL_HasRectIntersectionFloat(&pr, &cr)) continue;

            // compute overlap extents
            float overlapX = std::min(pr.x + pr.w, cr.x + cr.w) - std::max(pr.x, cr.x);
            float overlapY = std::min(pr.y + pr.h, cr.y + cr.h) - std::max(pr.y, cr.y);

            // Decide whether collision is primarily vertical (landing on crate) or horizontal (walking into crate)
            bool verticalCollision = false;
            float prevBottom = prevPlayerY + player->obj.tileHeight;
            float crateTop = cr.y;
            // If the player's previous bottom was at or above the crate top (within tolerance), prefer vertical landing
            if (prevBottom <= crateTop + 6.0f) verticalCollision = true;
            // Also, if vertical overlap is smaller than horizontal overlap, treat as vertical
            if (overlapY <= overlapX) verticalCollision = true;

            if (verticalCollision) {
                // Snap player to top of crate so they can stand even if positions are not tile-aligned
                player->obj.y = crateTop - player->obj.tileHeight;
                player->obj.vely = 0.0f;
                player->obj.onGround = true;
                // Gently carry player horizontally by crate velocity
                player->obj.x += c->velx;
                prevPlayerX = player->obj.x;
            }
            else {
                // horizontal resolution (player walked into crate)
                if (player->obj.x > prevPlayerX) {
                    // moved right into crate
                    player->obj.x = cr.x - player->obj.tileWidth;
                } else if (player->obj.x < prevPlayerX) {
                    // moved left into crate
                    player->obj.x = cr.x + cr.w;
                } else {
                    // no horizontal movement this frame — nudge based on centers
                    float pCenter = pr.x + pr.w * 0.5f;
                    float cCenter = cr.x + cr.w * 0.5f;
                    if (pCenter < cCenter) {
                        player->obj.x = cr.x - player->obj.tileWidth;
                    } else {
                        player->obj.x = cr.x + cr.w;
                    }
                }

                // stop player's horizontal movement; do NOT modify crate velocity here — only attacks move crates
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
    // Key pickup detection: if player touches a MapObject with spawn id 10, give key and deactivate it
    if (player && player->obj.alive && !inGameOver) {
        for (auto* mo : objects) {
            if (!mo || !mo->active) continue;
            if (mo->getTileIndex() == 10) {
                SDL_FRect pr = player->getRect();
                SDL_FRect mr = mo->getRect();
                if (SDL_HasRectIntersectionFloat(&pr, &mr)) {
                    player->hasKey = true;
                    mo->active = false; // remove key from world
                    // persist collection so key won't respawn later
                    markKeyCollected(currentLevelID, mo->getTileX(), mo->getTileY());
                    if (gSound) gSound->playSfx("arrow", 128, false);
                    SDL_Log("Picked up key at (%d,%d) on level %d", mo->getTileX(), mo->getTileY(), currentLevelID);
                    break; // only collect one key per frame
                }
            }
        }
    }

    for (auto it = projectiles.begin(); it != projectiles.end(); ) {
        Arrow* a = *it;
        a->update(map);

        // Early crate handling: any arrow (archer or trap) should be able to hit/push crates.
        // Do this before player/enemy collisions so arrows interact with world objects first.
        if (a->alive) {
            SDL_FRect ar = a->getRect();
            for (auto* mo : objects) {
                if (!mo || !mo->active) continue;
                Crate* c = dynamic_cast<Crate*>(mo);
                if (!c) continue;
                SDL_FRect cr = c->getRect();
                if (SDL_HasRectIntersectionFloat(&ar, &cr) && c->movable && c->hitInvuln == 0) {
                    float hNudge = a->isTrapArrow ? 2.0f : 4.0f;
                    float vNudge = a->isTrapArrow ? -1.5f : -3.0f;
                    float dir = (a->getVelX() > 0.0f) ? 1.0f : -1.0f;
                    c->velx = dir * hNudge;
                    if (c->onGround) c->vely = vNudge;
                    c->hitInvuln = 8;
                    if (gSound) gSound->playSfx("clang", 128, false);
                    a->alive = false;
                    if (gEngine) gEngine->triggerHitstop(6);
                    break;
                }
            }
        }

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

        // If this is a trap arrow, it should also hurt other game objects (orcs and archers)
        if (a->alive && a->isTrapArrow) {
            SDL_FRect ar = a->getRect();
            float attackerX = ar.x + ar.w * 0.5f;

            // Check orcs
            for (auto* o : orc) {
                if (!o || !o->obj.alive) continue;
                SDL_FRect orcR = o->getRect();
                if (SDL_HasRectIntersectionFloat(&ar, &orcR)) {
                    o->takeDamage(15.0f, attackerX, 3, 6, 30, 2.5f, -4.0f);
                    a->alive = false;
                    if (gSound) gSound->playSfx("arrow_impact");
                    if (gEngine) gEngine->triggerHitstop(6);
                    break;
                }
            }

            // Check archers (if arrow still alive)
            if (a->alive) {
                for (auto* archer : archers) {
                    if (!archer || !archer->obj.alive) continue;
                    SDL_FRect arR = archer->getRect();
                    if (SDL_HasRectIntersectionFloat(&ar, &arR)) {
                        archer->takeDamage(15.0f, attackerX, 3, 6, 30, 2.5f, -4.0f);
                        a->alive = false;
                        if (gSound) gSound->playSfx("arrow_impact");
                        if (gEngine) gEngine->triggerHitstop(6);
                        break;
                    }
                }
            }

            // For any arrow (archer or trap), also allow it to hit/push crates
            if (a->alive) {
                SDL_FRect ar = a->getRect();
                for (auto* mo : objects) {
                    if (!mo || !mo->active) continue;
                    Crate* c = dynamic_cast<Crate*>(mo);
                    if (!c) continue;
                    SDL_FRect cr = c->getRect();
                    if (SDL_HasRectIntersectionFloat(&ar, &cr) && c->movable && c->hitInvuln == 0) {
                        float hNudge = a->isTrapArrow ? 2.0f : 4.0f;
                        float vNudge = a->isTrapArrow ? -1.5f : -3.0f;
                        float dir = (a->getVelX() > 0.0f) ? 1.0f : -1.0f;
                        c->velx = dir * hNudge;
                        if (c->onGround) c->vely = vNudge;
                        c->hitInvuln = 8;
                        if (gSound) gSound->playSfx("clang", 128, false);
                        a->alive = false;
                        if (gEngine) gEngine->triggerHitstop(6);
                        break;
                    }
                }
            }

            // (trap-only block ends) - crate handling moved below for all arrows
        }

        if (!a->alive) { delete a; it = projectiles.erase(it); }
        else ++it;
    }

    // Update potions and handle pickups / removal
    for (auto itp = potions.begin(); itp != potions.end(); ) {
        Potion* p = *itp;
        if (!p) { itp = potions.erase(itp); continue; }
        p->update(map);
        if (!p->obj.alive) { delete p; itp = potions.erase(itp); }
        else ++itp;
    }

    // spawn potions from dead enemies (10% chance)
    for (auto it = orc.begin(); it != orc.end(); ) {
        Orc* e = *it;
        if (!e || !e->obj.alive) {
            // determine spawn chance
            int r = rand() % 100;
			if (r < 20) { // 20% chance to spawn potion
                // choose potion type randomly with 7:3 health:mana ratio
                int t = (rand() % 10) < 7 ? 0 : 1; // 0 = health (70%), 1 = mana (30%)
                float spawnX = e ? e->obj.x : 0.0f;
                float spawnY = (e ? e->obj.y : 0.0f) - 32.0f; // spawn 32px above ground
                std::string path = (t == 0) ? "Assets/Sprites/healthPot.png" : "Assets/Sprites/manaPot.png";
                Potion* p = new Potion(renderer, path, 16, 16, spawnX, spawnY, t == 0 ? Potion::Type::HEALTH : Potion::Type::MANA);
                potions.push_back(p);
            }
            delete e;
            it = orc.erase(it);
        } else ++it;
    }

    // If in game over, check for selection
    if (inGameOver && gameOver) {
        gameOver->update();
        int sel = gameOver->consumeSelection();
        if (sel != -1) {
            if (sel == 0) {
                // Restart: reload the starting level and return to menu
                inGameOver = false;
                // If a checkpoint exists, enable respawnFromCheckpoint and restart at that level; otherwise default to 22
                if (lastCheckpointLevel >= 1) {
                    respawnFromCheckpoint = true;
                    currentLevelID = lastCheckpointLevel;
                } else {
                    respawnFromCheckpoint = false;
                    currentLevelID = 22;
                }
                loadLevel(currentLevelID);
                if (player) player->obj.health = player->obj.maxHealth;
                if (player) player->obj.magic = player->obj.maxMagic;
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

    // update infoText: supply markup and player's world X, InfoText handles positioning and visibility
    if (infoText && player) {
        // single call: InfoText will choose which message/icon to show based on level and input device
        // Do not override the last-input state here — it is managed in handleEvents when input is detected.
        infoText->updateAuto(currentLevelID, player->obj.x + player->obj.tileWidth * 0.5f, &map);
    }
}

// --------------------------------------------------

void Engine::render()
{
    //SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (inMenu) {
        if (menu) menu->render(renderer);
        SDL_RenderPresent(renderer);
        return;
    }

    if (inGameOver && gameOver) {
        // Draw last frame of level under overlay
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // draw background for current level if any
        for (auto* b : backgrounds) {
            if (b && b->matchesLevel(currentLevelID)) {
                b->draw(renderer, camera.x, camera.y, SCREEN_W, SCREEN_H, map.height * TILE_SIZE);
                break;
            }
        }

        map.draw(renderer, camera.x, camera.y);
        for (auto* o : orc)
            o->draw(renderer, camera.x, camera.y);
        for (auto* f : fallT)
            f->draw(renderer, camera.x, camera.y);
        
        // Draw only small objects (tile-sized) here. Taller objects (e.g. 16x32 water)
        // are drawn later so they can appear in front of the player.
        for (auto* mo : objects) {
            if (!mo) continue;
            if (mo->getHeight() <= Map::TILE_SIZE) mo->draw(renderer, camera.x, camera.y, map);
        }
        for (auto* a : archers)
            a->draw(renderer, camera.x, camera.y);
        for (auto* p : projectiles)
            p->draw(renderer, camera.x, camera.y);

        if (player) player->draw(renderer, camera.x, camera.y);

        // draw foreground layer in front of player
        map.drawForeground(renderer, camera.x, camera.y);

        // Draw HUD including magic
        if (hud && player) hud->draw(renderer, player->obj.health, player->obj.maxHealth, player->obj.magic, player->obj.maxMagic, player->hasKey);

        gameOver->render(renderer);
        SDL_RenderPresent(renderer);
        return;
    }

    //SDL_SetRenderDrawColor(renderer, 31, 14, 28, 255);
    SDL_RenderClear(renderer);

    // draw background for current level if any
    for (auto* b : backgrounds) {
        if (b && b->matchesLevel(currentLevelID)) {
            b->draw(renderer, camera.x, camera.y, SCREEN_W, SCREEN_H, map.height * TILE_SIZE);
            break;
        }
    }
    
    map.drawForeground(renderer, camera.x, camera.y);

    // Draw objects that should appear behind the player (e.g., waterfall backdrops)
    for (auto* mo : objects) {
        if (!mo) continue;
        if (mo->drawBehind) mo->draw(renderer, camera.x, camera.y, map);
    }

    for (auto* o : orc)
        o->draw(renderer, camera.x, camera.y);

    // ---- draw traps / objects ----
    for (auto* f : fallT)
        f->draw(renderer, camera.x, camera.y);
    
    for (auto* mo : objects) {
        //if (!mo) continue;
        if (mo->getHeight() <= Map::TILE_SIZE) mo->draw(renderer, camera.x, camera.y, map);
    }

    for (auto* a : archers)
        a->draw(renderer, camera.x, camera.y);

    for (auto* p : projectiles)
        p->draw(renderer, camera.x, camera.y);

    for (auto* pot : potions)
        if (pot) pot->draw(renderer, camera.x, camera.y);

    if (player) player->draw(renderer, camera.x, camera.y);

    // draw base map layer on top of sprites if necessary (preserve previous behavior)
    map.draw(renderer, camera.x, camera.y);
    

    // Draw tall/overlay objects after map so they appear on top of player/tiles
    for (auto* mo : objects) {
        if (!mo) continue;
        // Objects that were drawn behind the player are already rendered earlier
        if (mo->drawBehind) continue;
        if (mo->getHeight() > Map::TILE_SIZE) mo->draw(renderer, camera.x, camera.y, map);
    }

    // Draw HUD last so it's on top (include key indicator)
    if (hud && player) hud->draw(renderer, player->obj.health, player->obj.maxHealth, player->obj.magic, player->obj.maxMagic, player->hasKey);

    // Draw in-world info text (after world rendering but before HUD maybe)
    if (infoText) infoText->draw(renderer, camera);

    if (transitioning)
    {
        // Immediately black out the screen during transitions (no fade)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
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
    // Preserve player magic across loads
    float savedMagic = 100.0f;
    float savedMaxMagic = 100.0f;
    // Preserve whether player had a key
    bool savedHasKey = false;
    if (player) {
        savedHealth = player->obj.health;
        savedMaxHealth = player->obj.maxHealth;
        savedMagic = player->obj.magic;
        savedMaxMagic = player->obj.maxMagic;
        savedHasKey = player->hasKey;
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
    // attempt to load optional foreground layer (Tile Layer 2)
    map.loadLayer2CSV("Maps/" + std::string(name) + "_Tile Layer 2.csv");

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
    // For vertical wrap transitions, find the opposite wrap tile in the destination
    if (entryDirection == WRAPD) portalToFind = WRAPU;
    if (entryDirection == WRAPU) portalToFind = WRAPD;
    if (entryDirection == WRAP)  portalToFind = WRAP;

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
                    if (portalToFind == DOWN)  spawnY;
                    if (portalToFind == WRAPU)  spawnX;
                    if (portalToFind == WRAPD)  spawnX;
                    if (portalToFind == WRAP && playerLastFacing)  spawnX += TILE_SIZE;
                    if (portalToFind == WRAP && !playerLastFacing)  spawnX -= TILE_SIZE;

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
    // Restore persisted magic if available
    player->obj.magic = savedMagic;
    player->obj.maxMagic = savedMaxMagic;
    // Restore whether player had key
    player->hasKey = savedHasKey;

    setPlayerFacingFromEntry(entryDirection);

    // ----------------------------------------
    // Spawn MAP OBJECTS (NOT PLAYER)
    // ----------------------------------------
    int orcNum = -1;
    auto spawns = map.getObjectSpawns();

    // track which spawn tiles we've consumed when forming multi-tile platforms
    std::vector<char> processed(map.width * map.height, 0);
    for (const auto& s : spawns)
    {
        int sx = s.x;
        int sy = s.y;
        int idx = sy * map.width + sx;
        if (processed[idx]) continue;

        int t = s.tileIndex;

        // Multi-tile platform starting with left edge (363)
        if (t == 363) {
            std::vector<int> tiles;
            int curx = sx;
            // collect left (363), zero or more middle (364), optional right (365)
            for (;; ++curx) {
                if (curx >= map.width) break;
                int st = map.spawn[sy * map.width + curx];
                if (curx == sx) {
                    if (st != 363) break; // sanity
                    tiles.push_back(st);
                    processed[sy * map.width + curx] = 1;
                    continue;
                }
                if (st == 364) {
                    tiles.push_back(st);
                    processed[sy * map.width + curx] = 1;
                    continue;
                }
                if (st == 365) {
                    tiles.push_back(st);
                    processed[sy * map.width + curx] = 1;
                    break;
                }
                // stop if encounter anything else
                break;
            }
            // create platform from collected tiles
            if (!tiles.empty()) {
                objects.push_back(new FallingPlatform(sx, sy, tiles));
            }
            continue;
        }

        // Single-tile platform (366)
        if (t == 366) {
            processed[idx] = 1;
            objects.push_back(new FallingPlatform(sx, sy, std::vector<int>{366}));
            continue;
        }

        // Non-platform spawns
        processed[idx] = 1;
        float px = float(sx * Map::TILE_SIZE);
        float py = float(sy * Map::TILE_SIZE);

        switch (t)
        {
        case Map::SPAWN_PLAYER:
            // 🚫 Player already handled
            break;

        case Map::SPAWN_ORC:
            orc.push_back(new Orc(renderer,"Assets/Sprites/orc.png",12, 16,px, py , 20));
            orcNum++;
            break;
        case Map::SPAWN_CHECKPOINT:
        {
            Checkpoint* cp = new Checkpoint(sx, sy, s.tileIndex);
            cp->setLevel(levelID);
            // Only move the player's spawn to the checkpoint position if we're currently respawning from a checkpoint
            // (i.e., the player died and selected Restart). This prevents normal portal transitions from using checkpoints.
            if (respawnFromCheckpoint && levelID == lastCheckpointLevel) {
                player->obj.x = float(sx * Map::TILE_SIZE);
                player->obj.y = float(sy * Map::TILE_SIZE);
            }
            objects.push_back(cp);
        }
        break;

        case Map::SPAWN_WATERFALL_LONG:
        {
            // Narrow tall waterfall (16x32, 4 frames) that sits behind the player
            MapObject* mo = new MapObject(sx, sy, s.tileIndex);
            mo->setSize(16, 32);
            if (!mo->loadAnimation(renderer, "Assets/Sprites/waterfall_long.png", 16, 32, 4, 0, 0, 0, 16, 32, 12)) {
                SDL_Log("Failed to load waterfall_long animation");
            }
            // move up by 16px so bottom aligns with tile
            mo->offsetPosition(0, -16);
            mo->drawBehind = true;
            objects.push_back(mo);
        }
        break;
        case Map::SPAWN_DUNGEON_WATER:
        {
            // Dungeon water sprite that should appear in front of the player
            MapObject* mo = new MapObject(sx, sy, s.tileIndex);
            mo->setSize(16, 48);
            if (!mo->loadAnimation(renderer, "Assets/Sprites/dungeon_water.png", 16, 48, 4, 0, 0, 0, 16, 48, 12)) {
                SDL_Log("Failed to load dungeon_water animation");
            }
            // Move up 32px so bottom aligns with tile
            mo->offsetPosition(0, -32);
            // default drawBehind is false, so this will render in front of the player
            objects.push_back(mo);
        }
        break;
        case Map::SPAWN_WATER:
        {
            // Create a generic MapObject and load a water animation for it
            MapObject* mo = new MapObject(sx, sy, s.tileIndex);
            mo->setSize(16, 32);
            if (!mo->loadAnimation(renderer, "Assets/Sprites/water.png", 16, 32, 4, 0, 0, 0, 16, 32, 12)) {
                SDL_Log("Failed to load water animation");
            }
            // Sprite is taller (32) than tile (16). Move object up one tile so bottoms align.
            mo->offsetPosition(0, -Map::TILE_SIZE);
            objects.push_back(mo);
        }
        break;
        case Map::SPAWN_WATERFALL_DAY:
        {
            // Create a generic MapObject and load a water animation for it
            MapObject* mo = new MapObject(sx, sy, s.tileIndex);
            mo->setSize(48, 64);
            if (!mo->loadAnimation(renderer, "Assets/Sprites/waterfall_day.png", 48, 64, 5, 0, 0, 0, 48, 64, 12)) {
                SDL_Log("Failed to load waterfall animation");
            }
            //mo->offsetPosition(-48-16, -64-16);
            objects.push_back(mo);
        }
        break;

        case Map::SPAWN_WATERFALL:
        {
            // Create a generic MapObject and load a narrow waterfall animation (16x48, 4 frames)
            MapObject* mo = new MapObject(sx, sy, s.tileIndex);
            mo->setSize(16, 48);
            if (!mo->loadAnimation(renderer, "Assets/Sprites/waterfall.png", 16, 48, 4, 0, 0, 0, 16, 48, 12)) {
                SDL_Log("Failed to load waterfall animation");
            }
            // Sprite is taller (48) than tile (16). Move object up so bottoms align (48 - 16 = 32 px)
            mo->offsetPosition(0, -32);
            // Draw this waterfall behind the player
            mo->drawBehind = true;
            objects.push_back(mo);
        }
        break;

        case Map::SPAWN_SLIME:
            orc.push_back(new Orc(renderer,"Assets/Sprites/slime.png",12, 16,px, py , 10));
            orcNum++;
			orc[orcNum]->chaseSpeed = 0.4f;
            orc[orcNum]->frames.walk = 6;
            orc[orcNum]->frames.attack = 6;
            orc[orcNum]->animFlash = new AnimationManager(orc[orcNum]->tex, 100, 100, 4 , 400, 44, 42, orc[orcNum]->obj.tileWidth, orc[orcNum]->obj.tileHeight);
            break;

        case Map::SPAWN_SKELETON:
            orc.push_back(new Orc(
                renderer,
                "Assets/Sprites/Skeleton.png",
                12, 16,
                px, py, 20, true
            ));
            orcNum++;
            orc[orcNum]->chaseSpeed = 0.7f;
            break;

        case Map::SPAWN_FALLINGTRAP:
            fallT.push_back(new FallingTrap(renderer, s.x, s.y));
            break;

        case Map::SPAWN_SPIKES:
            objects.push_back(new Spikes(s.x, s.y, s.tileIndex));
            break;

        case Map::SPAWN_PRESSURE_PLATE:
        {
            PressurePlate* pp = new PressurePlate(sx, sy, s.tileIndex);
            // Configure per-level plate behavior. Use levelID to decide what this plate should do.
            // Example mapping: add cases here to customize behavior per level and per-plate coords.
            switch (levelID) {
            case 43:
                // In level 22, all pressure plates fire nearby arrow traps
                pp->setTargetAction(PressurePlate::TargetAction::FIRE_ARROWTRAPS);
                break;
            case 22:
                // In level 22, all pressure plates fire nearby arrow traps
                pp->setTargetAction(PressurePlate::TargetAction::FIRE_ARROWTRAPS);
                break;
            case 222:
                // In level 24, a specific plate opens a door at (tx,ty)
                if (sx == 12 && sy == 4) {
                    pp->setTargetAction(PressurePlate::TargetAction::OPEN_DOOR, 13, 4);
                }
                break;
            case 228:
                // In level 28, a plate drops a falling trap at specific coords
                if (sx == 9 && sy == 7) {
                    pp->setTargetAction(PressurePlate::TargetAction::DROP_FALLINGTRAP, 10, 7);
                }
                break;
            default:
                // leave as NONE by default
                break;
            }

            // Pressure plate sprite should be positioned at tile top; no vertical offset necessary
            objects.push_back(pp);
        }
        break;

        case Map::SPAWN_ARCHER:
            archers.push_back(new Archer(renderer, "Assets/Sprites/archer.png", 12, 16, px, py, 10));
            break;
        case Map::SPAWN_ARROWTRAP_LEFT:
        {
            ArrowTrap* at = new ArrowTrap(renderer, s.x, s.y, s.tileIndex);
            if (levelID == 44) at->setAutoFire(true);
            objects.push_back(at);
        }
            break;
        case Map::SPAWN_ARROWTRAP_RIGHT:
        {
            ArrowTrap* at = new ArrowTrap(renderer, s.x, s.y, s.tileIndex);
            if (levelID == 44) at->setAutoFire(true);
            objects.push_back(at);
        }
            break;
        case Map::SPAWN_DOOR:
        {
            Door* d = new Door(sx, sy, s.tileIndex);
            // load door animation: assume sprite "Assets/Sprites/door.png" with frames horizontally
            if (!d->loadAnimation(renderer, "Assets/Sprites/door.png", 32, 48, 8, 0, 0, 0, 32, 48, 8)) {
                SDL_Log("Failed to load door animation");
            }
            // set level for persistence and restore opened state if previously opened
            d->setLevel(levelID);
            if (isDoorOpened(levelID, sx, sy)) {
                d->open(false); // open silently without SFX when restoring state
            }
            objects.push_back(d);
        }
        break;
       case Map::SPAWN_KEY: // key spawn
        {
            // Only spawn key object if it hasn't already been collected
            if (!isKeyCollected(levelID, sx, sy)) {
                MapObject* mo = new MapObject(sx, sy, s.tileIndex);
                // use default tile rendering (no animation) so tile from tileset is shown
                objects.push_back(mo);
            }
        }
        break;
        case Map::SPAWN_CRATE:
        {
            Crate* c = new Crate(sx, sy, s.tileIndex);
            objects.push_back(c);
        }
        break;
        default:
            // unhandled spawn types are ignored
            break;
        }
    }

    // ----------------------------------------
    // Finalize level state
    // ----------------------------------------
    currentLevelID = levelID;
    entryDirection = -1;
    // set default last start pos to spawn unless we are resuming at a checkpoint
    if (respawnFromCheckpoint && lastCheckpointLevel == levelID) {
        // position was set earlier when spawning checkpoint objects; keep it and then clear the respawn flag
        respawnFromCheckpoint = false; // consume the checkpoint respawn
    } else {
        lastStartPosX = spawnX;
        lastStartPosY = spawnY;
    }
}


void Engine::setPlayerFacingFromEntry(int entryTile)
{
    switch (entryTile)
    {
    case LEFT: // came from LEFT
        player->obj.facing = false;
        break;

    case RIGHT: // came from RIGHT
        player->obj.facing = true;
        break;

    case UP: // came from UP
        // optional – keep previous or face down
        break;

    case DOWN: // came from DOWN
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

int main(int argc, char* argv[])
{
    // Call once at program start
    srand(static_cast<unsigned>(time(nullptr)));

    Engine engine;

    // Request VSync on the engine's renderer (attempt to lock to display refresh, typically 60Hz)
    if (engine.renderer) {
        SDL_SetRenderVSync(engine.renderer, 1);
        SDL_Log("Main: VSync requested on renderer (target ~60Hz)");
    } else {
        SDL_Log("Main: renderer not available to set VSync");
    }

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