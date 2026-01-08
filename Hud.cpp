#include "Hud.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>

Hud::Hud(SDL_Renderer* renderer, const std::string& spritePath)
{
    SDL_Surface* surf = IMG_Load(spritePath.c_str());
    if (surf) {
        tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_DestroySurface(surf);
    }
}

Hud::~Hud()
{
    if (tex) SDL_DestroyTexture(tex);
}

void Hud::draw(SDL_Renderer* renderer, float health, float maxHealth)
{
    if (!tex) return;

    // Guard maxHealth
    if (maxHealth <= 0.0f) maxHealth = 1.0f;

    // Map health fraction [0..1] to frame index where 0 = full, frameCount-1 = empty
    float frac = std::clamp(health / maxHealth, 0.0f, 1.0f);

    // We want full health -> frame 0. empty -> frame (frameCount-1)
    int frame = int((1.0f - frac) * (frameCount - 1) + 0.5f);
    frame = std::clamp(frame, 0, frameCount - 1);

    SDL_FRect src{ float(frame * frameW), 0.0f, float(frameW), float(frameH) };
    // Scale up 2x
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_FRect dst{ 4.0f, 4.0f, float(frameW) * 1.5f, float(frameH) * 1.5f };

    SDL_RenderTexture(renderer, tex, &src, &dst);
}
