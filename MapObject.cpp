#include "MapObject.h"
#include "GameObject.h"
#include "AnimationManager.h"
#include <SDL3_image/SDL_image.h>
#include <cmath>

MapObject::MapObject(int tileX, int tileY, int tileIndex)
    : tx(tileX), ty(tileY), tileIndex(tileIndex)
{
    x = float(tx * Map::TILE_SIZE);
    y = float(ty * Map::TILE_SIZE);
    w = Map::TILE_SIZE;
    h = Map::TILE_SIZE;
    animTexture = nullptr;
    anim = nullptr;
}

MapObject::~MapObject()
{
    if (anim) delete anim;
    if (animTexture) SDL_DestroyTexture(animTexture);
}

bool MapObject::loadAnimation(SDL_Renderer* renderer, const std::string& path,
                       int frameW, int frameH, int frames, int rowY,
                       int innerX, int innerY, int innerW, int innerH,
                       int speed)
{
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) return false;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    if (!tex) return false;

    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    animTexture = tex;
    anim = new AnimationManager(tex, frameW, frameH, frames, rowY, innerX, innerY, innerW ? innerW : frameW, innerH ? innerH : frameH);
    anim->setSpeed(speed);
    animFrameW = frameW;
    animFrameH = frameH;
    return true;
}

void MapObject::offsetPosition(int dx, int dy)
{
    x += float(dx);
    y += float(dy);
}

void MapObject::update(GameObject& /*player*/, Map& /*map*/)
{
    if (anim) anim->update();
}

void MapObject::draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const
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
    else {
        SDL_FRect dest{ float(x - camX), float(y - camY), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
        SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
        SDL_RenderFillRect(renderer, &dest);
    }
}

SDL_FRect MapObject::getRect()
{
    return { x, y, float(w), float(h) };
}