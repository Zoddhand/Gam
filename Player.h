#pragma once
#include "GameObject.h"

class Player : public GameObject {
public:
    Player(SDL_Renderer* renderer,
        const std::string& spritePath,
        int tw,
        int th);
        int jumpToken;
    void input(const bool* keys);
};
