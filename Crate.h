#pragma once
#include "MapObject.h"
#include "GameObject.h"
#include "Map.h"

class Crate : public MapObject {
public:
    Crate(int tileX, int tileY, int tileIndex);
    ~Crate();

    void update(GameObject& player, Map& map) override;
    void draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const override;

    SDL_FRect getRect() { return { x, y, float(w), float(h) }; }

    bool movable = true; // can be pushed
    // Physics
    float velx = 0.0f;
    float vely = 0.0f;
    bool onGround = false;
    int hitInvuln = 0; // prevent repeated pushes while attack held
};
