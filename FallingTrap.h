#pragma once
#include "WorldObject.h"
#include "Map.h"

class FallingTrap : public WorldObject {
public:
    FallingTrap(SDL_Renderer* renderer, int tileX, int tileY);

    void update(Map& map);
    void remove();
    void checkTrigger(class GameObject& actor, Map& map);

private:
    bool triggered = false;
    bool falling = false;
    float velY = 0.0f;
    float gravity = 0.35f;

    int delayTicks = 15;
    int delayTimer = 0;
    int killTimer = 0;
    bool finishedFalling = false;
    GameObject* triggerer = nullptr;
};
