#include "ArrowTrap.h"
#include "Arrow.h"
#include "Engine.h"
#include "GameObject.h"
#include "Player.h"
#include "Map.h"
#include "Sound.h"
#include "PressurePlate.h"

// External trigger flag: set by pressure plates when they are triggered.
// This allows arrow traps to respond even if update order means the plate was processed earlier.
void ArrowTrap::triggerExternal() {
    // Defer actual arrow spawning to update() so we can consult the Map and
    // place the arrow in a nearby non-solid tile to avoid immediate collisions.
    externalTriggered = true;
}

ArrowTrap::ArrowTrap(SDL_Renderer* renderer_, int tileX, int tileY, int tileIndex)
    : MapObject(tileX, tileY, tileIndex), renderer(renderer_)
{
    // attempt to load arrow trap animation (3 frames horizontally)
    if (gEngine && gEngine->renderer) {
        if (!loadAnimation(gEngine->renderer, "Assets/Sprites/arrow_trap.png", 16, 16, 3, 0, 0, 0, 16, 16, animPlaySpeed)) {
            SDL_Log("ArrowTrap: failed to load animation");
        } else {
            // freeze on first frame until triggered
            if (anim) anim->setSpeed(100000);
            if (anim) anim->currentFrame = 0;
        }
    }
}

void ArrowTrap::update(GameObject& obj, Map& map)
{
    if (!active) return;
    // Advance internal animation manager if present; MapObject::update handles anim->update(),
    // but we don't want to run default behaviour for MapObject here so call anim->update() directly
    if (anim) anim->update();
    // If animation is playing, check for end condition: after last frame, return to frame 0 and freeze
    if (animPlaying && anim) {
        if (anim->currentFrame >= anim->frameCount - 1 && anim->timer == 0) {
            // reached final frame and completed its duration — reset
            animPlaying = false;
            anim->currentFrame = 0;
            anim->setSpeed(100000);
        }
    }
    if (!obj.obj.alive) return;

    // Determine facing from tileIndex: use map spawn constants
    bool shootLeft = (tileIndex == Map::SPAWN_ARROWTRAP_LEFT);
    bool shootRight = (tileIndex == Map::SPAWN_ARROWTRAP_RIGHT);
    if (!shootLeft && !shootRight) return;

    // Player center
    SDL_FRect pr = obj.getRect();
    float pCenterX = pr.x + pr.w * 0.5f;
    float pCenterY = pr.y + pr.h * 0.5f;

    // Trap bounds
    SDL_FRect tr = getRect();
    float trapLeft = tr.x;
    float trapRight = tr.x + tr.w;

    bool inFront = false;
    if (shootLeft) {
        // player passes to left of trap horizontally within same vertical band
        inFront = (pCenterX < trapLeft) && (pCenterY > tr.y - 8 && pCenterY < tr.y + tr.h + 8);
    } else if (shootRight) {
        inFront = (pCenterX > trapRight) && (pCenterY > tr.y - 8 && pCenterY < tr.y + tr.h + 8);
    }

    if (cooldownTimer > 0) --cooldownTimer;
    // Also check for pressure plates in front of the trap. If any plate in the trap's firing
    // zone is currently triggered and it was not triggered in the previous frame, treat that
    // as a firing event (rising edge). This allows remote activation by standing on plates.
    bool plateTriggeredNow = false;
    if (gEngine) {
        // check all map objects for PressurePlate instances
        for (auto* mo : gEngine->objects) {
            if (!mo || !mo->active) continue;
            PressurePlate* pp = dynamic_cast<PressurePlate*>(mo);
            if (!pp) continue;

            // Only consider plates roughly in front of trap (within same vertical band +/- 8 px)
            SDL_FRect pr = pp->getRect();
            float pCenterX = pr.x + pr.w * 0.5f;
            float pCenterY = pr.y + pr.h * 0.5f;
            if (pCenterY < tr.y - 8 || pCenterY > tr.y + tr.h + 8) continue;

            if (shootLeft && pCenterX < trapLeft) plateTriggeredNow = plateTriggeredNow || pp->isTriggered();
            if (shootRight && pCenterX > trapRight) plateTriggeredNow = plateTriggeredNow || pp->isTriggered();
            if (plateTriggeredNow) break;
        }
    }

    // If a plate has been newly triggered (rising edge), consider that an activation event
    bool plateRising = (plateTriggeredNow && !plateWasTriggered);

    bool activationNow = (inFront && !playerWasInZone) || plateRising || externalTriggered;

    if (activationNow && cooldownTimer <= 0) {
        if (ammo > 0) {
            // choose a spawn X located in the nearest non-solid tile in the firing direction
            const float arrowW = 19.0f;
            float sy = tr.y + tr.h * 0.5f; // vertical center; Arrow ctor will center vertically
            int trapCenterTileX = int((tr.x + tr.w * 0.5f) / Map::TILE_SIZE);
            int spawnTileY = int(sy / Map::TILE_SIZE);
            int foundTx = -1;
            if (shootLeft) {
                for (int d = 1; d <= 3; ++d) {
                    int tx = trapCenterTileX - d;
                    if (tx < 0) break;
                    if (map.getCollision(tx, spawnTileY) == -1) { foundTx = tx; break; }
                }
            } else {
                for (int d = 1; d <= 3; ++d) {
                    int tx = trapCenterTileX + d;
                    if (tx >= map.width) break;
                    if (map.getCollision(tx, spawnTileY) == -1) { foundTx = tx; break; }
                }
            }
            float sx;
            if (foundTx != -1) {
                sx = foundTx * Map::TILE_SIZE + Map::TILE_SIZE * 0.5f - arrowW * 0.5f;
            } else {
                // fallback: center of trap
                sx = tr.x + tr.w * 0.5f - arrowW * 0.5f;
            }
            float speed = 4.0f;
            float vx = shootLeft ? -speed : speed;
            float vy = 0.0f;

            if (gEngine) {
                Arrow* a = new Arrow(gEngine->renderer, sx, sy, vx, vy, true); // use trap sprite
                gEngine->projectiles.push_back(a);
                if (gSound) gSound->playSfx("arrow");
                SDL_Log("ArrowTrap: fired arrow from trap at tile (%d,%d)", getTileX(), getTileY());
                // start trap firing animation (play once)
                if (anim) {
                    anim->reset();
                    anim->setSpeed(animPlaySpeed);
                    animPlaying = true;
                }
            }

            --ammo;
            cooldownTimer = cooldownTicks;
        }
        else {
            if (gSound) gSound->playSfx("arrow_empty");
            // give a short cooldown so empty sound doesn't spam every frame while player remains in zone
            cooldownTimer = cooldownTicks / 2;
        }
    }

    playerWasInZone = inFront;
    plateWasTriggered = plateTriggeredNow;
    // clear external trigger after use (or if activation attempted this frame)
    if (externalTriggered) externalTriggered = false;
}


