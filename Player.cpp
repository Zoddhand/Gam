#include "Player.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include "Sound.h"

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
        if (obj.velx != 0 && !obj.attacking) {
            // prevent rapid retriggering: no overlap, min interval 200ms
            if (gSound) gSound->playSfx("step", 128, false, 100);
        }
    }
    // Jump (optional: only if not attacking)
    static bool jumpPressedLastFrame = false;


    // If player presses Down + Jump while on ground -> drop through one-way platforms
    bool downHeld = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    if (keys[SDL_SCANCODE_SPACE] && !jumpPressedLastFrame && !obj.attacking && jumpToken > 0)
    {
        if (downHeld && obj.onGround)
        {
            // Request temporary ignore of one-way platforms (frames). Do NOT perform jump.
            obj.ignoreOneWayTimer = 12; // ~12 frames; tune as needed
            // do not consume jumpToken or set vely (we are dropping)
        }
        else
        {
            // Normal jump
            obj.vely = -5.5f;
            obj.onGround = false;
            jumpToken--;
            if(jumpToken == 1) {
                if (gSound) gSound->playSfx("jump2");
            } else {
                if (gSound) gSound->playSfx("jump1");
            }
        }
    }

    jumpPressedLastFrame = keys[SDL_SCANCODE_SPACE];
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
        jumpToken = 2;
        if (obj.velx != 0 && !obj.attacking) {
            if (gSound) gSound->playSfx("step", 128, false, 100);
        }
    }

    // Use controller-provided rising-edge flag so repeated frames of a held button
    // do not consume multiple jumps. This prevents the "stuck" double-jump issue.
    if (cs.jumpPressed && !obj.attacking && jumpToken > 0) {
        if (cs.down && obj.onGround) {
            obj.ignoreOneWayTimer = 12; // drop-through window
        } else {
            obj.vely = -5.5f;
            obj.onGround = false;
            jumpToken--;
            if (jumpToken == 1) {
                if (gSound) gSound->playSfx("jump2");
            } else {
                if (gSound) gSound->playSfx("jump1");
            }
        }
    }
}