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
        int dam
    );

    void aiUpdate(Player& player, Map& map);
    SDL_FRect getAttackRect() const override;
};
