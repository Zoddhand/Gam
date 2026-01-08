#include "Player.h"
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
            gSound->playSfx(attackSfx);
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
            gSound->playSfx("step");
        }
    }
    // Jump (optional: only if not attacking)
    static bool jumpPressedLastFrame = false;


    if (keys[SDL_SCANCODE_SPACE] && !jumpPressedLastFrame && !obj.attacking && jumpToken > 0)
    {
        obj.vely = -5.5f;
        obj.onGround = false;
        jumpToken--;
        if(jumpToken == 1)
            gSound->playSfx("jump2");
		else
            gSound->playSfx("jump1");
    }

    jumpPressedLastFrame = keys[SDL_SCANCODE_SPACE];
}