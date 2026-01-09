#pragma once
#include <SDL3/SDL.h>
#include "Map.h"

class Arrow {
public:
    Arrow(SDL_Renderer* renderer, float x, float y, float vx, float vy);
    ~Arrow();

    void update(Map& map);
    void draw(SDL_Renderer* renderer, int camX, int camY);
    SDL_FRect getRect() const;
    bool alive = true;

private:
    float x, y;
    float velx, vely;
    int w = 19, h = 7;
    SDL_Texture* tex = nullptr;
};
