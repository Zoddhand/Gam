#include "GameOver.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>

GameOver::GameOver(SDL_Renderer* renderer, float viewScale_) : viewScale(viewScale_) {
    options.push_back("Restart");
    options.push_back("Quit");

    // load background image (reuse Menu background)
    SDL_Surface* surf = IMG_Load("Assets/Menu/gameover.png");
    if (surf) {
        bgTex = SDL_CreateTextureFromSurface(renderer, surf);
        if (bgTex) SDL_SetTextureScaleMode(bgTex, SDL_SCALEMODE_NEAREST);
        SDL_DestroySurface(surf);
    } else {
        SDL_Log("GameOver: failed to load background: %s", SDL_GetError());
        bgTex = nullptr;
    }

    if (!TTF_Init()) {
        SDL_Log("GameOver: TTF_Init failed: %s", SDL_GetError());
        font = nullptr;
    } else {
        font = TTF_OpenFont("Assets/Fonts/font.ttf", (float)fontSize);
        if (!font) {
            SDL_Log("GameOver: Failed to load font: %s", SDL_GetError());
        }
    }
}

GameOver::~GameOver() {
    if (font) { TTF_CloseFont(font); font = nullptr; }
    if (bgTex) { SDL_DestroyTexture(bgTex); bgTex = nullptr; }
    TTF_Quit();
}

void GameOver::handleInput(const bool* keys) {
    bool up = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    bool down = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    bool enter = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];

    if (up && !upLastKbd) selected = (selected - 1 + (int)options.size()) % (int)options.size();
    if (down && !downLastKbd) selected = (selected + 1) % (int)options.size();
    if (enter && !enterLastKbd) activated = true;

    upLastKbd = up; downLastKbd = down; enterLastKbd = enter;
}

void GameOver::handleInput(const Controller::State& cs) {
    bool up = cs.up;
    bool down = cs.down;
    bool enter = cs.attack || cs.jump;

    if (up && !upLastCtrl) selected = (selected - 1 + (int)options.size()) % (int)options.size();
    if (down && !downLastCtrl) selected = (selected + 1) % (int)options.size();
    if (enter && !enterLastCtrl) activated = true;

    upLastCtrl = up; downLastCtrl = down; enterLastCtrl = enter;
}

void GameOver::update() { }

static SDL_Texture* createTextTexture(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, SDL_Color color, float& outW, float& outH) {
    if (!font) return nullptr;
    SDL_Surface* surf = TTF_RenderText_Solid(font, text.c_str(), 0, color);
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    float w = 0, h = 0;
    SDL_GetTextureSize(tex, &w, &h);
    outW = w; outH = h;
    SDL_free(surf);
    return tex;
}

void GameOver::render(SDL_Renderer* renderer) {
    // Draw background or fallback color
    if (bgTex) {
        float w = 0, h = 0;
        SDL_GetTextureSize(bgTex, &w, &h);
        float scale = 1.0f / viewScale; // compensate for engine scaling
        SDL_FRect dest{ 0.0f, 0.0f, w * scale, h * scale };
        SDL_RenderTexture(renderer, bgTex, nullptr, &dest);
    } else {
        SDL_SetRenderDrawColor(renderer, 10, 10, 30, 255);
        SDL_RenderClear(renderer);
    }

    float uiScale = 1.0f / viewScale;
    float baseX = 80.0f * uiScale;
    float baseY = 50.0f * uiScale;
    float itemW = 160.0f * uiScale;
    float itemH = 28.0f * uiScale;
    float itemGap = 36.0f * uiScale;

    // Title
    if (font) {
        SDL_Color col = { 255, 255, 255, 255 };
        float tw, th;
        SDL_Texture* titleTex = createTextTexture(renderer, font, "Game Over", col, tw, th);
        if (titleTex) {
            SDL_FRect dst{ 45 , 10.0f, tw, th };
            SDL_RenderTexture(renderer, titleTex, nullptr, &dst);
            SDL_DestroyTexture(titleTex);
        }
    }

    for (size_t i = 0; i < options.size(); ++i) {
        SDL_FRect r{ baseX, baseY + float(i) * itemGap, itemW, itemH };
        if ((int)i == selected)
            SDL_SetRenderDrawColor(renderer, 200, 200, 80, 200);
        else
            SDL_SetRenderDrawColor(renderer, 160, 160, 160, 200);
        SDL_RenderFillRect(renderer, &r);

        if (font) {
            SDL_Color textColor = { 0, 0, 0, 255 };
            float tw = 0, th = 0;
            SDL_Texture* tex = createTextTexture(renderer, font, options[i], textColor, tw, th);
            if (tex) {
                SDL_FRect dst{ r.x + (r.w - tw) * 0.5f, r.y + (r.h - th) * 0.5f, tw, th };
                SDL_RenderTexture(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
        }
    }
}

int GameOver::consumeSelection() {
    if (!activated) return -1;
    activated = false;
    return selected; // 0 = Restart, 1 = Quit
}
