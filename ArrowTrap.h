#pragma once
#include "MapObject.h"
#include "Map.h"

class ArrowTrap :
    public MapObject
{
public:
    ArrowTrap(SDL_Renderer* renderer, int tileX, int tileY, int tileIndex);
    void update(GameObject& obj, Map& map) override;

private:
    SDL_Renderer* renderer = nullptr;
    int cooldownTicks = 60; // frames between shots
    int cooldownTimer = 0;
    int ammo = 2;
    bool playerWasInZone = false; // prevent firing every frame while player remains in zone
};

