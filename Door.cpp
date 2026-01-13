#include "Door.h"
#include "GameObject.h"
#include "Player.h"
#include "Map.h"
#include "Engine.h"
#include "Sound.h"
#include <SDL3/SDL.h>

Door::Door(int tileX, int tileY, int tileIndex)
    : MapObject(tileX, tileY, tileIndex)
{
    // door is 32x48 pixels per request
    w = 32;
    h = 48;
    // Align door so its bottom aligns with tile bottom (tile is 16 high)
    // Since h=48 (3 tiles tall), offset up by 2 tiles so bottoms align
    offsetPosition(0, -Map::TILE_SIZE * 2);
}

Door::~Door() = default;

void Door::open(bool playSound)
{
    if (opened) return;
    opened = true;
    if (anim) {
        // set to last frame and ensure timers won't advance it
        anim->currentFrame = std::max(0, anim->frameCount - 1);
        anim->timer = 0;
    }
    if (playSound && gSound) gSound->playSfx("door_open", 128, false);

    // record persistence with engine if available and level set
    if (gEngine && level != -1) {
        gEngine->markDoorOpened(level, tx, ty);
    }
}

void Door::update(GameObject& obj, Map& map)
{
    if (!active) return;

    // If already opened, keep final frame and do not advance animation
    if (opened) {
        if (anim) {
            // ensure frame locked to last
            anim->currentFrame = std::max(0, anim->frameCount - 1);
            anim->timer = 0;
        }
        return;
    }

    if (!anim) return;

    // detect player touching door
    SDL_FRect playerRect = obj.getRect();
    SDL_FRect doorRect = getRect();

    bool touching = SDL_HasRectIntersectionFloat(&playerRect, &doorRect);

    Player* p = dynamic_cast<Player*>(&obj);
    if (!p) {
        // do not update wasPlayerTouching for non-player callers to avoid
        // overwriting the player's edge state (other objects like orcs call this too)
        return;
    }

    bool edgeTouch = touching && !wasPlayerTouching;

    // On edge touch, play feedback and consume key if present
    if (edgeTouch) {
        if (p->hasKey) {
            // play lock open SFX and consume key
            if (gSound) gSound->playSfx("lock_open", 128, false);
            p->hasKey = false;
            unlocked = true; // allow animation to continue even if player leaves
            SDL_Log("Player touched door at level %d (%d,%d): key consumed, door unlocked", level, tx, ty);
        } else {
            // play lock fail SFX
            if (gSound) gSound->playSfx("lock_fail", 128, false);
            SDL_Log("Player touched locked door at level %d (%d,%d) without key", level, tx, ty);
        }
    }

    // Advance animation if door was unlocked (so it continues even if player walks away)
    if (unlocked) {
        anim->timer++;
        if (anim->timer >= anim->speed) {
            anim->timer = 0;
            if (anim->currentFrame < anim->frameCount - 1) {
                anim->currentFrame++;
                if (anim->currentFrame >= anim->frameCount - 1) {
                    open(true);
                }
            } else {
                // already at last frame
                open(true);
            }
        }
    }

    wasPlayerTouching = touching;
}

void Door::draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const
{
    if (!active) return;

    if (anim && animTexture) {
        SDL_FRect src = anim->getSrcRect();
        SDL_FRect dst{ float(x - camX), float(y - camY), float(w), float(h) };
        dst.x = std::round(dst.x);
        dst.y = std::round(dst.y);
        SDL_RenderTexture(renderer, animTexture, &src, &dst);
        return;
    }

    SDL_FRect dst{ float(x - camX), float(y - camY), float(w), float(h) };
    SDL_SetRenderDrawColor(renderer, 120, 80, 40, 255);
    SDL_RenderFillRect(renderer, &dst);
}
