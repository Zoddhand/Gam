#pragma once
#include "GameObject.h"
#include "Player.h"

class Orc : public GameObject {
public:

    Orc(SDL_Renderer* renderer,
        const std::string& spritePath,
        int tw,
        int th,
        float startX,
        float startY,
        int dam,
		bool block = false
    );

    void aiUpdate(Player& player, Map& map);
    SDL_FRect getAttackRect() const override;
    bool canBlock = false;

private:
    // handle blocking timing
    int blockTimer = 0; // frames remaining in block stance
    const int BLOCK_DURATION = 15; // default block duration in ticks
};
