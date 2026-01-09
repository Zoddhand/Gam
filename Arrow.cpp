#include "Arrow.h"
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include "Sound.h"

// Shared texture for all arrows
static SDL_Texture* s_arrowTex = nullptr;

Arrow::Arrow(SDL_Renderer* renderer, float x_, float y_, float vx, float vy)
    : x(x_), y(y_), velx(vx), vely(vy)
{
	y = y_ - h * 0.5f; // center vertically
    // Load shared texture once using provided renderer
    if (renderer && !s_arrowTex) {
        SDL_Surface* surf = IMG_Load("Assets/Sprites/Arrow.png");
        if (surf) {
            s_arrowTex = SDL_CreateTextureFromSurface(renderer, surf);
            if (s_arrowTex) SDL_SetTextureScaleMode(s_arrowTex, SDL_SCALEMODE_NEAREST);
            SDL_DestroySurface(surf);
        } else {
            SDL_Log("Arrow: failed to load sprite 'Assets/Sprites/Arrow.png': %s", SDL_GetError());
        }
    }

    // Instance uses shared texture (may be null -> fallback drawing)
    tex = s_arrowTex;
}

Arrow::~Arrow() {
    // Do not destroy shared texture here; engine will clean up renderer/tex lifecycle elsewhere
}

void Arrow::update(Map& map)
{
    gSound->playSfx("arrow");
    // simple physics
    y += vely;
    x += velx;

    // off-map -> destroy
    if (x < 0 || y < 0 || x > map.width * map.TILE_SIZE || y > map.height * map.TILE_SIZE) {
        alive = false;
        return;
    }

    // collision with solid tiles (basic): check center point
    int tx = int((x + w*0.5f) / map.TILE_SIZE);
    int ty = int((y + h*0.5f) / map.TILE_SIZE);
    if (map.getCollision(tx, ty) != -1) {
        alive = false;
    }
}

void Arrow::draw(SDL_Renderer* renderer, int camX, int camY)
{
    if (!alive) return;
    SDL_FRect dst{ x - camX, y - camY, (float)w, (float)h };
    if (tex) {
        // Compute rotation angle so the arrow's top faces its velocity direction.
        // Sprite faces right by default, so use atan2(vely, velx).
        float angleRad = std::atan2(vely, velx);
        float angleDeg = angleRad * 180.0f / 3.14159265f;
        SDL_RenderTextureRotated(renderer, tex, nullptr, &dst, angleDeg, nullptr, SDL_FLIP_NONE);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
        SDL_RenderFillRect(renderer, &dst);
    }
}

SDL_FRect Arrow::getRect() const { return { x, y, (float)w, (float)h }; }
