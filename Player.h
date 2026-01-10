#pragma once
#include "GameObject.h"
#include "Controller.h"
#include "Map.h"

class Player : public GameObject {
public:
    Player(SDL_Renderer* renderer,
        const std::string& spritePath,
        int tw,
        int th);
    int jumpToken;
    void input(const bool* keys);

    // Controller-driven input overload (declared)
    void input(const Controller::State& cs);

    // Per-frame update (player-specific physics / jump handling)
    void update(Map& map);

    // Buffer to allow a quick tap of Down to be used with Jump to drop through
    int downPressBufferTimer;
    bool downPressedLastFrame;

    // Jump/buffer/coyote state for improved jumping
    int jumpBufferTimer = 0; // frames remaining in jump buffer
    int coyoteTimer = 0;     // frames remaining for coyote time
    bool jumpPressedLastFrame = false; // previous raw jump state (keyboard)
    bool jumpHeld = false;   // whether jump button is currently held
    bool downHeld = false;   // whether down is considered held (including buffer)

    // Configurable jump height (positive value; upward velocity will be -jumpHeight)
    float jumpHeight = 4.5f;

    // Double-jump strength as percentage of primary jump height (0.0 - 1.0)
    float doubleJumpPercent = 0.8f;
};