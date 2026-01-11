#include "Player.h"
#include <SDL3/SDL.h>
#include "Sound.h"
#include <cmath>
#include "Engine.h"
#include "Orc.h"
#include "Archer.h"

// tuning constants
static const int COYOTE_FRAMES = 6;        // allow jump for ~6 frames after leaving ground
static const int JUMP_BUFFER_FRAMES = 8;   // allow jump input to buffer for ~8 frames
static const float BASE_GRAVITY = 0.35f;   // must match GameObject::update base
static const float FAST_FALL_MULT = 1.6f;  // gravity multiplier when falling
static const float LOW_JUMP_MULT = 2.0f;   // gravity multiplier when release jump while rising
static const float HOLD_JUMP_MULT = 0.6f;  // gravity multiplier when holding jump while rising

// Charge attack tuning
static const int CHARGE_FRAMES = 6; // on the 6th frame dash occurs (frames are 1-based)
static const int CHARGE_TICKS_PER_FRAME = 5; // attackTimer uses 5 * attSpeed earlier; use similar granularity
static const float CHARGE_DASH_SPEED = 6.0f; // horizontal dash velocity applied on dash
static const float CHARGE_DASH_DURATION = 10.0f; // ticks that dash persists (approx)
static const float CHARGE_DAMAGE = 40.0f; // damage dealt by charged dash
static const float CHARGE_PREMOVE_SPEED = 1.2f; // small forward movement during charge wind-up
static const int CHARGE_MASH_MAX_TICKS = 20; // how long mash window persists without presses
static const int CHARGE_MASH_TICKS_PER_FRAME = 4; // speed of frame advance while mashing

// Magic costs
static const float CHARGE_START_COST = 20.0f; // cost to start charge
static const float CHARGE_MASH_COST_PER_SECOND = 10.0f; // cost per mashed second
static const int TICKS_PER_SECOND = 60; // engine tick rate

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

    // charge attack state defaults
    chargeAttackKeyPressed = false;
    chargeCharging = false;
    chargeTimer = 0;
    chargeFrame = 0;
    chargePressedLastFrame = false;
}


void Player::input(const bool* keys) {
    int attRand = Sound::randomInt(1, 3);
    const char* attackSfx = (attRand == 1) ? "attack1" : (attRand == 2) ? "attack2" : "attack3";

    if (knockbackTimer > 0) {
        return;
    }

    // Charged attack via keyboard: K key
    bool rawCharge = keys[SDL_SCANCODE_K];
    // detect rising edge so a single press triggers the charge sequence
    if (rawCharge && !chargePressedLastFrame) {
        // start charging immediately on press (no hold required)
        if (!chargeCharging && !obj.attacking) {
            if (obj.magic >= CHARGE_START_COST) {
                obj.magic -= CHARGE_START_COST;
                chargeCharging = true;
                chargeTimer = 0;
                chargeFrame = 0;
                chargeMashMode = false; // ensure we begin with full pre-dash wind-up
                chargeMashPhase = 0;
                chargeMashPhaseTick = 0;
                chargeMashMagicTicks = 0;
                currentAnim = animPlayerCharge;
                if (currentAnim) currentAnim->reset();
                if (gSound) gSound->playSfx("charge_start");
            } else {
                SDL_Log("Player: not enough magic to start charge (%f required)", CHARGE_START_COST);
            }
        }
    }
    chargePressedLastFrame = rawCharge;
    // track current raw state for update-time logic
    chargeAttackKeyPressed = rawCharge;
    // record pending mash presses during wind-up so they apply when mash window opens
    if (rawCharge && chargeCharging && !chargeMashMode) {
        pendingMashPress = true;
    }

    // If K is released before dash completes, cancel charging (but dash should occur automatically when timer reaches frame)
    if (!rawCharge && chargeCharging && !chargeDashing) {
        // allow charge to continue once started until it either completes or is explicitly canceled by some other action
        // in this design, single press starts charge and releasing does NOT cancel automatically; comment out cancel
        // chargeCharging = false;
        // chargeTimer = 0;
        // chargeFrame = 0;
        // if (!obj.attacking) currentAnim = animIdle;
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

    // Horizontal movement only if NOT attacking and NOT charging (unless dashing)
    if (!obj.attacking && !chargeCharging && !chargeDashing) {
        obj.velx = 0;
        if (keys[SDL_SCANCODE_A]) { obj.velx = -2; obj.facing = false; }
        if (keys[SDL_SCANCODE_D]) { obj.velx = 2; obj.facing = true; }
    }
    else if (!chargeDashing) {
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

    // Charged attack via controller: use attackChargedPressed edge
    if (cs.attackChargedPressed) {
        if (!chargeCharging && !obj.attacking) {
            if (obj.magic >= CHARGE_START_COST) {
                obj.magic -= CHARGE_START_COST;
                chargeCharging = true;
                chargeTimer = 0;
                chargeFrame = 0;
                chargeMashMagicTicks = 0;
                currentAnim = animPlayerCharge;
                if (currentAnim) currentAnim->reset();
                if (gSound) gSound->playSfx("charge_start");
            } else {
                SDL_Log("Player: not enough magic to start charge (%f required)", CHARGE_START_COST);
            }
        }
    }
    // update raw state
    chargeAttackKeyPressed = cs.attackCharged;
    // record pending mash presses during wind-up for controller
    if (cs.attackCharged && chargeCharging && !chargeMashMode) pendingMashPress = true;

    // Do NOT cancel charging on controller button release; charging should complete its wind-up.

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

    // Horizontal movement only if NOT attacking and NOT charging (unless dashing)
    if (!obj.attacking && !chargeCharging && !chargeDashing) {
        obj.velx = 0;
        if (cs.left) { obj.velx = -2; obj.facing = false; }
        if (cs.right) { obj.velx = 2; obj.facing = true; }
    } else if (!chargeDashing) {
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

    // Handle charge attack progression
    if (chargeCharging && !chargeDashing) {
        ++chargeTimer;
        // small forward movement during wind-up before mash window
        if (!chargeMashMode) {
            obj.velx = obj.facing ? CHARGE_PREMOVE_SPEED : -CHARGE_PREMOVE_SPEED;
        }

        // compute current frame index based on ticks
        int newFrame = chargeTimer / CHARGE_TICKS_PER_FRAME; // 0-based
        if (newFrame != chargeFrame) {
            chargeFrame = newFrame;
            // when we reach the CHARGE_FRAMES perform the dash (6th frame)
            if (chargeFrame + 1 == CHARGE_FRAMES) {
                // perform dash now: set attacking state and dash velocity
                obj.attacking = true;
                obj.attackTimer = obj.attSpeed * 2;
                obj.velx = (obj.facing ? CHARGE_DASH_SPEED : -CHARGE_DASH_SPEED);
                chargeDashing = true;
                chargeDashTimer = int(CHARGE_DASH_DURATION);
                chargeDashed = true; // mark that dash occurred during the animation
                if (gSound) gSound->playSfx("charge_dash");
            }
        }
    }

    // If dash occurred and the charge animation has reached its final frame, open the mash window
    if (chargeDashed && !chargeMashMode && animPlayerCharge && animPlayerCharge->currentFrame == animPlayerCharge->frameCount - 1) {
        chargeMashMode = true;
        chargeMashTimer = CHARGE_MASH_MAX_TICKS;
        chargeMashPhase = 0;
        chargeMashPhaseTick = 0;
        // If player was mashing during wind-up, apply it now
        if (pendingMashPress) {
            chargeAttackKeyPressed = true;
            pendingMashPress = false;
        }
        // If player not pressing now, close charging and return to idle
        if (!chargeAttackKeyPressed) {
            chargeCharging = false;
            chargeDashed = false;
            chargeTimer = 0;
            chargeFrame = 0;
            chargeMashMode = false;
            if (currentAnim == animPlayerCharge) {
                currentAnim = animIdle;
                currentAnim->setSpeed(10);
            }
        }
    }

    // Mash-extension handling: while in mash mode, repeat last 7 frames when key is pressed; otherwise timeout -> nothing
    if (chargeMashMode && !chargeDashing && animPlayerCharge) {
         int totalFrames = animPlayerCharge->frameCount;
         int last7Start = std::max(0, totalFrames - 7);

         if (chargeAttackKeyPressed) {
                     // reset mash timer to keep extending while player continues to press/mash
                     chargeMashTimer = CHARGE_MASH_MAX_TICKS;
                     // hold position
                     obj.velx = 0;
 
                     // advance mash frame timing
                     ++chargeMashPhaseTick;
                     if (chargeMashPhaseTick >= CHARGE_MASH_TICKS_PER_FRAME) {
                         chargeMashPhaseTick = 0;
                         chargeMashPhase = (chargeMashPhase + 1) % 7;
                     }
 
                     animPlayerCharge->currentFrame = last7Start + chargeMashPhase;
                     animPlayerCharge->timer = 0;

                    // Drain magic over time while mashing: 10 magic per second
                    ++chargeMashMagicTicks;
                    if (chargeMashMagicTicks >= TICKS_PER_SECOND) {
                        chargeMashMagicTicks = 0;
                        if (obj.magic >= CHARGE_MASH_COST_PER_SECOND) {
                            obj.magic -= CHARGE_MASH_COST_PER_SECOND;
                        } else {
                            // Not enough magic to continue mashing: end mash mode
                            chargeMashMode = false;
                            chargeCharging = false;
                            chargeDashed = false;
                            chargeTimer = 0;
                            chargeFrame = 0;
                            if (currentAnim == animPlayerCharge) {
                                currentAnim = animIdle;
                                currentAnim->setSpeed(10);
                            }
                            chargeAttackKeyPressed = false;
                            chargeMashMagicTicks = 0;
                        }
                    }

                    // Deal damage each mash cycle to any enemies in front
                    // Use the same rect as the debug hitbox so it matches what is shown
                    float atkW = float(obj.tileWidth) * 2.0f;
                    float atkH = 8.0f;
                    float atkX = obj.facing ? obj.x + obj.tileWidth : obj.x - atkW;
                    float atkY = obj.y + 4.0f;
                    SDL_FRect atk = { atkX, atkY, atkW, atkH };

                    // Check orcs
                    for (auto* e : gEngine->orc) {
                        if (!e || !e->obj.alive) continue;
                        SDL_FRect er = e->getRect();
                        if (SDL_HasRectIntersectionFloat(&atk, &er)) {
                            float atkCenter = atk.x + atk.w * 0.5f;
                            e->takeDamage(baseDamage, atkCenter, 3, 6, 30, 2.5f, -4.0f);
                        }
                    }
                    // Check archers
                    for (auto* a : gEngine->archers) {
                        if (!a || !a->obj.alive) continue;
                        SDL_FRect ar = a->getRect();
                        if (SDL_HasRectIntersectionFloat(&atk, &ar)) {
                            float atkCenter = atk.x + atk.w * 0.5f;
                            a->takeDamage(baseDamage, atkCenter, 3, 6, 30, 2.5f, -4.0f);
                        }
                    }
                 } else {
            // not pressing: count down; when expires, end mash mode and clear state
            if (chargeMashTimer > 0) --chargeMashTimer;
            if (chargeMashTimer <= 0) {
                chargeMashMode = false;
                chargeCharging = false;
                chargeDashed = false;
                chargeTimer = 0;
                chargeFrame = 0;
                chargeMashMagicTicks = 0;
                if (currentAnim == animPlayerCharge) {
                    currentAnim = animIdle;
                    currentAnim->setSpeed(10);
                }
            }
        }
     }

    // If we're attacking (including from charge) GameObject::update will handle attackTimer countdown

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

    // If we're in any charge-related state, request GameObject::update preserve our animation
    if ((chargeCharging || chargeMashMode || chargeDashed) && animPlayerCharge) {
        preventAnimOverride = true;
        currentAnim = animPlayerCharge;
        currentAnim->setSpeed(std::max(1, CHARGE_TICKS_PER_FRAME));
    } else {
        preventAnimOverride = false;
    }

    // Call base update which handles movement and collisions (will respect preventAnimOverride)
    GameObject::update(map);

    // after update: nothing to do here for animation override

    // Decrement timers
    if (jumpBufferTimer > 0) --jumpBufferTimer;
    if (coyoteTimer > 0) --coyoteTimer;

    if (chargeDashing) {
        if (chargeDashTimer > 0) {
            --chargeDashTimer;
        } else {
            chargeDashing = false;
            // stop horizontal momentum after dash
            obj.velx = 0;
        }
    }
}