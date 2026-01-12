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

    // initialize public multiplier vectors with defaults
    layerHMultiplier.assign(layers, 1.0f);
    layerVMultiplier.assign(layers, 1.0f);
    layerAutoScrollMultiplier.assign(layers, 1.0f);

    // default factors:
    // - vertical factors: follow camera exactly so bottoms align with the map
    // - horizontal factors: parallax (closer layers move more)
    for (int i = 0; i < layers; ++i) {
        m_factors[i] = 1.0f; // vertical follow
        float t = float(i) / float(std::max(1, layers - 1));
        // horizontal: closer layers (small i) should move more -> invert t
        float ht = 1.0f - t;
        m_hFactors[i] = 0.05f + ht * 0.95f;
        m_scrollOffsets[i] = 0.0f;
        m_scrollSpeeds[i] = 0.0f;
        m_autoScroll[i] = 0;
    }

    // Autonomous horizontal scroll for clouds (layers 5 and 6 -> indices 4 and 5)
    for (int i = 0; i < layers && i < (int)DEFAULT_AUTO_SCROLL_SPEEDS.size(); ++i) {
        float def = DEFAULT_AUTO_SCROLL_SPEEDS[i];
        if (def != 0.0f) {
            m_autoScroll[i] = 1;
            m_scrollSpeeds[i] = def;
        }
    }

    // Apply default multipliers arrays if they exist for this many layers
    for (int i = 0; i < layers; ++i) {
        if (i < (int)DEFAULT_LAYER_H_MULT.size()) layerHMultiplier[i] = DEFAULT_LAYER_H_MULT[i];
        if (i < (int)DEFAULT_LAYER_V_MULT.size()) layerVMultiplier[i] = DEFAULT_LAYER_V_MULT[i];
        // layerAutoScrollMultiplier left as 1.0 by default
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
    layerHMultiplier.clear();
    layerVMultiplier.clear();
    layerAutoScrollMultiplier.clear();
}

void Background::update(float dt)
{
    if (m_textures.empty()) return;
    for (int i = 0; i < m_layers; ++i) {
        if (i < (int)m_autoScroll.size() && m_autoScroll[i]) {
            float mult = (i < (int)layerAutoScrollMultiplier.size()) ? layerAutoScrollMultiplier[i] : 1.0f;
            m_scrollOffsets[i] += m_scrollSpeeds[i] * mult * dt;
            // keep offset in range to avoid large floats (optional)
            // will wrap when drawing using fmod
        }
    }
}

void Background::draw(SDL_Renderer* renderer, int camX, int camY, int screenW, int screenH, int mapPixelHeight)
{
    if (m_textures.empty()) return;
    // draw furthest layers first so closer layers render on top
    for (int ii = m_layers - 1; ii >= 0; --ii) {
        int i = ii;
        SDL_Texture* tex = m_textures[i];
        if (!tex) continue;

        float fw = 0.0f, fh = 0.0f;
        SDL_GetTextureSize(tex, &fw, &fh);
        int tw = (int)fw;
        int th = (int)fh;
        if (tw <= 0 || th <= 0) continue;

        // horizontal parallax factor (per-layer)
        float baseH = (i < (int)m_hFactors.size()) ? m_hFactors[i] : 1.0f;
        float hMult = (i < (int)layerHMultiplier.size()) ? layerHMultiplier[i] : 1.0f;
        float hFactor = baseH * hMult;

        // vertical follow factor (per-layer) - should be 1.0 for bottom alignment
        float baseV = (i < (int)m_factors.size()) ? m_factors[i] : 1.0f;
        float vMult = (i < (int)layerVMultiplier.size()) ? layerVMultiplier[i] : 1.0f;
        float vFactor = baseV * vMult;

        // compute offsets
        float ox = float(camX) * hFactor;
        float oy = float(camY) * vFactor;

        // add autonomous scroll offset (horizontal)
        if (i < (int)m_scrollOffsets.size()) ox += m_scrollOffsets[i];

        float mod = fmod(ox, (float)tw);
        int startX = int(-mod);
        if (startX > 0) startX -= tw;

        SDL_FRect dst;
        // align bottom of texture with bottom of map (mapPixelHeight) then subtract camera Y* vFactor
        float baseY = float(mapPixelHeight - th);
        dst.y = baseY - oy;
        dst.h = (float)th;
        for (int x = startX; x < screenW; x += tw) {
            dst.x = (float)x;
            dst.w = (float)tw;
            SDL_SetRenderDrawColor(renderer, 169, 228, 238, 255);
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
