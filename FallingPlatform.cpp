#include "FallingPlatform.h"
#include "GameObject.h"
#include "Map.h"
#include <SDL3/SDL.h>
#include "Sound.h"

FallingPlatform::FallingPlatform(int leftTileX, int tileY, const std::vector<int>& tileInds)
    : MapObject(leftTileX, tileY, tileInds.empty() ? -1 : tileInds[0]), initTx(leftTileX), initTy(tileY), initTileIndex(tileInds.empty() ? -1 : tileInds[0]), tileIndices(tileInds), tileCount((int)tileInds.size())
{
    // set width to cover multiple tiles
    w = Map::TILE_SIZE * tileCount;
}

FallingPlatform::~FallingPlatform() = default;

void FallingPlatform::ensureCollisionLayer(Map& map) {
    if (map.collision.empty()) {
        map.collision.assign(map.width * map.height, -1);
    }
}

void FallingPlatform::setCollision(Map& map, int val) {
    for (int i = 0; i < tileCount; ++i) {
        int txi = initTx + i;
        if (txi >= 0 && initTy >= 0 && txi < map.width && initTy < map.height) {
            ensureCollisionLayer(map);
            map.collision[initTy * map.width + txi] = val;
        }
    }
}

void FallingPlatform::draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const {
    if (!active) return;

    if (map.tilesetTexture) {
        for (int i = 0; i < tileCount; ++i) {
            int tIndex = tileIndices[i];
            if (tIndex < 0) continue;
            int txIdx = tIndex % map.tileCols;
            int tyIdx = tIndex / map.tileCols;
            SDL_FRect src{ float(txIdx * Map::TILE_SIZE), float(tyIdx * Map::TILE_SIZE), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
            SDL_FRect dest{ float(x + i * Map::TILE_SIZE - camX), float(y - camY), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
            SDL_RenderTexture(renderer, map.tilesetTexture, &src, &dest);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
        for (int i = 0; i < tileCount; ++i) {
            SDL_FRect dest{ float(x + i * Map::TILE_SIZE - camX), float(y - camY), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
            SDL_RenderFillRect(renderer, &dest);
        }
    }
}

void FallingPlatform::update(GameObject& playerObj, Map& map) {
    if (!active) return;

    // Ensure collision tile is present while platform is static
    if (!falling) {
        setCollision(map, 1);
    }

    // If already falling, apply gravity and move down. No collision with tiles required.
    if (falling) {
        // Clear collision so other objects don't treat it as solid while falling
        setCollision(map, -1);

        velY += gravity;
        y += velY;

        // start reset timer once platform begins falling
        if (resetTimer == 0) resetTimer = resetTicks;

        // countdown reset
        if (resetTimer > 0) {
            if (--resetTimer <= 0) {
                // restore spawn tiles and reset platform back to initial position
                if (initTx >= 0 && initTy >= 0 && initTx < map.width && initTy < map.height && !map.spawn.empty()) {
                    for (int i = 0; i < tileCount; ++i) {
                        map.spawn[initTy * map.width + (initTx + i)] = tileIndices[i];
                    }
                }
                // restore collision and reset object state
                setCollision(map, 1);
                x = float(initTx * Map::TILE_SIZE);
                y = float(initTy * Map::TILE_SIZE);
                triggered = false;
                falling = false;
                velY = 0.0f;
                delayTimer = 0;
                // keep active true so platform is usable again
                active = true;
            }
        }
        return;
    }

    // While static, support standing: if player overlaps horizontally and feet are near platform top,
    // snap player to platform so collision feels solid even though the tile is in spawn layer.
    {
        SDL_FRect pr = playerObj.getRect();
        float platLeft = x;
        float platRight = x + w;
        float feetY = pr.y + pr.h;

        bool horizOverlap = (pr.x + pr.w > platLeft + 1.0f) && (pr.x < platRight - 1.0f);
        bool feetNearTop = (feetY >= y - 4.0f) && (feetY <= y + 4.0f);
        if (horizOverlap && feetNearTop && playerObj.obj.vely >= 0.0f) {
            // Snap player onto platform
            playerObj.obj.y = y - playerObj.obj.tileHeight;
            playerObj.obj.vely = 0.0f;
            playerObj.obj.onGround = true;
        }
    }

    // Check player overlap of centre x and standing on top to trigger fall
    // Trigger only when player's feet rectangle intersects the platform top area
    SDL_FRect pr = playerObj.getRect();
    float actorCenterX = pr.x + pr.w * 0.5f;
    SDL_FRect feetRect{ pr.x, pr.y + pr.h - 2.0f, pr.w, 2.0f };
    SDL_FRect platformTopRect{ x, y - 2.0f, w, 4.0f };

    bool horizontallyUnder = actorCenterX >= x && actorCenterX <= x + w;
    // actor's feet y coordinate
    float actorFeetY = pr.y + pr.h;

    bool feetTouching = SDL_HasRectIntersectionFloat(&feetRect, &platformTopRect);
    bool centerOver = (pr.x + pr.w * 0.5f) >= x && (pr.x + pr.w * 0.5f) <= x + w;

    if (feetTouching && centerOver && playerObj.obj.vely >= 0.0f) {
        if (!triggered) {
			gSound->playSfx("fall_plat");
            triggered = true;
            delayTimer = delayTicks;
        }
    }

    if (triggered && !falling) {
        if (--delayTimer <= 0) {
            falling = true;
            velY = 1.0f;
            // remove spawn tiles so Map::getCollision no longer treats it as solid
            if (initTx >= 0 && initTy >= 0 && initTx < map.width && initTy < map.height && !map.spawn.empty()) {
                for (int i = 0; i < tileCount; ++i) {
                    map.spawn[initTy * map.width + (initTx + i)] = -1;
                }
            }
            // clear collision entries as it begins to fall
            setCollision(map, -1);
            // initialize reset timer immediately so countdown starts this frame
            if (resetTimer == 0) resetTimer = resetTicks;
        }
    }
}
