#include "WorldObject.h"
#include <SDL3_image/SDL_image.h>

WorldObject::WorldObject(SDL_Renderer* renderer,
    const std::string& spritePath,
    int w,
    int h)
    : width(w), height(h)
{
    SDL_Surface* surf = IMG_Load(spritePath.c_str());
    if (!surf) {
        SDL_Log("Failed to load world object sprite: %s", SDL_GetError());
        return;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(surf);
}

WorldObject::~WorldObject() {
    if (texture)
        SDL_DestroyTexture(texture);
}

void WorldObject::draw(SDL_Renderer* renderer, int camX, int camY) {
    if (!alive || !texture) return;

    SDL_FRect dst{
        x - camX,
        y - camY,
        (float)width,
        (float)height
    };

    SDL_RenderTexture(renderer, texture, nullptr, &dst);
}

SDL_FRect WorldObject::getRect() const {
    return { x, y, (float)width, (float)height };
}
