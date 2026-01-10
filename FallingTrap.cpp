#include "FallingTrap.h"
#include "GameObject.h"
#include "Sound.h"
#include <iostream>

FallingTrap::FallingTrap(SDL_Renderer* renderer, int tileX, int tileY)
    : WorldObject(renderer, "Assets/Sprites/falling_trap.png", 16, 16)
{

    x = tileX * Map::TILE_SIZE;
    y = tileY * Map::TILE_SIZE;
}

void FallingTrap::checkTrigger(GameObject& actor, Map& map) {
    if (triggered || falling || !alive) return;

    SDL_FRect a = actor.getRect();
    float actorCenterX = a.x + a.w * 0.5f;

    float trapLeft = x;
    float trapRight = x + width;

    bool horizontallyUnder =
        actorCenterX >= trapLeft &&
        actorCenterX <= trapRight;

    bool actorBelow = (a.y > y);

    if (!(horizontallyUnder && actorBelow)) return;

    // New: check for occluding solid tiles between trap and actor vertically.
    // Compute tile columns that the trap covers (use trap center column)
    int trapCenterTileX = int((x + width * 0.5f) / Map::TILE_SIZE);

    // Compute tile row for trap and actor
    int trapTileY = int(y / Map::TILE_SIZE);
    int actorTileY = int(a.y / Map::TILE_SIZE);

    // If actorTileY is above trapTileY, actor is above; we only trigger when actor is below the trap
    // Iterate from trapTileY+1 to actorTileY (exclusive of actor tile) and if any solid tile exists in that column,
    // then there's a platform between trap and actor, so do not trigger.

    int startRow = trapTileY + 1;
    int endRow = actorTileY; // actor's tile row where their top is
    if (endRow <= startRow) {
        // actor is directly adjacent or same tile vertically; allow trigger
        triggered = true;
        delayTimer = delayTicks;
        triggerer = &actor;
        return;
    }

    bool occluded = false;
    for (int ty = startRow; ty < endRow; ++ty) {
        if (map.isSolid(trapCenterTileX, ty)) {
            occluded = true;
            break;
        }
    }

    if (!occluded) {
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
            if (gSound) gSound->playSfx("fallTrapRelease", 128, true);
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
        if (gSound) gSound->playSfx("fallTrapLand", 128, true);
    }
}

void FallingTrap::remove()
{
    if(finishedFalling == true)
        killTimer++;
    if (killTimer >= 50)
        alive = false;
}