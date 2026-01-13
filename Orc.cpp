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
    obj.velx = patrolSpeed;   // patrol speed
    obj.avoidEdges = true;
    obj.attSpeed = 4;
    obj.damage = dam;
	audio.hitSfx = "orc_hit";
	audio.deathSfx = "orc_death";
	canBlock = block;

    // initialize random pause cooldown so they don't all pause at once
    // Start in a paused state so newly spawned orcs may begin idle
    paused = true;
    pauseTimer = rand() % (PAUSE_MAX - PAUSE_MIN + 1) + PAUSE_MIN;
    obj.velx = 0; // don't move while paused
    // set cooldown that will be used after this initial pause
    pauseCooldown = rand() % (COOLDOWN_MAX - COOLDOWN_MIN + 1) + COOLDOWN_MIN;
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

    // Use the orc's attack hitbox width as the attack-start distance
    float attackRange = float(obj.tileWidth) * 2.0f;
    bool playerClose = std::abs(dist) <= attackRange;

    // Determine whether the orc can "see" the player for aggression purposes
    bool playerNearbyForSight = sameLevel && std::abs(dist) < 80 && playerInFront;
    if (playerNearbyForSight) {
        // First time seeing the player -> enter aggressive mode
        if (!hasSeenPlayer) {
            hasSeenPlayer = true;
            aggressiveMode = true;
            aggressiveTimer = AGGRESSIVE_DURATION;
        }
    }

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
            // Refresh aggressive timer when engaging the player
            hasSeenPlayer = true;
            aggressiveMode = true;
            aggressiveTimer = AGGRESSIVE_DURATION;
        }
    }

    // --------------------
    // Random pause (idle) behaviour
    // --------------------
    if (!obj.attacking && knockbackTimer <= 0 && !blocking) {
        if (pauseCooldown > 0) --pauseCooldown;

        if (paused) {
            // If player is close, interrupt pause and resume chasing
            if (sameLevel && std::abs(dist) < 80) {
                paused = false;
                obj.facing = (dist > 0);
                // start moving using chase speed when resuming to chase
                obj.velx = obj.facing ? chaseSpeed : -chaseSpeed;
                // mark aggressive
                hasSeenPlayer = true;
                aggressiveMode = true;
                aggressiveTimer = AGGRESSIVE_DURATION;
            }
            else {
                if (pauseTimer > 0) --pauseTimer;
                obj.velx = 0;
                if (pauseTimer <= 0) {
                    paused = false;
                    // Choose a random direction to resume moving
                    bool faceRight = (rand() % 2) == 0;
                    obj.facing = faceRight;
                    // if currently in aggressive mode use chaseSpeed, otherwise patrolSpeed
                    float useSpeed = aggressiveMode ? chaseSpeed : patrolSpeed;
                    obj.velx = obj.facing ? useSpeed : -useSpeed;
                }
            }
        }
        else {
            if (pauseCooldown <= 0) {
                // small per-frame chance to enter a pause
                if ((rand() % 100) < 3) {
                    // Only enter a pause if player is not nearby
                    if (!(sameLevel && std::abs(dist) < 80)) {
                        paused = true;
                        pauseTimer = rand() % (PAUSE_MAX - PAUSE_MIN + 1) + PAUSE_MIN;
                        pauseCooldown = rand() % (COOLDOWN_MAX - COOLDOWN_MIN + 1) + COOLDOWN_MIN;
                        obj.velx = 0;
                    } else {
                        // player is too close; delay next pause attempt
                        pauseCooldown = COOLDOWN_MIN;
                    }
                }
            }
        }
    }

    // --------------------
    // Movement intent
    // --------------------
    if (!obj.attacking && knockbackTimer <= 0 && !blocking) {
        if (!paused) {
            if (sameLevel && std::abs(dist) < 80 && playerInFront) {
                // actively chase player using chaseSpeed
                obj.velx = (dist < 0) ? -chaseSpeed : chaseSpeed;
                obj.facing = obj.velx > 0;
                // entering chase refreshes aggressive timer
                hasSeenPlayer = true;
                aggressiveMode = true;
                aggressiveTimer = AGGRESSIVE_DURATION;
            }
            else {
                // Patrol: use patrolSpeed unless in aggressive mode where we use chaseSpeed
                float useSpeed = aggressiveMode ? chaseSpeed : patrolSpeed;
                obj.velx = obj.facing ? useSpeed : -useSpeed;
            }
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

    // decrement aggressive timer if active
    if (aggressiveMode && aggressiveTimer > 0) {
        --aggressiveTimer;
        if (aggressiveTimer <= 0) {
            aggressiveMode = false;
        }
    }

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
    float w = obj.tileWidth * 2;
    float h = 8;
    float x = obj.facing ? obj.x + obj.tileWidth : obj.x - w;
    float y = obj.y + 4;

    return { x, y, w, h };
}
