#pragma once
#include "MapObject.h"
#include "Map.h"

class ArrowTrap :
    public MapObject
{
public:
    ArrowTrap(SDL_Renderer* renderer, int tileX, int tileY, int tileIndex);
    void update(GameObject& obj, Map& map) override;
    // Called when an external trigger (e.g., a pressure plate) wants this trap to fire
    void triggerExternal();
    void setAutoFire(bool v) { autoFire = v; }

private:
    SDL_Renderer* renderer = nullptr;
    int cooldownTicks = 60; // frames between shots
    int cooldownTimer = 0;
    int ammo = 200;
    bool playerWasInZone = false; // prevent firing every frame while player remains in zone
    bool plateWasTriggered = false; // track previous frame pressure-plate trigger state
    bool externalTriggered = false; // set by external triggers (pressure plates)
    // Animation control: when true, play the trap's animation once then stop
    bool animPlaying = false;
    int animPlaySpeed = 6; // ticks per frame when playing
    bool autoFire = false; // when true, trap will fire on sight (in front)
};

