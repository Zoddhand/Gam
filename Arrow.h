#pragma once
#include <SDL3/SDL.h>
#include "Map.h"

class Arrow {
public:
    Arrow(SDL_Renderer* renderer, float x, float y, float vx, float vy, bool trapSprite = false);
    ~Arrow();

    void update(Map& map);
    void draw(SDL_Renderer* renderer, int camX, int camY);
    SDL_FRect getRect() const;
    bool alive = true;
    bool isTrapArrow = false;
    float prevX = 0.0f;
    float prevY = 0.0f;
    float getVelX() const;
    float getVelY() const;

private:
    float x, y;
    float velx, vely;
    int w = 19, h = 7;
    SDL_Texture* tex = nullptr;
};
