#pragma once
#include <SDL3/SDL.h>
#include <string>

class WorldObject {
public:
    float x = 0, y = 0;
    int width = 16, height = 16;
    bool alive = true;

    WorldObject(SDL_Renderer* renderer,
        const std::string& spritePath,
        int w,
        int h);

    virtual ~WorldObject();

    virtual void update() {}
    virtual void draw(SDL_Renderer* renderer, int camX, int camY);
    SDL_FRect getRect() const;

protected:
    SDL_Texture* texture = nullptr;
};
