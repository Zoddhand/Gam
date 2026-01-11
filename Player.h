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
    // Draw override to render debug visuals
    // void draw(SDL_Renderer* renderer, int camX, int camY); // removed

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

    // Charged attack state
    bool chargeAttackKeyPressed = false; // whether keyboard charge attack key is held
    bool chargeCharging = false; // whether currently charging
    int chargeTimer = 0; // ticks spent charging
    int chargeFrame = 0; // animation frame count for charge
    bool chargePressedLastFrame = false; // keyboard rising-edge detection for charge

    // Dash state produced by successful charge
    bool chargeDashing = false; // whether currently performing dash
    int chargeDashTimer = 0; // remaining ticks for dash
    // Mash-loop state: repeat last 7 frames while mashing
    int chargeMashTimer = 0; // ticks remaining for mash hold
    int chargeMashPhase = 0; // index 0..6 within the repeated last-7 frames
    int chargeMashPhaseTick = 0; // tick counter to time frame advance while mashing
    bool chargeMashMode = false; // whether currently in mash-extension mode
    bool pendingMashPress = false; // records presses during wind-up to apply when mash window opens
    bool chargeDashed = false; // whether dash (on 6th frame) already occurred
    int baseDamage = 35; // player's normal attack damage

    // Tick counter to accumulate real-time seconds for magic drain while mashing
    int chargeMashMagicTicks = 0;
};