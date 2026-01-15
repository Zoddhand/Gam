#pragma once
#include "GameObject.h"
#include "Player.h"
#include "Map.h"
#include <SDL3/SDL.h>
#include <vector>

class Arrow; // forward

class Archer : public GameObject {
public:
    Archer(SDL_Renderer* renderer,
        const std::string& spritePath,
        int tw,
        int th,
        float startX,
        float startY,
        int dam = 10);

    // Update AI; may push new arrows into projectiles vector
    void aiUpdate(Player& player, Map& map, std::vector<class Arrow*>& projectiles);

private:
    int shootCooldown = 0; // frames until next shot
    const int SHOOT_COOLDOWN_MAX = 120; // 60 ticks ~ 1s
    float range = 180.0f; // pixels
    float chaseSpeed = 1.0f; // horizontal speed when chasing player
    SDL_Renderer* rendererPtr = nullptr; // renderer for projectile creation
    bool shotFired = false; // whether arrow has been spawned in current attack animation
};
