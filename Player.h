#pragma once
#include "GameObject.h"
#include "Controller.h"

class Player : public GameObject {
public:
    Player(SDL_Renderer* renderer,
        const std::string& spritePath,
        int tw,
        int th);
    int jumpToken;
    void input(const bool* keys);

    // Controller-driven input overload (declared)
    void input(const Controller::State& cs);
};