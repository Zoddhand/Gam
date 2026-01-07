#include "FallingTrap.h"
#include "GameObject.h"
#include <iostream>

FallingTrap::FallingTrap(SDL_Renderer* renderer, int tileX, int tileY)
    : WorldObject(renderer, "Assets/Sprites/falling_trap.png", 16, 16)
{

    x = tileX * Map::TILE_SIZE;
    y = tileY * Map::TILE_SIZE;
}

void FallingTrap::checkTrigger(GameObject& actor) {
    if (triggered || falling || !alive) return;

    SDL_FRect a = actor.getRect();
    float actorCenterX = a.x + a.w * 0.5f;

    float trapLeft = x;
    float trapRight = x + width;

    bool horizontallyUnder =
        actorCenterX >= trapLeft &&
        actorCenterX <= trapRight;

    bool actorBelow = (a.y > y);

    if (horizontallyUnder && actorBelow) {
        triggered = true;
        delayTimer = delayTicks;
        triggerer = &actor;
    }
}

void FallingTrap::update(Map& map) {
    if (!alive) return;
    remove();
    if (triggered && !falling) {
        if (--delayTimer <= 0) {
            falling = true;
            velY = 0.5f;
        }
        return;
    }

    if (!falling) return;

    velY += gravity;
    y += velY;

     if (triggerer && triggerer->obj.alive) {
            SDL_FRect trapRect = getRect();
            SDL_FRect actorRect = triggerer->getRect();
            if (SDL_HasRectIntersectionFloat(&trapRect, &actorRect)) {
                // apply damage via GameObject::takeDamage so flashing and knockback occur
                // attackerX is trap center X so knockback pushes actor away from trap
                float trapCenterX = x + 16 * 0.5f;
                // kbX: horizontal knockback magnitude, kbY: vertical nudge (negative = upward)
                triggerer->takeDamage(10, trapCenterX, 3 /*flashes*/, 6 /*interval*/, 30 /*invulnTicks*/, 4.0f /*kbX*/, -4.0f /*kbY*/, 12 /*kbTicks*/);

                // deactivate trap after hitting
                alive = false;
                return;
            }
        }

    // Ground collision
    int leftTile = int(x / Map::TILE_SIZE);
    int rightTile = int((x + width - 1) / Map::TILE_SIZE);
    int bottomTile = int((y + height - 1) / Map::TILE_SIZE);

    if (map.isSolid(leftTile, bottomTile) ||
        map.isSolid(rightTile, bottomTile))
    {
        y = bottomTile * Map::TILE_SIZE - height;
        falling = false;
        triggered = false;
        finishedFalling = true;
    }
}

void FallingTrap::remove()
{
    if(finishedFalling == true)
        killTimer++;
    if (killTimer >= 50)
        alive = false;
}