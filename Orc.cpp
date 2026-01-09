#include "Orc.h"
#include <cmath>
#include "Sound.h"

Orc::Orc(SDL_Renderer* renderer,
    const std::string& spritePath,
    int tw,
    int th,
    float startX,
    float startY,
    int dam,
    bool block)
    : GameObject(renderer, spritePath, tw, th)
{
    obj.x = startX;
    obj.y = startY;

    obj.facing = true;
    obj.velx = 1.0f;   // patrol speed
    obj.attSpeed = 5;
    obj.damage = dam;
	audio.hitSfx = "orc_hit";
	audio.deathSfx = "orc_death";
	canBlock = block;
}

void Orc::aiUpdate(Player& player, Map& map)
{
    if (!obj.alive) return;
    if (knockbackTimer > 0) {
        GameObject::update(map);
        // If knockback just ended during GameObject::update, orient to face the player
        if (knockbackTimer <= 0) {
            obj.facing = (player.obj.x > obj.x);
        }
        return;
    }

    float dist = player.obj.x - obj.x;
    bool sameLevel = std::abs(player.obj.y - obj.y) < obj.tileHeight;
    bool playerInFront = (dist > 0 && obj.facing) || (dist < 0 && !obj.facing);
    bool playerClose = std::abs(dist) <= 24;

    // --------------------
    // Start attack
    // --------------------
    if (sameLevel && playerClose && playerInFront && !obj.attacking && knockbackTimer <= 0 && !blocking) {
        {
            obj.attacking = true;
            obj.attackTimer = obj.attSpeed * 5;
            obj.velx = 0;
            currentAnim = animAttack;
            currentAnim->reset();
        }
    }

    // --------------------
    // Movement intent
    // --------------------
    if (!obj.attacking && knockbackTimer <= 0 && !blocking) {
        if (sameLevel && std::abs(dist) < 80 && playerInFront) {
            obj.velx = (dist < 0) ? -1.0f : 1.0f;   
            obj.facing = obj.velx > 0;
        }
        else {
            obj.velx = obj.facing ? 1.0f : -1.0f;
        }
    }
    else if (!obj.attacking && knockbackTimer > 0) {
        // While knocked back, keep facing consistent with velocity
        obj.facing = obj.velx > 0;
    }
    else {
        if (!blocking) obj.velx = 0;
    }

    // --------------------
    // WALL + LEDGE CHECK (AI only)
    //   Do not let AI override knockback movement.
    // --------------------
    if (!obj.attacking && knockbackTimer <= 0 && !blocking) {
        float nextX = obj.x + obj.velx;

        int l = int(nextX / map.TILE_SIZE);
        int r = int((nextX + obj.tileWidth - 1) / map.TILE_SIZE);
        int t = int(obj.y / map.TILE_SIZE);
        int b = int((obj.y + obj.tileHeight - 1) / map.TILE_SIZE);

        bool blocked = false;

        // Wall
        if (obj.velx > 0 && (map.isSolid(r, t) || map.isSolid(r, b)))
            blocked = true;
        if (obj.velx < 0 && (map.isSolid(l, t) || map.isSolid(l, b)))
            blocked = true;

        // Ledge (tile in front & below)
        int frontX = obj.velx > 0 ? r : l;
        int footY = b + 1;
        if (!map.isSolid(frontX, footY))
            blocked = true;

        if (blocked) {
            obj.facing = !obj.facing;
            obj.velx = 0; // let next frame pick direction
        }
    }


    // --------------------
    // Apply physics + animation
    // --------------------
    GameObject::update(map);
    // --------------------
    // Die if hit / Block if attacked from front
    // --------------------
    if (player.obj.attacking) {
        SDL_FRect atk = player.getAttackRect();
        SDL_FRect me = getRect();

        if (SDL_HasRectIntersectionFloat(&atk, &me)) {
            float playerCenter = player.obj.x + player.obj.tileWidth * 0.5f;
            float orcCenter = obj.x + obj.tileWidth * 0.5f;
            bool attackFromFront = (playerCenter > orcCenter && obj.facing) || (playerCenter < orcCenter && !obj.facing);

            if (attackFromFront && canBlock) {
                // Block the attack: enter blocking state and avoid taking damage
			obj.velx = 0;
                blocking = true;
                blockTimer = BLOCK_DURATION;
                // cancel any in-progress attack so hitbox/animation won't overlap
                obj.attacking = false;
                obj.attackTimer = 0;
                // stop horizontal movement immediately
                obj.velx = 0;
                if (animBlock) { currentAnim = animBlock; currentAnim->reset(); }
                // optional: play block sfx if available
                if (gSound) gSound->playSfx("hit", 128); // small feedback
			player.obj.x -= (player.obj.facing) ? -0.1f : 0.1f; // slight pull to player
			player.obj.attackTimer -= 1.0f; // slight delay to player's attack
			gSound->playSfx("clang");
            } else {
                // Attacked from behind or side -> take damage normally
                float playerCenter = player.obj.x + player.obj.tileWidth * 0.5f;
                takeDamage(35.0f, playerCenter, 3, 6, 30, 2.5f, -4.0f);
                obj.facing = player.obj.facing;
            }
        }
    }
    else if (obj.attacking) {
        SDL_FRect atk = getAttackRect();
        SDL_FRect me = player.getRect();

        if (SDL_HasRectIntersectionFloat(&atk, &me)) {
            float orcCenter = obj.x + obj.tileWidth * 0.5f;
            player.takeDamage(obj.damage, orcCenter, 3, 6, 30, 2.5f, -4.0f);
        }
    }

    // --------------------
    // Blocking timer decrement
    // --------------------
    if (blockTimer > 0) {
        --blockTimer;
        if (blockTimer <= 0) {
            blocking = false;
        }
    }
}

SDL_FRect Orc::getAttackRect() const
{
    if (!obj.attacking)
        return { 0,0,0,0 };

    // Wind-up: first half of attack has NO hitbox
    if (obj.attackTimer > (obj.attSpeed * 3))
        return { 0,0,0,0 };

    // Strike frames
    float w = obj.tileWidth;
    float h = 8;
    float x = obj.facing ? obj.x + obj.tileWidth : obj.x - w;
    float y = obj.y + 4;

    return { x, y, w, h };
}
