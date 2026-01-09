#include "Archer.h"
#include "Arrow.h"
#include "Sound.h"
#include <cmath>

Archer::Archer(SDL_Renderer* renderer,
    const std::string& spritePath,
    int tw,
    int th,
    float startX,
    float startY,
    int dam)
    : GameObject(renderer, spritePath, tw, th), rendererPtr(renderer)
{
    obj.x = startX;
    obj.y = startY;
    obj.damage = dam;
    obj.attSpeed = 5;
    // start patrolling to the right by default
    obj.facing = true;
    obj.velx = 1.0f;
    audio.hitSfx = "hit";
    audio.deathSfx = "death";
    shotFired = false;
}

void Archer::aiUpdate(Player& player, Map& map, std::vector<Arrow*>& projectiles)
{
    if (!obj.alive) return;
    if (knockbackTimer > 0) { GameObject::update(map); return; }

    float dx = (player.obj.x) - obj.x;
    float dy = (player.obj.y) - obj.y;
    float dist = std::sqrt(dx*dx + dy*dy);

    bool sameLevel = std::abs(player.obj.y - obj.y) < obj.tileHeight;
    bool playerInFront = (dx > 0 && obj.facing) || (dx < 0 && !obj.facing);

    // Only attempt to start shooting if within range, on same level, cooldown expired and not already in attack
    if (sameLevel && dist <= range && shootCooldown <= 0 && !obj.attacking)
    {
        // face the player when starting to shoot
        obj.facing = dx > 0;

        // start shooting animation and set attack timing
        if (animShoot) {
            currentAnim = animShoot;
            // ensure animation plays at desired speed
            currentAnim->setSpeed(obj.attSpeed);
            currentAnim->reset();
            
            // make total attack duration cover full animation
            obj.attackTimer = currentAnim->frameCount * currentAnim->speed;
        } else {
            // fallback: use melee attack timing
            obj.attackTimer = obj.attSpeed * 5;
            currentAnim = animAttack;
            if (currentAnim) currentAnim->reset();
        }

        obj.attacking = true;
        shotFired = false;
        if (gSound) gSound->playSfx("monster");
        // do not spawn arrow yet — wait until animation frame 8
    }

    if (shootCooldown > 0) --shootCooldown;

    // --------------------
    // Movement intent (patrol / chase) - similar to Orc
    // --------------------
    if (!obj.attacking && knockbackTimer <= 0) {
        // If player is roughly on same level and close in front, move toward them
        if (sameLevel && std::abs(dx) < 80 && playerInFront) {
            obj.velx = (dx < 0) ? -1.0f : 1.0f;
            obj.facing = obj.velx > 0;
        }
        else {
            // regular patrol
            obj.velx = obj.facing ? 1.0f : -1.0f;
        }
    } else if (!obj.attacking && knockbackTimer > 0) {
        obj.facing = obj.velx > 0;
    } else {
        // attacking: stop horizontal movement
        obj.velx = 0;
    }

    // --------------------
    // WALL + LEDGE CHECK (AI only) - prevent walking off ledges
    // --------------------
    if (!obj.attacking && knockbackTimer <= 0) {
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

    // Apply physics + animation (this advances the animation frame)
    GameObject::update(map);

    // While attacking and animShoot is active, check animation frame to spawn arrow on frame 8 (0-based index 7)
    if (obj.attacking && currentAnim == animShoot && animShoot) {
        int frame = animShoot->currentFrame; // read after update so frame reflects current visible frame
        // 9 frames, index 0..8 -> fire on frame index 7 (8th frame)
        if (frame == 7 && !shotFired) {
            // recompute dx/dy to aim at current player position
            float pdx = (player.obj.x) - obj.x;
            float pdy = (player.obj.y) - obj.y;
            float pdist = std::sqrt(pdx*pdx + pdy*pdy);

            // create arrow velocity towards player with fixed speed
            float speed = 4.0f;
            float nx = pdx / (pdist + 0.0001f);
            float ny = pdy / (pdist + 0.0001f);
            float vx = nx * speed;
            float vy = ny * speed;

            // spawn arrow slightly in front of archer
            float sx = obj.x + (obj.facing ? obj.tileWidth : -8);
            float sy = obj.y + obj.tileHeight * 0.5f;

            Arrow* a = new Arrow(rendererPtr, sx, sy, vx, vy);
            projectiles.push_back(a);
            shotFired = true;
        }
    }

    // Take damage from player's melee attack (same logic as Orc but no blocking)
    if (player.obj.attacking) {
        SDL_FRect atk = player.getAttackRect();
        SDL_FRect me = getRect();

        if (SDL_HasRectIntersectionFloat(&atk, &me)) {
            float playerCenter = player.obj.x + player.obj.tileWidth * 0.5f;
            takeDamage(35.0f, playerCenter, 3, 6, 30, 2.5f, -4.0f);
        }
    }

    // When attack concludes, reset shotFired and start cooldown
    if (!obj.attacking && shotFired) {
        shotFired = false;
        shootCooldown = SHOOT_COOLDOWN_MAX;
    }
}
