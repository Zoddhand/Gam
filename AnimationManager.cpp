
#include "AnimationManager.h"

AnimationManager::AnimationManager(SDL_Texture* tex, int frameW, int frameH, int frames, int rowY,
    int innerX, int innerY, int innerW, int innerH)
    : texture(tex),
    frameWidth(frameW),
    frameHeight(frameH),
    frameCount(frames),
    animY(rowY),
    spriteOffsetX(innerX),
    spriteOffsetY(innerY),
    spriteWidth(innerW),
    spriteHeight(innerH)
{
}

void AnimationManager::update() {
    timer++;
    if (timer >= speed) {  // use variable speed
        currentFrame = (currentFrame + 1) % frameCount;
        timer = 0;
    }
}


void AnimationManager::setRow(int rowY, int frames) {
    animY = rowY;
    frameCount = frames;
    currentFrame = 0;
    timer = 0;
}

void AnimationManager::reset() {
    currentFrame = 0;
    timer = 0;
}

SDL_FRect AnimationManager::getSrcRect()
{
    SDL_FRect rect = { currentFrame * frameWidth, animY, frameWidth, frameHeight };
    return rect;
}
