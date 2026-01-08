#include "Menu.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include "Sound.h"

Menu::Menu(SDL_Renderer* renderer, float viewScale_) : viewScale(viewScale_) {
    options.push_back("Play");
    options.push_back("Options");
    options.push_back("Quit");

    // options submenu
    optOptions.push_back("Music: On");
    optOptions.push_back("Back");

    // load background image
    SDL_Surface* surf = IMG_Load("Assets/Menu/Background.png");
    if (surf) {
        bgTex = SDL_CreateTextureFromSurface(renderer, surf);
        if (bgTex) SDL_SetTextureScaleMode(bgTex, SDL_SCALEMODE_NEAREST);
        SDL_DestroySurface(surf);
    } else {
        SDL_Log("Menu: failed to load background: %s", SDL_GetError());
        bgTex = nullptr;
    }

    if (!TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        font = nullptr;
        return;
    }

    font = TTF_OpenFont("Assets/Fonts/font.ttf", (float)fontSize);
    if (!font) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
    }
}

Menu::~Menu() {
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
    if (bgTex) {
        SDL_DestroyTexture(bgTex);
        bgTex = nullptr;
    }
    TTF_Quit();
}

void Menu::handleInput(const bool* keys) {
    bool up = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    bool down = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    bool enter = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];

    if (up && !upLast) {
        if (state == State::Main)
            selected = (selected - 1 + (int)options.size()) % (int)options.size();
        else
            selected = (selected - 1 + (int)optOptions.size()) % (int)optOptions.size();
    }
    if (down && !downLast) {
        if (state == State::Main)
            selected = (selected + 1) % (int)options.size();
        else
            selected = (selected + 1) % (int)optOptions.size();
    }
    if (enter && !enterLast) {
        activated = true;
    }

    upLast = up;
    downLast = down;
    enterLast = enter;
}

void Menu::update() {
    // nothing for now
}

void Menu::render(SDL_Renderer* renderer) {
    // Draw background or fallback color
    if (bgTex) {
        float w = 0, h = 0;
        SDL_GetTextureSize(bgTex, &w, &h);
        // scale background to fit window while preserving aspect (simple fit)
        float scale = 1.0f / viewScale; // compensate for engine scaling
        SDL_FRect dest{ 0.0f, 0.0f, w * scale, h * scale };
        SDL_RenderTexture(renderer, bgTex, nullptr, &dest);
    } else {
        SDL_SetRenderDrawColor(renderer, 10, 10, 30, 255);
        SDL_RenderClear(renderer);
    }

    const auto& list = (state == State::Main) ? options : optOptions;

    // UI layout uses inverse viewScale so items are smaller when engine scales up
    float uiScale = 1.0f / viewScale;
    float baseX = 60.0f * uiScale;
    float baseY = 45.0f * uiScale;
    float itemW = 200.0f * uiScale;
    float itemH = 28.0f * uiScale;
    float itemGap = 36.0f * uiScale;

    for (size_t i = 0; i < list.size(); ++i) {
        SDL_FRect r{ baseX, baseY + float(i) * itemGap, itemW, itemH };
        if ((int)i == selected)
            SDL_SetRenderDrawColor(renderer, 200, 200, 80, 150);
        else
            SDL_SetRenderDrawColor(renderer, 160, 160, 160, 150);
        SDL_RenderFillRect(renderer, &r);

        if (font) {
            SDL_Color textColor = { 0, 0, 0, 255 };
            // Use solid rendering for pixel fonts (no AA)
            SDL_Surface* surf = TTF_RenderText_Solid(font, list[i].c_str(), 0, textColor);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
                    float tw = 0, th = 0;
                    SDL_GetTextureSize(tex, &tw, &th);
                    // scale by integer factor to avoid fractional scaling blur
                    int scale = 1;
                    if (tw > 0 && th > 0) {
                        float sx = r.w / tw;
                        float sy = r.h / th;
                        float smin = std::floor(std::min(sx, sy));
                        scale = std::max(1, (int)smin);
                    }
                    SDL_FRect dst{ r.x + (r.w - tw*scale) * 0.5f, r.y + (r.h - th*scale) * 0.5f, tw * scale, th * scale };
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_RenderTexture(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_free(surf);
            }
        }
    }
}

int Menu::consumeSelection() {
    if (!activated) return -1;

    // consume activation now and handle actions; return Play (0) to caller when chosen
    activated = false;

    if (state == State::Main) {
        if (selected == 0) {
            // Play selected - return to engine so it can start the game
            return 0;
        }
        else if (selected == 1) {
            // Options - enter options submenu
            state = State::Options;
            selected = 0;
            return -1;
        }
        else if (selected == 2) {
            // Quit
            SDL_Event e;
            e.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&e);
            return -1;
        }
    }
    else { // in options submenu
        if (selected == 0) {
            // toggle music
            if (gSound) {
                bool muted = gSound->isMusicMuted();
                gSound->setMusicMuted(!muted);
                optOptions[0] = std::string("Music: ") + (gSound->isMusicMuted() ? "Off" : "On");
            }
            return -1;
        }
        else if (selected == 1) {
            // Back to main menu
            state = State::Main;
            selected = 0;
            return -1;
        }
    }

    return -1;
}
