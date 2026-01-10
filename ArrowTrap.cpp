#include "ArrowTrap.h"
#include "Arrow.h"
#include "Engine.h"
#include "GameObject.h"
#include "Player.h"
#include "Map.h"
#include "Sound.h"

ArrowTrap::ArrowTrap(SDL_Renderer* renderer_, int tileX, int tileY, int tileIndex)
    : MapObject(tileX, tileY, tileIndex), renderer(renderer_)
{
}

void ArrowTrap::update(GameObject& obj, Map& map)
{
    if (!active) return;
    if (!obj.obj.alive) return;

    // Determine facing from tileIndex: 101 = left, 102 = right
    bool shootLeft = (tileIndex == 101);
    bool shootRight = (tileIndex == 102);
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

    if (inFront && !playerWasInZone && cooldownTimer <= 0) {
        if (ammo > 0) {
            // spawn arrow from the center of the trap sprite
            // Arrow width in Arrow.cpp is 19, so offset by half to center
            const float arrowHalfW = 9.5f;
            float sx = tr.x + tr.w * 0.5f - arrowHalfW;
            float sy = tr.y + tr.h * 0.5f; // vertical center; Arrow ctor will center vertically
            float speed = 4.0f;
            float vx = shootLeft ? -speed : speed;
            float vy = 0.0f;

            if (gEngine) {
                Arrow* a = new Arrow(gEngine->renderer, sx, sy, vx, vy, true); // use trap sprite
                gEngine->projectiles.push_back(a);
                if (gSound) gSound->playSfx("arrow");
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
}


