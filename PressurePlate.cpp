#include "PressurePlate.h"
#include "GameObject.h"
#include "Map.h"
#include "Engine.h"
#include "Orc.h"
#include "Archer.h"
#include "ArrowTrap.h"
#include <SDL3_Image/SDL_image.h>
#include <SDL3/SDL.h>

PressurePlate::PressurePlate(int tileX, int tileY, int tileIndex)
    : MapObject(tileX, tileY, tileIndex)
{
    // pressure plate is small: 19x3 pixels
    w = 19;
    h = 3;
    // center horizontally on tile (tile is 16px; 19px means we offset -1.5 -> use -1)
    // position sprite at bottom of tile so plate sits at lowest point
    offsetPosition(-1, Map::TILE_SIZE - h);

    // load animation if engine renderer available
    SDL_Renderer* rend = nullptr;
    if (gEngine) rend = gEngine->renderer;
    if (rend) {
        if (!loadAnimation(rend, "Assets/Sprites/pressureplate.png", 19, 3, 2, 0, 0, 0, 19, 3, 12)) {
            SDL_Log("PressurePlate: failed to load animation");
        } else {
            // keep animation from auto-advancing; we'll switch frames based on triggered state
            if (anim) anim->setSpeed(100000);
            if (anim) anim->currentFrame = 0;
        }
    }
}

PressurePlate::~PressurePlate() = default;

void PressurePlate::update(GameObject& obj, Map& /*map*/)
{
    if (!active) return;
    // Use a small hitbox 2px high anchored at the bottom of the plate
    SDL_FRect plateHit = getRect();
    plateHit.y = plateHit.y + float(plateHit.h - 2);
    plateHit.h = 2.0f;

    // When called with the player first each frame, perform a full scan of relevant objects
    if (gEngine && gEngine->player && &obj == gEngine->player) {
        triggered = false;

        // check player
        SDL_FRect pr = gEngine->player->getRect();
        if (SDL_HasRectIntersectionFloat(&plateHit, &pr)) triggered = true;

        // check all map objects (includes crates and other MapObject-derived items)
        for (auto* mo : gEngine->objects) {
            if (!mo || !mo->active) continue;
            if (mo == this) continue;
            SDL_FRect mr = mo->getRect();
            if (SDL_HasRectIntersectionFloat(&plateHit, &mr)) { triggered = true; break; }
        }

        // check orcs
        for (auto* o : gEngine->orc) {
            if (!o || !o->obj.alive) continue;
            SDL_FRect orr = o->getRect();
            if (SDL_HasRectIntersectionFloat(&plateHit, &orr)) { triggered = true; break; }
        }

        // check archers
        for (auto* a : gEngine->archers) {
            if (!a || !a->obj.alive) continue;
            SDL_FRect ar = a->getRect();
            if (SDL_HasRectIntersectionFloat(&plateHit, &ar)) { triggered = true; break; }
        }
    }
    else {
        // Otherwise only check the supplied object (player/orc/archer)
        SDL_FRect other = obj.getRect();
        if (SDL_HasRectIntersectionFloat(&plateHit, &other)) triggered = true;
    }

    // Update animation frame based on triggered state
    if (anim) anim->currentFrame = triggered ? 1 : 0;

    // If this plate became newly triggered this frame, notify nearby arrow traps.
    if (triggered && !prevTriggered) {
        if (gEngine) {
            SDL_FRect pr = getRect();
            float pCenterX = pr.x + pr.w * 0.5f;
            float pCenterY = pr.y + pr.h * 0.5f;
            for (auto* mo : gEngine->objects) {
                if (!mo || !mo->active) continue;
                ArrowTrap* at = dynamic_cast<ArrowTrap*>(mo);
                if (!at) continue;
                SDL_FRect tr = at->getRect();
                // simple vertical band check (+/- 8 px) and horizontal in front check
                if (pCenterY < tr.y - 8 || pCenterY > tr.y + tr.h + 8) continue;
                float trapLeft = tr.x;
                float trapRight = tr.x + tr.w;
                int ti = at->getTileIndex();
                bool shootLeft = (ti == Map::SPAWN_ARROWTRAP_LEFT);
                bool shootRight = (ti == Map::SPAWN_ARROWTRAP_RIGHT);
                if (shootLeft && pCenterX < trapLeft) at->triggerExternal();
                if (shootRight && pCenterX > trapRight) at->triggerExternal();
            }
        }
    }

    prevTriggered = triggered;
}

void PressurePlate::draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const
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

    // fallback: draw simple rectangle
    SDL_FRect dst{ x - camX, y - camY, float(w), float(h) };
    SDL_SetRenderDrawColor(renderer, triggered ? 0 : 200, triggered ? 200 : 30, 0, 255);
    SDL_RenderFillRect(renderer, &dst);
}
