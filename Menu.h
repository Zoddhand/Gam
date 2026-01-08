#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <SDL3_ttf/SDL_ttf.h>
#include "Player.h" // for Controller::State

class Menu {
public:
    Menu(SDL_Renderer* renderer, float viewScale = 1.0f);
    ~Menu();

    void handleInput(const bool* keys);
    void handleInput(const Controller::State& cs); // controller overload
    void update();
    void render(SDL_Renderer* renderer);

    // Returns the selected option index and resets the selection flag; returns -1 when none
    int consumeSelection();

private:
    enum class State { Main, Options } state = State::Main;

    std::vector<std::string> options; // main menu
    std::vector<std::string> optOptions; // options menu
    int selected = 0;
    bool activated = false;

    // simple keyboard edge detection (separate for keyboard + controller)
    bool upLastKbd = false;
    bool downLastKbd = false;
    bool enterLastKbd = false;

    bool upLastCtrl = false;
    bool downLastCtrl = false;
    bool enterLastCtrl = false;

    // font
    TTF_Font* font = nullptr;
    int fontSize = 14;

    // background image
    SDL_Texture* bgTex = nullptr;

    // rendering scale (from engine view scale)
    float viewScale = 1.0f;
};
