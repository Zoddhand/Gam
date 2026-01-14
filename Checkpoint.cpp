#include "Checkpoint.h"
#include "AnimationManager.h"
#include "GameObject.h"
#include "Engine.h"
#include "Map.h"
#include <SDL3_Image/SDL_image.h>
#include <SDL3/SDL.h>
#include <iostream>

// Helper to create texture using engine renderer
static SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path) {
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

Checkpoint::Checkpoint(int tileX, int tileY, int tileIndex)
    : MapObject(tileX, tileY, tileIndex)
{
    // Sprite is taller and wider than a tile: use 32x64
    w = 32;
    h = 64;

    // Move object up so bottom of sprite aligns with tile bottom
    offsetPosition(0, -(h - Map::TILE_SIZE));

    // Use engine renderer if available
    SDL_Renderer* rend = nullptr;
    if (gEngine) rend = gEngine->renderer;
    if (rend) {
        animTexture = loadTexture(rend, "Assets/Sprites/checkpoint.png");
        if (animTexture) {
            SDL_SetTextureScaleMode(animTexture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(animTexture, SDL_BLENDMODE_BLEND);
            // 3 frames horizontally, each 32x64
            anim = new AnimationManager(animTexture, 32, 64, 3, 0, 0, 0, 32, 64);
            // default to showing first frame only (index 0)
            anim->currentFrame = 0;
            anim->timer = 0;
            anim->setSpeed(100000); // keep internal updater from advancing frames
        }
    } else {
        SDL_Log("Checkpoint: no engine renderer to load texture");
    }
}

Checkpoint::~Checkpoint()
{
    if (anim) { delete anim; anim = nullptr; }
    if (animTexture) { SDL_DestroyTexture(animTexture); animTexture = nullptr; }
}

void Checkpoint::markActivated()
{
}

void Checkpoint::update(GameObject& obj, Map& map)
{
    if (!active) return;
    // only respond to the player
    if (!gEngine || !gEngine->player) return;
    if (&obj != gEngine->player) return;
    if (!gEngine->player->obj.alive) return;

    SDL_FRect pr = gEngine->player->getRect();
    SDL_FRect cr = getRect();
    if (SDL_HasRectIntersectionFloat(&pr, &cr)) {
        if (!activated) {
            // activate
            activated = true;
            // persist checkpoint: store level and tile world position
            if (gEngine) {
                // save last checkpoint level and pos
                gEngine->lastCheckpointLevel = (levelID >= 0) ? levelID : gEngine->lastCheckpointLevel;
                gEngine->lastStartPosX = int(x);
                gEngine->lastStartPosY = int(y + (h - Map::TILE_SIZE)); // store ground-aligned position
                // NOTE: do NOT set respawnFromCheckpoint here — only set that when the player actually dies and selects Restart
            }

            // start looping frames 2 and 3 by initializing our timer and starting on frame 1
            if (anim) {
                anim->currentFrame = 1; // second frame (index 1)
                animTimer = 0;
                // keep AnimationManager from auto-advancing; we'll toggle frames manually
                anim->setSpeed(100000);
            }

            if (gSound) gSound->playSfx("lock_open");
            SDL_Log("Checkpoint activated at level %d (%d,%d)", gEngine ? gEngine->lastCheckpointLevel : -1, tx, ty);
        }
    }

    // Custom two-frame loop (frames indices 1 and 2) when activated
    if (activated && anim) {
        ++animTimer;
        if (animTimer >= animInterval) {
            animTimer = 0;
            if (anim->currentFrame == 1) anim->currentFrame = 2;
            else anim->currentFrame = 1;
        }
    }
}

void Checkpoint::draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const
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

    if (map.tilesetTexture) {
        int txIdx = tileIndex % map.tileCols;
        int tyIdx = tileIndex / map.tileCols;
        SDL_FRect src{ float(txIdx * Map::TILE_SIZE), float(tyIdx * Map::TILE_SIZE), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
        SDL_FRect dest{ float(x - camX), float(y - camY), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
        SDL_RenderTexture(renderer, map.tilesetTexture, &src, &dest);
    }
}
