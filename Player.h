#pragma once
#include "GameObject.h"
#include "Controller.h"
#include "Map.h"

// Player: extends GameObject with input handling, jumping, and charged attack logic
class Player : public GameObject {
public:
    // Constructor
    Player(SDL_Renderer* renderer,
        const std::string& spritePath,
        int tw,
        int th);

    // Per-frame input (keyboard)
    void input(const bool* keys);
    // Per-frame input (controller)
    void input(const Controller::State& cs);

    // Per-frame update (physics, jump handling, charge attack progression)
    void update(Map& map);

    // --- Jump related state ---
    // Number of available jumps (resets on ground)
    int jumpToken;

    // Small buffer to allow a quick tap of Down then Jump to drop through one-way platforms
    int downPressBufferTimer;
    bool downPressedLastFrame;
    bool downHeld = false; // whether down is considered held (including buffer)

    // Jump/coyote/buffer state for forgiving jumping
    int jumpBufferTimer = 0; // frames remaining in jump buffer
    int coyoteTimer = 0;     // frames remaining for coyote time
    bool jumpPressedLastFrame = false; // previous raw jump state
    bool jumpHeld = false;   // whether jump button is currently held

    // Configurable jump tuning
    float jumpHeight = 4.5f;           // positive value; upward velocity will be -jumpHeight
    float doubleJumpPercent = 0.8f;    // double-jump strength [0.0 .. 1.0]

    // --- Charge attack state ---
    bool chargeAttackKeyPressed = false; // raw input state for charge attack key
    bool chargeCharging = false;         // currently in charge wind-up
    int chargeTimer = 0;                 // ticks spent charging
    int chargeFrame = 0;                 // animation frame index for charge wind-up
    bool chargePressedLastFrame = false; // keyboard rising-edge detection for charge

    // Dash state produced by successful charge
    bool chargeDashing = false; // currently performing dash
    int chargeDashTimer = 0;    // remaining ticks for dash

    // Mash-loop state: repeat last 7 frames while mashing to extend charge
    int chargeMashTimer = 0;      // ticks remaining for mash hold
    int chargeMashPhase = 0;      // index 0..6 within the repeated last-7 frames
    int chargeMashPhaseTick = 0;  // tick counter to time frame advance while mashing
    bool chargeMashMode = false;  // whether currently in mash-extension mode
    bool pendingMashPress = false; // records presses during wind-up to apply when mash window opens
    bool chargeDashed = false;    // whether dash (on CHARGE_FRAMES) already occurred
    int chargeMashMagicTicks = 0; // tick counter to drain magic while mashing

    // Misc
    int baseDamage = 35; // player's normal attack damage

    // Adjustable attack hitbox width (in pixels). Can be tuned per-player.
    float attackHitboxWidth = 15.9f;

    // Whether player has a key to open doors (default false). Set this later when keys are collected.
    bool hasKey = false;
    // Override to provide player-specific attack rect width
    SDL_FRect getAttackRect() const override;
};