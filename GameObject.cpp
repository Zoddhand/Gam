#include "GameObject.h"
#include "Sound.h"
#include "Engine.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include <algorithm>

GameObject::GameObject(SDL_Renderer* renderer, const std::string& spritePath, int tw, int th) {
    SDL_Surface* surf = IMG_Load(spritePath.c_str());
    if (!surf && spritePath != "NULL") {
        SDL_Log("Failed to load sprite: %s", SDL_GetError());
        return;
    }
    tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    // enable alpha blending for sprites
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_DestroySurface(surf);

    // Parameters: texture, frameW, frameH, frames, rowY, innerX, innerY, innerW, innerH
    obj.tileWidth = tw;     
    obj.tileHeight = th;
    animIdle = new AnimationManager(tex, 100, 100, frames.idle, 0, 44, 42, obj.tileWidth, obj.tileHeight);
    animWalk = new AnimationManager(tex, 100, 100, frames.walk, 100, 44, 42, obj.tileWidth, obj.tileHeight);
    animAttack = new AnimationManager(tex, 100, 100, frames.attack, 200, 44, 42, obj.tileWidth, obj.tileHeight);
    animFlash = new AnimationManager(tex, 100, 100, frames.flash, 500, 44, 42, obj.tileWidth, obj.tileHeight);
    animBlock = new AnimationManager(tex, 100, 100, frames.block, 400, 44, 42, obj.tileWidth, obj.tileHeight);
    animShoot = new AnimationManager(tex, 100, 100, frames.shoot, 400, 44, 42, obj.tileWidth, obj.tileHeight);
    animPlayerCharge = new AnimationManager(tex, 100, 100, frames.playerCharge, 400, 44, 42, obj.tileWidth, obj.tileHeight);
    animPrev = animIdle;
    currentAnim = animIdle;

}

// Called by Frames when a frame count value changes; update live AnimationManager instances accordingly
void GameObject::onFrameChanged(int which, int newValue)
{
    using FI = Frames::FrameIndex;
    switch ((FI)which) {
    case FI::IDLE:
        if (animIdle) {
            animIdle->frameCount = newValue;
            if (animIdle->currentFrame >= animIdle->frameCount) animIdle->currentFrame = 0;
        }
        break;
    case FI::WALK:
        if (animWalk) {
            animWalk->frameCount = newValue;
            if (animWalk->currentFrame >= animWalk->frameCount) animWalk->currentFrame = 0;
        }
        break;
    case FI::ATTACK:
        if (animAttack) {
            animAttack->frameCount = newValue;
            if (animAttack->currentFrame >= animAttack->frameCount) animAttack->currentFrame = 0;
        }
        break;
    case FI::FLASH:
        if (animFlash) {
            animFlash->frameCount = newValue;
            if (animFlash->currentFrame >= animFlash->frameCount) animFlash->currentFrame = 0;
        }
        break;
    case FI::BLOCK:
        if (animBlock) {
            animBlock->frameCount = newValue;
            if (animBlock->currentFrame >= animBlock->frameCount) animBlock->currentFrame = 0;
        }
        break;
    case FI::SHOOT:
        if (animShoot) {
            animShoot->frameCount = newValue;
            if (animShoot->currentFrame >= animShoot->frameCount) animShoot->currentFrame = 0;
        }
        break;
    case FI::PLAYERCHARGE:
        if (animPlayerCharge) {
            animPlayerCharge->frameCount = newValue;
            if (animPlayerCharge->currentFrame >= animPlayerCharge->frameCount) animPlayerCharge->currentFrame = 0;
        }
        break;
    default:
        break;
    }
}

SDL_FRect GameObject::getRect() const {
    return { obj.x, obj.y, 12, 16 };
}

SDL_FRect GameObject::getAttackRect() const {
    // Default behavior: provide a hitbox only during strike frames.
    // Delay the hitbox until later in the attack so global hitstop lines up with the visible swing.
    if (!obj.attacking)
        return { 0,0,0,0 };

    // Use a common wind-up rule for all game objects: the strike becomes active when
    // attackTimer has counted down into the final two attSpeed units (~4th frame).
    if (obj.attackTimer > (obj.attSpeed * 2))
        return { 0,0,0,0 };

    float w = 18.0f;
    float h = 12.0f;
    float x = obj.facing ? obj.x + obj.tileWidth : obj.x - w;
    float y = obj.y + 4;

    return { x, y, w, h };
}

void GameObject::draw(SDL_Renderer* renderer, int camX, int camY) {
    if (!obj.alive) return;
        SDL_FRect dst;

        // Scale so inner sprite matches hitbox
        float scaleX = obj.tileWidth / currentAnim->getInnerW();
        float scaleY = obj.tileHeight / currentAnim->getInnerH();

        dst.w = currentAnim->frameWidth * scaleX;   // full 100x100 scaled
        dst.h = currentAnim->frameHeight * scaleY;
        // Round destination position to integer pixels to keep pixels sharp
        float rawX = obj.x - camX - currentAnim->getInnerX() * scaleX;
        float rawY = obj.y - camY - currentAnim->getInnerY() * scaleY;
        dst.x = std::round(rawX);
        dst.y = std::round(rawY) + 1.0f; // shift sprite down 1 pixel so feet align with hitbox

        SDL_FRect src = currentAnim->getSrcRect();
        SDL_FlipMode flip = obj.facing ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;

        // no debug rect here

        // Render the sprite normally
        SDL_RenderTextureRotated(renderer, currentAnim->getTexture(), &src, &dst, 0.0, nullptr, flip);

        SDL_FRect tmpattackRect = this->getAttackRect();
        // Debug draw: convert attack rect (world coordinates) to screen coordinates by subtracting camera.
        // Previously the world rect was passed directly to SDL_RenderRect which made it appear far away.
        if (tmpattackRect.w > 0 && tmpattackRect.h > 0) {
            SDL_FRect screenAttackRect = tmpattackRect;
            screenAttackRect.x -= float(camX);
            screenAttackRect.y -= float(camY);
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 128);
            SDL_RenderRect(renderer, &screenAttackRect); // debug: draw screen rect
        }
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
    // If configured to avoid edges, detect lack of ground ahead and turn around instead of moving
    if (obj.avoidEdges && obj.velx != 0 && knockbackTimer <= 0 && !obj.attacking) {
        float nextX = obj.x + obj.velx;
        int l = int(nextX / map.TILE_SIZE);
        int r = int((nextX + obj.tileWidth - 1) / map.TILE_SIZE);
        int t = int(obj.y / map.TILE_SIZE);
        int b = int((obj.y + obj.tileHeight - 1) / map.TILE_SIZE);

        // Determine front column based on movement direction
        int frontX = obj.velx > 0 ? r : l;
        int footY = b + 1;

        bool blockedByEdge = false;
        // If frontX is outside map -> consider it an edge and flip
        if (frontX < 0 || frontX >= map.width) blockedByEdge = true;
        else {
            // If tile under front is not solid -> it's a ledge; flip
            if (!map.isSolid(frontX, footY)) blockedByEdge = true;

            // Additionally, treat spawn portal tiles as edges so enemies don't step onto transition tiles
            // Portal spawn values: 96 (LEFT), 97 (UP), 98 (RIGHT), 99 (DOWN)
            if (!blockedByEdge && !map.spawn.empty()) {
                if (footY >= 0 && footY < map.height) {
                    int sp = map.spawn[footY * map.width + frontX];
                    if (sp == 96 || sp == 97 || sp == 98 || sp == 99)
                        blockedByEdge = true;
                }
            }
        }

        if (blockedByEdge) {
            obj.facing = !obj.facing;
            obj.velx = 0;
            // prevent AI from immediately overriding this flip and moving back onto the edge
            knockbackTimer = 6; // short pause in AI control
            // skip movement this frame
        } else {
            obj.x += obj.velx;
        }
    } else {
        obj.x += obj.velx;
    }

    int leftTile = int(obj.x / map.TILE_SIZE);
    int rightTile = int((obj.x + obj.tileWidth - 1) / map.TILE_SIZE);
    int topTile = int(obj.y / map.TILE_SIZE);
    int bottomTile = int((obj.y + obj.tileHeight - 1) / map.TILE_SIZE);

    // Horizontal collision
    if (obj.velx > 0) { // moving right
        // Treat one-way platforms as non-blocking for horizontal collisions.
        int colTop = map.getCollision(rightTile, topTile);
        int colBottom = map.getCollision(rightTile, bottomTile);
        auto horSolid = [](int col) -> bool { return (col != -1 && col != Map::COLL_ONEWAY); };
        if (horSolid(colTop) || horSolid(colBottom))
        {
            obj.x = rightTile * map.TILE_SIZE - obj.tileWidth;
            obj.velx = 0.0f;
        }
    }
    else if (obj.velx < 0) { // moving left
        int colTop = map.getCollision(leftTile, topTile);
        int colBottom = map.getCollision(leftTile, bottomTile);
        auto horSolid = [](int col) -> bool { return (col != -1 && col != Map::COLL_ONEWAY); };
        if (horSolid(colTop) || horSolid(colBottom))
        {
            obj.x = (leftTile + 1) * map.TILE_SIZE;
            obj.velx = 0.0f;
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
    if (preventAnimOverride) {
        // Caller requested to preserve currentAnim; just update flip/speed as-is and advance animation.
        currentAnim->setFlip(!obj.facing);
        currentAnim->update();
    }
    else if (flashing && flashOn) {
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

    // After selecting currentAnim, advance its frame when not preserving via preventAnimOverride.
    if (!preventAnimOverride && currentAnim) {
        currentAnim->setFlip(!obj.facing);
        currentAnim->update();
    }

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

    // If this GameObject is marked to avoid edges and somehow ended up standing on a portal
    // (spawn tile 96..99), push it off the portal and flip to avoid getting stuck.
    if (obj.avoidEdges && !map.spawn.empty()) {
        int leftTile = int(obj.x / map.TILE_SIZE);
        int rightTile = int((obj.x + obj.tileWidth - 1) / map.TILE_SIZE);
        int bottomTile = int((obj.y + obj.tileHeight - 1) / map.TILE_SIZE);

        if (bottomTile >= 0 && bottomTile < map.height) {
            // check both tiles under the object's feet
            auto checkAndResolvePortal = [&](int tx) {
                if (tx < 0 || tx >= map.width) return false;
                int sp = map.spawn[bottomTile * map.width + tx];
                if (sp == 96 || sp == 97 || sp == 98 || sp == 99) {
                    // Nudge the object off this tile depending on approach
                    // If object center is left of tile center, push to left; otherwise push to right
                    float center = obj.x + obj.tileWidth * 0.5f;
                    float tileCenter = tx * map.TILE_SIZE + map.TILE_SIZE * 0.5f;
                    if (center < tileCenter) {
                        obj.x = float(tx * map.TILE_SIZE) - obj.tileWidth; // place left
                        obj.facing = true; // face right after nudging
                    } else {
                        obj.x = float((tx + 1) * map.TILE_SIZE);
                        obj.facing = false; // face left after nudging
                    }
                    obj.velx = 0.0f;
                    knockbackTimer = 6;
                    return true;
                }
                return false;
            };

            if (checkAndResolvePortal(leftTile)) {
                // resolved
            } else {
                checkAndResolvePortal(rightTile);
            }
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

    // Trigger a short global hitstop when an object takes damage
    if (gEngine) gEngine->triggerHitstop(6);

    // Only shake the camera if the player itself was hit
    if (gEngine && gEngine->player == this) {
        int dur = std::min(12, std::max(4, int(amount / 3)));
        int mag = std::min(12, std::max(2, int(amount / 5)));
        gEngine->camera.shake(dur, mag);
    }

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
