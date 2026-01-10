#include "Background.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstdio>
#include <iostream>
#include <cmath>

Background::Background()
{
}

Background::~Background()
{
    unload();
}

// Try to load via IMG_Load first, fall back to SDL_LoadBMP
static SDL_Surface* loadSurface(const char* path)
{
    SDL_Surface* surf = IMG_Load(path);
    if (surf) return surf;

    // fallback to BMP
    surf = SDL_LoadBMP(path);
    if (surf) return surf;

    SDL_Log("Background: failed to load %s: IMG/SDL error: %s", path, SDL_GetError());
    return nullptr;
}

bool Background::load(SDL_Renderer* renderer, const std::string& dir, int layers)
{
    unload();
    m_dir = dir;
    m_layers = layers;
    m_textures.resize(layers, nullptr);
    m_factors.resize(layers, 0.0f);
    m_hFactors.resize(layers, 0.0f);
    m_scrollOffsets.resize(layers, 0.0f);
    m_scrollSpeeds.resize(layers, 0.0f);
    m_autoScroll.resize(layers, 0);

    // default factors: farther layers move less (0.0 = fixed, 1.0 = follows camera)
    for (int i = 0; i < layers; ++i) {
        float t = float(i) / float(std::max(1, layers - 1));
        m_factors[i] = 0.05f + t * (0.95f);
        // horizontal factors similar but allow specific overrides below
        m_hFactors[i] = 0.05f + t * (0.95f);
        m_scrollOffsets[i] = 0.0f;
        m_scrollSpeeds[i] = 0.0f;
        m_autoScroll[i] = 0;
    }

    // Customize autonomous horizontal scroll for clouds (layers 2 and 3 -> indices 1 and 2)
    if (m_layers > 1) {
        m_autoScroll[1] = 1;
        m_scrollSpeeds[1] = 8.0f;   // move right at 8 px/sec
        m_hFactors[1] = 0.25f;      // still apply modest parallax to camera
    }
    if (m_layers > 2) {
        m_autoScroll[2] = 1;
        m_scrollSpeeds[2] = -12.0f; // move left at 12 px/sec
        m_hFactors[2] = 0.6f;
    }

    char path[1024];
    for (int i = 0; i < layers; ++i) {
        snprintf(path, sizeof(path), "%s/%d.png", dir.c_str(), i + 1);
        SDL_Surface* surf = loadSurface(path);
        if (!surf) {
            SDL_Log("Background: skipping missing layer %s", path);
            continue;
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_DestroySurface(surf);
        if (!tex) {
            SDL_Log("Background: texture creation failed for %s: %s", path, SDL_GetError());
            continue;
        }

        // nearest sampling and alpha blending
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        m_textures[i] = tex;
    }

    return true;
}

void Background::unload()
{
    for (auto* t : m_textures) if (t) SDL_DestroyTexture(t);
    m_textures.clear();
    m_factors.clear();
    m_hFactors.clear();
    m_scrollOffsets.clear();
    m_scrollSpeeds.clear();
    m_autoScroll.clear();
    m_levels.clear();
    m_layers = 0;
}

void Background::update(float dt)
{
    if (m_textures.empty()) return;
    for (int i = 0; i < m_layers; ++i) {
        if (i < (int)m_autoScroll.size() && m_autoScroll[i]) {
            m_scrollOffsets[i] += m_scrollSpeeds[i] * dt;
            // keep offset in range to avoid large floats (optional)
            // will wrap when drawing using fmod
        }
    }
}

void Background::draw(SDL_Renderer* renderer, int camX, int camY, int screenW, int screenH)
{
    if (m_textures.empty()) return;
    for (int i = 0; i < m_layers; ++i) {
        SDL_Texture* tex = m_textures[i];
        if (!tex) continue;

        float fw = 0.0f, fh = 0.0f;
        SDL_GetTextureSize(tex, &fw, &fh);
        int tw = (int)fw;
        int th = (int)fh;
        if (tw <= 0 || th <= 0) continue;

        // use horizontal-specific factor for X offset and vertical factor for Y
        float hFactor = (i < (int)m_hFactors.size()) ? m_hFactors[i] : m_factors[i];
        float vFactor = (i < (int)m_factors.size()) ? m_factors[i] : 0.0f;

        float ox = camX * hFactor;
        float oy = camY * vFactor;

        // add autonomous scroll offset
        if (i < (int)m_scrollOffsets.size()) ox += m_scrollOffsets[i];

        float mod = fmod(ox, (float)tw);
        int startX = int(-mod);
        if (startX > 0) startX -= tw;

        SDL_FRect dst;
        dst.y = int(-oy);
        dst.h = (float)th;
        for (int x = startX; x < screenW; x += tw) {
            dst.x = (float)x;
            dst.w = (float)tw;
            SDL_RenderTexture(renderer, tex, nullptr, &dst);
        }
    }
}

void Background::addLevel(int levelID)
{
    m_levels.push_back(levelID);
}

bool Background::matchesLevel(int levelID) const
{
    for (int id : m_levels) if (id == levelID) return true;
    return false;
}
