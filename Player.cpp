#include "Player.h"
#include <SDL3/SDL.h>
#include "Sound.h"
#include <cmath>

// tuning constants
static const int COYOTE_FRAMES = 6;        // allow jump for ~6 frames after leaving ground
static const int JUMP_BUFFER_FRAMES = 8;   // allow jump input to buffer for ~8 frames
static const float BASE_GRAVITY = 0.35f;   // must match GameObject::update base
static const float FAST_FALL_MULT = 1.6f;  // gravity multiplier when falling
static const float LOW_JUMP_MULT = 2.0f;   // gravity multiplier when release jump while rising
static const float HOLD_JUMP_MULT = 0.6f;  // gravity multiplier when holding jump while rising

Player::Player(SDL_Renderer* renderer,
    const std::string& spritePath,
    int tw,
    int th)
    : GameObject(renderer, spritePath, tw, th)
{
    // You can now safely access obj here
    obj.x = 50;
    obj.y = 100;
    jumpToken = 2;

    // buffer for allowing a quick tap of Down to be used with Jump
    downPressBufferTimer = 0;
    downPressedLastFrame = false;

    jumpBufferTimer = 0;
    coyoteTimer = 0;
    jumpPressedLastFrame = false;
    jumpHeld = false;
    downHeld = false;
}


void Player::input(const bool* keys) {
    int attRand = Sound::randomInt(1, 3);
    const char* attackSfx = (attRand == 1) ? "attack1" : (attRand == 2) ? "attack2" : "attack3";

    if (knockbackTimer > 0) {
        return;
    }

    if (keys[SDL_SCANCODE_J]) {
        if (!obj.attackKeyPressed && !obj.attacking) {
            obj.attacking = true;      // start attack
            obj.attackTimer = 5 * obj.attSpeed;  // 6 frames * 10 ticks per frame
            currentAnim = animAttack;
            currentAnim->reset();
            if (gSound) gSound->playSfx(attackSfx);
        }
        obj.attackKeyPressed = true;
    }
    else {
        obj.attackKeyPressed = false;
    }

    // Horizontal movement only if NOT attacking
    if (!obj.attacking) {
        obj.velx = 0;
        if (keys[SDL_SCANCODE_A]) { obj.velx = -2; obj.facing = false; }
        if (keys[SDL_SCANCODE_D]) { obj.velx = 2; obj.facing = true; }
    }
    else {
        obj.velx = 0;
    }
    
    if (obj.onGround) {
        jumpToken = 2;
        // jumpToken will be reset in update but keep here for parity
        if (obj.velx != 0 && !obj.attacking) {
            // prevent rapid retriggering: no overlap, min interval 200ms
            if (gSound) gSound->playSfx("step", 128, false, 100);
        }
    }

    // Jump buffering: detect rising edge and set buffer timer
    bool rawJump = keys[SDL_SCANCODE_SPACE];
    if (rawJump && !jumpPressedLastFrame) {
        jumpBufferTimer = JUMP_BUFFER_FRAMES;
    }
    jumpPressedLastFrame = rawJump;
    jumpHeld = rawJump;

    // If player presses Down + Jump while on ground -> drop through one-way platforms
    // Down press buffer (allow quick tap of down to combo with jump to drop-through)
    bool currentDown = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    // detect tap of Down and start a short buffer window
    if (currentDown && !downPressedLastFrame) {
        downPressBufferTimer = 12; // ~12 frames window to press jump
    }
    // update member downHeld
    downHeld = currentDown || (downPressBufferTimer > 0);
    if (downPressBufferTimer > 0) --downPressBufferTimer;

    // Note: immediate jump is handled in update() to support buffering/coyote/double-jump

    downPressedLastFrame = currentDown;
}

// Controller input overload
void Player::input(const Controller::State& cs) {
    int attRand = Sound::randomInt(1, 3);
    const char* attackSfx = (attRand == 1) ? "attack1" : (attRand == 2) ? "attack2" : "attack3";

    if (knockbackTimer > 0) {
        return;
    }

    // Attack via controller
    if (cs.attack) {
        if (!obj.attackKeyPressed && !obj.attacking) {
            obj.attacking = true;
            obj.attackTimer = 5 * obj.attSpeed;
            currentAnim = animAttack;
            currentAnim->reset();
            if (gSound) gSound->playSfx(attackSfx);
        }
        obj.attackKeyPressed = true;
    } else {
        obj.attackKeyPressed = false;
    }

    // Horizontal movement only if NOT attacking
    if (!obj.attacking) {
        obj.velx = 0;
        if (cs.left) { obj.velx = -2; obj.facing = false; }
        if (cs.right) { obj.velx = 2; obj.facing = true; }
    } else {
        obj.velx = 0;
    }

    if (obj.onGround) {
        // jumpToken reset handled in update
        if (obj.velx != 0 && !obj.attacking) {
            if (gSound) gSound->playSfx("step", 128, false, 100);
        }
    }

    // Use controller-provided rising-edge flag so repeated frames of a held button
    // do not consume multiple jumps. This prevents the "stuck" double-jump issue.
    // Allow a quick tap of Down to count for a subsequent Jump press via buffer
    bool currentDown = cs.down;
    if (currentDown && !downPressedLastFrame) {
        downPressBufferTimer = 12;
    }
    // update member downHeld
    downHeld = currentDown || (downPressBufferTimer > 0);
    if (downPressBufferTimer > 0) --downPressBufferTimer;

    if (cs.jumpPressed) {
        jumpBufferTimer = JUMP_BUFFER_FRAMES;
    }
    // controller reports current jump raw state in cs.jump
    jumpHeld = cs.jump;

    downPressedLastFrame = currentDown;
}

// Per-frame update for Player: handle jump buffering, coyote time, and improved gravity
void Player::update(Map& map)
{
    // Reset jumpToken and coyote when standing on ground
    if (obj.onGround) {
        jumpToken = 2;
        coyoteTimer = COYOTE_FRAMES;
    }

    // If jump was buffered and we are allowed to jump, perform jump now
    if (jumpBufferTimer > 0 && jumpToken > 0 && !obj.attacking) {
        // Detect whether there's a one-way platform under the player's feet even
        // if obj.onGround might be false due to minor float differences. This makes
        // drop-through more forgiving when the player is effectively standing on
        // a one-way tile.
        int leftTile = int(obj.x / map.TILE_SIZE);
        int rightTile = int((obj.x + obj.tileWidth - 1) / map.TILE_SIZE);
        int footTile = int((obj.y + obj.tileHeight) / map.TILE_SIZE);

        bool oneWayBelow = false;
        int colL = map.getCollision(leftTile, footTile);
        int colR = map.getCollision(rightTile, footTile);
        if (colL == Map::COLL_ONEWAY || colR == Map::COLL_ONEWAY) {
            oneWayBelow = true;
        }

        // If player intends to drop through platforms (either holding down or used the tap buffer)
        // and there's a one-way tile beneath their feet (or they are onGround), trigger drop-through.
        if ((downHeld || downPressBufferTimer > 0) && (obj.onGround || oneWayBelow)) {
            obj.ignoreOneWayTimer = 12; // drop-through window
            jumpBufferTimer = 0;
            // Ensure we actually start falling so the one-way collision check
            // won't immediately re-assert the ground. Clear onGround and give
            // a small downward nudge to begin falling through the platform.
            obj.onGround = false;
            obj.vely = 1.0f;
        }
        // Ground (or coyote) jump: perform full jump
        else if (obj.onGround || coyoteTimer > 0) {
            obj.vely = -jumpHeight;
            obj.onGround = false;
            jumpToken--;
            jumpBufferTimer = 0;
            if (jumpToken == 1) {
                if (gSound) gSound->playSfx("jump2");
            } else {
                if (gSound) gSound->playSfx("jump1");
            }
        }
        // Mid-air double-jump: compute correct impulse that accounts for gravity
        else if (!obj.onGround && jumpToken > 0) {
            float pct = doubleJumpPercent;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 1.0f) pct = 1.0f;
            // desired initial velocity magnitude ~ sqrt(percent) * primary velocity
            float djVel = -std::sqrt(pct) * jumpHeight;
            // If currently moving upwards faster than djVel, keep the stronger upward velocity.
            obj.vely = std::min(obj.vely, djVel);
            obj.onGround = false;
            jumpToken--;
            jumpBufferTimer = 0;
            if (gSound) gSound->playSfx("jump2");
        }
    }

    // Apply custom gravity modifiers BEFORE GameObject::update.
    // GameObject::update will add BASE_GRAVITY; to achieve a desired gravity
    // this frame we set obj.vely so after GameObject::update's +BASE_GRAVITY it matches desired.
    float desiredVel = obj.vely;
    if (obj.vely > 0.0f) { // falling
        desiredVel += BASE_GRAVITY * FAST_FALL_MULT;
    } else if (obj.vely < 0.0f) { // rising
        if (jumpHeld) {
            desiredVel += BASE_GRAVITY * HOLD_JUMP_MULT;
        } else {
            desiredVel += BASE_GRAVITY * LOW_JUMP_MULT;
        }
    } else {
        // not moving vertically: apply normal gravity
        desiredVel += BASE_GRAVITY;
    }

    // Set obj.vely so that GameObject::update's addition results in desiredVel
    obj.vely = desiredVel - BASE_GRAVITY;

    // Call base update which handles movement and collisions
    GameObject::update(map);

    // Decrement timers
    if (jumpBufferTimer > 0) --jumpBufferTimer;
    if (coyoteTimer > 0) --coyoteTimer;
}