#pragma once
#include <SDL3/SDL.h>

class AnimationManager {
public:
    AnimationManager(SDL_Texture* tex, int frameW, int frameH, int frames, int rowY,
        int innerX, int innerY, int innerW, int innerH);

    void update();
    void setRow(int rowY, int frames);
    void reset();
    void setFlip(bool flip) { flipped = flip; }

    // Get full source rectangle of current frame
    SDL_FRect getSrcRect();

    // Inner sprite info for scaling
    int getInnerX() const { return spriteOffsetX; }
    int getInnerY() const { return spriteOffsetY; }
    int getInnerW() const { return spriteWidth; }
    int getInnerH() const { return spriteHeight; }

    SDL_Texture* getTexture() const { return texture; }

    int frameWidth;
    int frameHeight;
    SDL_Texture* texture = nullptr;
    bool flipped = false;

    int frameCount;
    int currentFrame = 0;
    int animY;
    int timer = 0;
    int speed = 10; // default ticks per frame
    void setSpeed(int s) { speed = s; }


    // Inner sprite (hitbox mapping)
    int spriteOffsetX;
    int spriteOffsetY;
    int spriteWidth;
    int spriteHeight;
};