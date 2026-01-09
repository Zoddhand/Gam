#include "GameObject.h"
#include "Sound.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "Sound.h"

GameObject::GameObject(SDL_Renderer* renderer, const std::string& spritePath, int tw, int th) {
    SDL_Surface* surf = IMG_Load(spritePath.c_str());
    if (!surf && spritePath != "NULL") {
        SDL_Log("Failed to load sprite: %s", SDL_GetError());
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(surf);

    // Parameters: texture, frameW, frameH, frames, rowY, innerX, innerY, innerW, innerH
    obj.tileWidth = tw;     
    obj.tileHeight = th;
    animIdle = new AnimationManager(tex, 100, 100, 6, 0, 44, 42, obj.tileWidth, obj.tileHeight);
    animWalk = new AnimationManager(tex, 100, 100, 8, 100, 44, 42, obj.tileWidth, obj.tileHeight);
    animAttack = new AnimationManager(tex, 100, 100, 9, 200, 44, 42, obj.tileWidth, obj.tileHeight);
    animFlash = new AnimationManager(tex, 100, 100, 4, 500, 44, 42, obj.tileWidth, obj.tileHeight);
    animBlock = new AnimationManager(tex, 100, 100, 4, 400, 44, 42, obj.tileWidth, obj.tileHeight);
    animShoot = new AnimationManager(tex, 100, 100, 9, 400, 44, 42, obj.tileWidth, obj.tileHeight);
    animPrev = animIdle;
    currentAnim = animIdle;

}

SDL_FRect GameObject::getRect() const {
    return { obj.x, obj.y, 12, 16 };
}

SDL_FRect GameObject::getAttackRect() const {
    return { obj.facing ? obj.x + 12 : obj.x - 12, obj.y + 4, 12, 8 };
}

void GameObject::draw(SDL_Renderer* renderer, int camX, int camY) {
    if (!obj.alive) return;
        SDL_FRect dst;

        // Scale so inner sprite matches hitbox
        float scaleX = obj.tileWidth / currentAnim->getInnerW();
        float scaleY = obj.tileHeight / currentAnim->getInnerH();

        dst.w = currentAnim->frameWidth * scaleX;   // full 100x100 scaled
        dst.h = currentAnim->frameHeight * scaleY;
        dst.x = obj.x - camX - currentAnim->getInnerX() * scaleX;
        dst.y = obj.y - camY - currentAnim->getInnerY() * scaleY;

        SDL_FRect src = currentAnim->getSrcRect();
        SDL_FlipMode flip = obj.facing ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;

        // Render the sprite normally
        SDL_RenderTextureRotated(renderer, currentAnim->getTexture(), &src, &dst, 0.0, nullptr, flip);

        // flashing overlay removed; animation swap will show flashing sprite when active
    }

void GameObject::update(Map& map)
{
    // --------------------
    // Gravity
    // --------------------
    obj.vely += 0.35f;
    if (obj.vely > 8.0f)
        obj.vely = 8.0f;

    // --------------------
    // Horizontal movement
    // --------------------
    obj.x += obj.velx;

    int leftTile = int(obj.x / map.TILE_SIZE);
    int rightTile = int((obj.x + obj.tileWidth - 1) / map.TILE_SIZE);
    int topTile = int(obj.y / map.TILE_SIZE);
    int bottomTile = int((obj.y + obj.tileHeight - 1) / map.TILE_SIZE);

    // Horizontal collision
    if (obj.velx > 0) { // moving right
        if (map.isSolid(rightTile, topTile) ||
            map.isSolid(rightTile, bottomTile))
        {
            obj.x = rightTile * map.TILE_SIZE - obj.tileWidth;
        }
    }
    else if (obj.velx < 0) { // moving left
        if (map.isSolid(leftTile, topTile) ||
            map.isSolid(leftTile, bottomTile))
        {
            obj.x = (leftTile + 1) * map.TILE_SIZE;
        }
    }

    // --------------------
    // Vertical movement
    // --------------------
    // Save previous Y so we can detect coming-from-above for one-way platforms
    float prevY = obj.y;

    obj.y += obj.vely;

    leftTile = int(obj.x / map.TILE_SIZE);
    rightTile = int((obj.x + obj.tileWidth - 1) / map.TILE_SIZE);
    topTile = int(obj.y / map.TILE_SIZE);
    bottomTile = int((obj.y + obj.tileHeight - 1) / map.TILE_SIZE);

    // Vertical collision
    if (obj.vely > 0) { // falling
        // Check both bottom tiles
        int colLeft = map.getCollision(leftTile, bottomTile);
        int colRight = map.getCollision(rightTile, bottomTile);

        auto shouldStopOn = [&](int col, int tileY) -> bool {
            if (col == -1) return false;
            if (col == Map::COLL_ONEWAY) {
                // Only stop if we were above the platform in the previous frame
                float platformTop = float(bottomTile * map.TILE_SIZE);
                float prevBottom = prevY + obj.tileHeight - 1;
                if (prevBottom <= platformTop && obj.ignoreOneWayTimer <= 0)
                    return true;
                return false;
            }
            // Any other non -1 collision is solid
            return true;
        };

        if (shouldStopOn(colLeft, bottomTile) || shouldStopOn(colRight, bottomTile))
        {
            obj.y = bottomTile * map.TILE_SIZE - obj.tileHeight;
            obj.vely = 0.0f;
            obj.onGround = true;
        } else {
            obj.onGround = false;
                }
    }
    else if (obj.vely < 0) { // jumping / hitting ceiling
        // One-way platforms should NOT block you when going up; treat ONLY non-one-way as solid
        int colLeftTop = map.getCollision(leftTile, topTile);
        int colRightTop = map.getCollision(rightTile, topTile);

        auto isSolidCeil = [&](int col) -> bool {
            if (col == -1) return false;
            if (col == Map::COLL_ONEWAY) return false; // allow passing up through one-way
            return true;
        };

        if (isSolidCeil(colLeftTop) || isSolidCeil(colRightTop))
        {
            obj.y = (topTile + 1) * map.TILE_SIZE;
            obj.vely = 0.0f;
        }
    }

    // --------------------
    // Attack timing
    // --------------------
    if (obj.attacking && --obj.attackTimer <= 0)
        obj.attacking = false;

    // --------------------
    // Animation selection
    // --------------------
    if (flashing && flashOn) {
        // swap to flashing animation (save previous)
        if (currentAnim != animFlash) {
            animPrev = currentAnim;
            currentAnim = animFlash;
        }
    } else {
        // not currently showing flash animation
        if (animPrev && currentAnim == animFlash) {
            currentAnim = animPrev;
            animPrev = nullptr;
        }

        // New: blocking animation takes precedence
        if (blocking && animBlock) {
            currentAnim = animBlock;
            currentAnim->setSpeed(10);
        }
        else if (obj.attacking) {
            // If a ranged "shoot" animation exists and was explicitly selected by the caller
            // (currentAnim == animShoot), preserve it. Otherwise fall back to the melee attack anim.
            if (animShoot && currentAnim == animShoot) {
                currentAnim->setSpeed(obj.attSpeed);
            } else {
                currentAnim = animAttack;
                currentAnim->setSpeed(obj.attSpeed);
            }
        }
        else if (obj.velx != 0) {
            currentAnim = animWalk;
            currentAnim->setSpeed(10);
        }
        else {
            currentAnim = animIdle;
            currentAnim->setSpeed(10);
        }
    }

    currentAnim->setFlip(!obj.facing);
    currentAnim->update();

    // --------------------
    // Flashing update (swap animation pulses)
    // --------------------
    if (flashing) {
        if (--flashTicksLeft <= 0) {
            if (flashOn) {
                // turn flash animation off for the off-phase
                flashOn = false;
                flashTicksLeft = flashInterval;
            } else {
                // completed an off-phase -> one visible pulse consumed
                --flashCyclesLeft;
                if (flashCyclesLeft <= 0) {
                    // finished all pulses
                    flashing = false;
                    // ensure we restore previous animation next frame
                    if (animPrev && currentAnim == animFlash) {
                        currentAnim = animPrev;
                        animPrev = nullptr;
                    }
                } else {
                    // start next visible phase
                    flashOn = true;
                    flashTicksLeft = flashInterval;
                }
            }
        }
    }

    // --------------------
    // Invulnerability timer (prevents multiple hits per attack)
    // --------------------
    if (invulnTimer > 0)
        --invulnTimer;

    // --------------------
    // Knockback timer (prevents AI from overriding knockback)
    // --------------------
    if (knockbackTimer > 0)
        --knockbackTimer;

    // --------------------
    // Decrement drop-through timer (if set by player)
    // --------------------
    if (obj.ignoreOneWayTimer > 0)
        --obj.ignoreOneWayTimer;

    // --------------------
    // Clamp to map bounds so objects/players cannot fall out of the world
    // --------------------
    if (map.width > 0 && map.height > 0) {
        int worldW = map.width * map.TILE_SIZE;
        int worldH = map.height * map.TILE_SIZE;

        if (obj.x < 0.0f) obj.x = 0.0f;
        if (obj.x + obj.tileWidth > worldW) obj.x = float(worldW - obj.tileWidth);

        if (obj.y < 0.0f) {
            obj.y = 0.0f;
            obj.vely = 0.0f;
        }
        if (obj.y + obj.tileHeight > worldH) {
            obj.y = float(worldH - obj.tileHeight);
            obj.vely = 0.0f;
            obj.onGround = true;
        }
    }
}

void GameObject::startFlash(int flashes, int intervalTicks) {
    if (flashes <= 0) return;
    flashing = true;
    flashOn = true;
    flashInterval = intervalTicks > 0 ? intervalTicks : 6;
    flashTicksLeft = flashInterval;
    flashCyclesLeft = flashes;
}

void GameObject::takeDamage(
    float amount,
    float attackerX,
    int flashes,
    int intervalTicks,
    int invulnTicks,
    float kbX,
    float kbY,
    int kbTicks)
{
    // --------------------
    // Invulnerability check
    // --------------------
    if (invulnTimer > 0)
        return;

    // --------------------
    // Apply damage
    // --------------------
    obj.health -= amount;
    if (obj.health <= 0.0f) {
        obj.health = 0.0f;
        if (gSound) gSound->playSfx(audio.deathSfx);
        obj.alive = false;
    }

    // --------------------
    // Visual feedback
    // --------------------
    startFlash(flashes, intervalTicks);

    // Play hit sound
    if (gSound) gSound->playSfx(audio.hitSfx);

    // --------------------
    // Invulnerability timer
    // --------------------
    invulnTimer = (invulnTicks > 0) ? invulnTicks : 30;

    // --------------------
    // Horizontal knockback
    // Push away from attacker
    // --------------------
    float magX = fabs(kbX);

    float myCenter = obj.x + obj.tileWidth * 0.5f;

    // If I'm left of the attacker → go left
    // If I'm right of the attacker → go right
    float sign = (myCenter < attackerX) ? -1.0f : 1.0f;

    obj.velx = sign * magX;

    // Safety clamp so knockback is never zero
    if (fabs(obj.velx) < 0.1f)
        obj.velx = sign * magX;

    // --------------------
    // Vertical knockback
    // Small lift only if grounded
    // --------------------
    if (obj.onGround && kbY != 0.0f) {
        obj.vely = kbY * 0.5f;   // tune this multiplier as needed
        obj.onGround = false;
    }

    // --------------------
    // Prevent AI/input from overriding knockback
    // --------------------
    knockbackTimer = (kbTicks > 0) ? kbTicks : 12;
}
