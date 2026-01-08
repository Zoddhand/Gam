#pragma once
#include <SDL3/SDL.h>
#include <string>

class Hud {
public:
    Hud(SDL_Renderer* renderer, const std::string& spritePath);
    ~Hud();

    // Draw the heart in the top-left. health/maxHealth mapped to frames (0 = full, N-1 = empty)
    void draw(SDL_Renderer* renderer, float health, float maxHealth);

private:
    SDL_Texture* tex = nullptr;
    int frameW = 11;
    int frameH = 11;    
    int frameCount = 6;
};

