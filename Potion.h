#pragma once
#include "GameObject.h"

class Player;

class Potion : public GameObject {
public:
    enum class Type { HEALTH, MANA };

    Potion(SDL_Renderer* renderer,
           const std::string& spritePath,
           int tw,
           int th,
           float startX,
           float startY,
           Type t);

    void update(Map& map);
    SDL_FRect getRect() const;
    void draw(SDL_Renderer* renderer, int camX, int camY);

    // Called when player picks this up
    void onPickup(Player* player);

private:
    Type type;

    // Blink state: toggle visibility periodically to attract attention
    int blinkTimer = 0;       // ticks remaining until next toggle
    int blinkInterval = 30;   // ticks per visible/invisible phase (~0.5s at 60fps)
    bool blinkVisible = true; // whether currently visible
};
