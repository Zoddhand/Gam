#pragma once
#include <SDL3/SDL.h>
#include "Map.h"
#include "AnimationManager.h"
class GameObject
{
public:
    struct object {
        int tileWidth = 12;
        int tileHeight = 16;
        float x = 50, y = 100;
        float velx = 0, vely = 0;
        bool onGround = false;
        bool facing = true;
        bool attackKeyPressed = false;
        int attSpeed = 5;
        bool attacking = false;
        float attackTimer = 0;
        bool alive = true;
        float health = 100.0f;
        float maxHealth = 100.0f;
		int damage = 10;
        bool canMove = true;
    }; object obj;

    struct Audio {
		const char* hitSfx = "hit";
		const char* walkSfx = "walk";
		const char* deathSfx = "death";
    } audio;

    AnimationManager* animIdle;
    AnimationManager* animWalk;
    AnimationManager* animAttack;
    AnimationManager* currentAnim;
    AnimationManager* animFlash; // flashing sprite animation
    AnimationManager* animPrev;  // saved animation to restore after flash

public:
    GameObject(SDL_Renderer* renderer, const std::string& spritePath, int tw, int th);

    void draw(SDL_Renderer* renderer, int camX, int camY);
    void update(Map& map);

    SDL_FRect getRect() const;
    virtual SDL_FRect getAttackRect() const;

    // Start the flash pulse sequence (declared so other systems can trigger it)
    void startFlash(int flashes, int intervalTicks = 6);

    // Call when this object takes damage.
    // `attackerX` is used to compute knockback direction (object is pushed away from attacker).
    // `invulnTicks` sets how long the object is immune to further hits (prevents multi-hit in one attack).
    // `kbX`/`kbY` are knockback velocity components; `kbTicks` is how long AI movement is suppressed.
    void takeDamage(float amount, float attackerX, int flashes = 3, int intervalTicks = 6, int invulnTicks = 30, float kbX = 2.5f, float kbY = -4.0f, int kbTicks = 12);

protected:
    // Flashing state
    bool flashing = false; // whether flash sequence active
    bool flashOn = false;        // whether flashing animation is currently active
    int flashInterval = 6;      // ticks per on/off phase
    int flashTicksLeft = 0;     // ticks remaining in current phase
    int flashCyclesLeft = 0;    // number of remaining visible pulses

    // Damage / invulnerability
    int invulnTimer = 0;        // remaining invulnerability ticks

    // Knockback: while >0, AI movement should not override obj.velx
    int knockbackTimer = 0;    // remaining ticks to keep knockback
};