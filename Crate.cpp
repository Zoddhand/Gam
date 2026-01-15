#include "Crate.h"
#include <SDL3/SDL.h>
#include "GameObject.h"
#include "Sound.h"
#include "Engine.h"
#include "PressurePlate.h"
#include <cmath>
#include <algorithm>

Crate::Crate(int tileX, int tileY, int tileIndex)
    : MapObject(tileX, tileY, tileIndex)
{
    // crates are tile-sized by default, but allow slightly different physics
    w = Map::TILE_SIZE;
    h = Map::TILE_SIZE;
}

void Crate::draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const
{
    if (!active) return;
    // Draw simple brown rectangle if no tileset available
    if (map.tilesetTexture) {
        // attempt to draw using tile index if valid
        int txIdx = tileIndex % map.tileCols;
        int tyIdx = tileIndex / map.tileCols;
        SDL_FRect src{ float(txIdx * Map::TILE_SIZE), float(tyIdx * Map::TILE_SIZE), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
        SDL_FRect dest{ float(x - camX), float(y - camY), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
        SDL_RenderTexture(renderer, map.tilesetTexture, &src, &dest);
        return;
    }

    SDL_FRect dst{ x - camX, y - camY, float(w), float(h) };
    SDL_SetRenderDrawColor(renderer, 120, 70, 20, 255);
    SDL_RenderFillRect(renderer, &dst);
}

Crate::~Crate() = default;

void Crate::update(GameObject& player, Map& map)
{
    if (!active) return;
    // Physics constants
    const float GRAV = 0.35f;
    const float MAX_FALL = 8.0f;
    const float FRICTION = 0.85f;

    // Apply gravity
    vely += GRAV;
    if (vely > MAX_FALL) vely = MAX_FALL;

    // Horizontal friction
    velx *= FRICTION;
    if (fabs(velx) < 0.01f) velx = 0.0f;

    // Move horizontally and resolve collisions with map
    x += velx;
    int leftTile = int(x / Map::TILE_SIZE);
    int rightTile = int((x + w - 1) / Map::TILE_SIZE);
    int topTile = int(y / Map::TILE_SIZE);
    int bottomTile = int((y + h - 1) / Map::TILE_SIZE);

    if (velx > 0) {
        int colTop = map.getCollision(rightTile, topTile);
        int colBottom = map.getCollision(rightTile, bottomTile);
        if ((colTop != -1 && colTop != Map::COLL_ONEWAY) || (colBottom != -1 && colBottom != Map::COLL_ONEWAY)) {
            x = rightTile * Map::TILE_SIZE - w;
            velx = 0.0f;
        }
    } else if (velx < 0) {
        int colTop = map.getCollision(leftTile, topTile);
        int colBottom = map.getCollision(leftTile, bottomTile);
        if ((colTop != -1 && colTop != Map::COLL_ONEWAY) || (colBottom != -1 && colBottom != Map::COLL_ONEWAY)) {
            x = (leftTile + 1) * Map::TILE_SIZE;
            velx = 0.0f;
        }
    }

    // Move vertically and resolve collisions
    float prevY = y;
    y += vely;
    topTile = int(y / Map::TILE_SIZE);
    bottomTile = int((y + h - 1) / Map::TILE_SIZE);

    if (vely > 0) {
        int colLeft = map.getCollision(leftTile, bottomTile);
        int colRight = map.getCollision(rightTile, bottomTile);
        auto shouldStopOn = [&](int col) -> bool {
            if (col == -1) return false;
            if (col == Map::COLL_ONEWAY) {
                float platformTop = float(bottomTile * map.TILE_SIZE);
                float prevBottom = prevY + h - 1;
                if (prevBottom <= platformTop) return true;
                return false;
            }
            return true;
        };
        if (shouldStopOn(colLeft) || shouldStopOn(colRight)) {
            y = bottomTile * Map::TILE_SIZE - h;
            vely = 0.0f;
            onGround = true;
        } else {
            // not landing on full-height tile; check for thin pressure plates underneath
            onGround = false;
            if (gEngine) {
                bool landedOnPlate = false;
                float currBottom = y + h;
                for (auto* mo : gEngine->objects) {
                    if (!mo || !mo->active) continue;
                    PressurePlate* pp = dynamic_cast<PressurePlate*>(mo);
                    if (!pp) continue;
                    SDL_FRect pr = pp->getRect();
                    float plateTop = pr.y + (pr.h - 2.0f);
                    // horizontal overlap check
                    float overlapX = std::min(x + w, pr.x + pr.w) - std::max(x, pr.x);
                    if (overlapX <= 0) continue;
                    float prevBottom = prevY + h - 1;
                    // require downward crossing
                    if (prevBottom < plateTop && currBottom >= plateTop && vely >= 0.0f) {
                        // snap crate to plate
                        y = plateTop - h;
                        vely = 0.0f;
                        onGround = true;
                        landedOnPlate = true;
                        break;
                    }
                }
                (void)landedOnPlate; // silence unused in some builds
            }
        }
    } else if (vely < 0) {
        int colLeftTop = map.getCollision(leftTile, topTile);
        int colRightTop = map.getCollision(rightTile, topTile);
        auto isSolidCeil = [&](int col) -> bool {
            if (col == -1) return false;
            if (col == Map::COLL_ONEWAY) return false;
            return true;
        };
        if (isSolidCeil(colLeftTop) || isSolidCeil(colRightTop)) {
            y = (topTile + 1) * Map::TILE_SIZE;
            vely = 0.0f;
        }
    }

    // Interaction with player attack: if player attack rect intersects crate, apply push
    if (hitInvuln > 0) --hitInvuln;
    SDL_FRect pr = player.getRect();
    SDL_FRect atk = player.getAttackRect();
    SDL_FRect cr = getRect();
    if (player.obj.attacking && SDL_HasRectIntersectionFloat(&atk, &cr) && hitInvuln == 0) {
        // push crate away from player
        float centerPlayer = atk.x + atk.w * 0.5f;
        float centerCrate = cr.x + cr.w * 0.5f;
        float dir = (centerCrate < centerPlayer) ? -1.0f : 1.0f;
        // stronger push when attack hits; direction away from player
        velx = dir * 4.0f; // push speed
        // small upward nudge if on ground
        if (onGround) vely = -3.0f;
        hitInvuln = 8; // short invuln so holding attack doesn't keep reapplying
        if (gSound) gSound->playSfx("clang", 128, false);
    }
    }

