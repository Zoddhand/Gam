#include "Hud.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>

Hud::Hud(SDL_Renderer* renderer, const std::string& spritePath)
{
    SDL_Surface* surf = IMG_Load(spritePath.c_str());
    if (surf) {
        // Create original texture
        tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_DestroySurface(surf);

        if (tex) {
            SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
    } else {
        SDL_Log("Hud: failed to load sprite '%s'", spritePath.c_str());
    }

    // Try to load a separate magic sprite (same directory assumed)
    SDL_Surface* msurf = IMG_Load("Assets/Sprites/magic.png");
    if (msurf) {
        texMagic = SDL_CreateTextureFromSurface(renderer, msurf);
        SDL_DestroySurface(msurf);
        if (texMagic) {
            SDL_SetTextureScaleMode(texMagic, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(texMagic, SDL_BLENDMODE_BLEND);
        }
    } else {
        // not fatal; we'll fall back to drawing the health sprite for magic
        texMagic = nullptr;
    }
}

Hud::~Hud()
{
    if (tex) SDL_DestroyTexture(tex);
    if (texMagic) SDL_DestroyTexture(texMagic);
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

    // Ensure texture color modulation is default
    SDL_SetTextureColorMod(tex, 0xFF, 0xFF, 0xFF);
    SDL_RenderTexture(renderer, tex, &src, &dst);
}

void Hud::draw(SDL_Renderer* renderer, float health, float maxHealth, float magic, float maxMagic)
{
    if (!tex) return;

    // Draw health using existing logic
    draw(renderer, health, maxHealth);

    // Guard maxMagic
    if (maxMagic <= 0.0f) maxMagic = 1.0f;

    // Map magic fraction [0..1] to frame index where 0 = full, frameCount-1 = empty
    float mfrac = std::clamp(magic / maxMagic, 0.0f, 1.0f);
    int mframe = int((1.0f - mfrac) * (frameCount - 1) + 0.5f);
    mframe = std::clamp(mframe, 0, frameCount - 1);

    SDL_FRect msrc{ float(mframe * frameW), 0.0f, float(frameW), float(frameH) };

    float scale = 1.5f;
    float hw = float(frameW) * scale;
    float hh = float(frameH) * scale;
    float spacing = 4.0f;
    SDL_FRect mdst{ 4.0f + hw + spacing, 4.0f, hw, hh };

    // If a separate magic texture was loaded, use it; otherwise reuse health texture
    SDL_Texture* use = texMagic ? texMagic : tex;
    SDL_SetTextureColorMod(use, 0xFF, 0xFF, 0xFF);
    SDL_RenderTexture(renderer, use, &msrc, &mdst);
}
