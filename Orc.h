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

    // Random pause (idle) behaviour to make patrols feel alive
    bool paused = false;            // whether currently paused
    int pauseTimer = 0;            // frames remaining in current pause
    int pauseCooldown = 0;         // frames until next possible pause

    // configuration (frames)
    const int PAUSE_MIN = 30;      // 0.5s at 60fps
    const int PAUSE_MAX = 180;     // 3s at 60fps
    const int COOLDOWN_MIN = 180;  // 3s
    const int COOLDOWN_MAX = 780;  // 12s
};
