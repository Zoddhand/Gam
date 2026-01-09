#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include "Controller.h"
#include <SDL3_ttf/SDL_ttf.h>

class GameOver {
public:
    GameOver(SDL_Renderer* renderer, float viewScale = 1.0f);
    ~GameOver();

    // Input: keyboard or controller state
    void handleInput(const bool* keys);
    void handleInput(const Controller::State& cs);

    void update();
    void render(SDL_Renderer* renderer);

    // returns 0 = restart, 1 = quit, -1 = none
    int consumeSelection();

private:
    std::vector<std::string> options;
    int selected = 0;
    bool activated = false;
    float viewScale = 1.5f;
    // simple keyboard edge detection
    bool upLastKbd = false;
    bool downLastKbd = false;
    bool enterLastKbd = false;
    bool upLastCtrl = false;
    bool downLastCtrl = false;
    bool enterLastCtrl = false;

    // font
    TTF_Font* font = nullptr;
    int fontSize = 18;

    // a simple darkened background texture optional
    SDL_Texture* bgTex = nullptr;
};
