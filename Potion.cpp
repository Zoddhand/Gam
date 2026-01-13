#include "Potion.h"
#include "Player.h"
#include "Engine.h"
#include "Sound.h"

Potion::Potion(SDL_Renderer* renderer, const std::string& spritePath, int tw, int th, float startX, float startY, Type t)
    : GameObject(renderer, spritePath, tw, th), type(t)
{
    obj.x = startX;
    obj.y = startY;
    obj.tileWidth = tw;
    obj.tileHeight = th;
    // Potions are small pickups: they should fall under gravity
    obj.velx = 0;
    obj.vely = 0;
    // Replace animations (GameObject sets up AnimationManager for sprite sheets). Potions use a simple 16x16 image.
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }

    // initialize blink
    blinkTimer = blinkInterval;
    blinkVisible = true;
}

void Potion::update(Map& map)
{
    if (!obj.alive) return;

    // basic gravity + ground collision handled by GameObject::update
    GameObject::update(map);

    // Blink update
    if (--blinkTimer <= 0) {
        blinkVisible = !blinkVisible;
        blinkTimer = blinkInterval;
    }

    // If potion falls out of world, mark dead
    if (obj.y > map.height * map.TILE_SIZE) obj.alive = false;

    // Check pickup by player
    if (gEngine && gEngine->player && gEngine->player->obj.alive) {
        SDL_FRect pr = gEngine->player->getRect();
        SDL_FRect my = getRect();
        if (SDL_HasRectIntersectionFloat(&pr, &my)) {
            onPickup(gEngine->player);
            obj.alive = false;
        }
    }
}

SDL_FRect Potion::getRect() const
{
    return { obj.x, obj.y, (float)obj.tileWidth, (float)obj.tileHeight };
}

void Potion::draw(SDL_Renderer* renderer, int camX, int camY)
{
    if (!obj.alive) return;
    if (!tex) return;

    // If currently blinking invisible, skip draw
    if (!blinkVisible) return;

    SDL_FRect src{ 0.0f, 0.0f, (float)obj.tileWidth, (float)obj.tileHeight };
    SDL_FRect dst{ obj.x - camX, obj.y - camY, (float)obj.tileWidth, (float)obj.tileHeight };

    SDL_RenderTexture(renderer, tex, &src, &dst);
}

void Potion::onPickup(Player* player)
{
    if (!player) return;

    const float healPct = 0.20f; // restore 20%
    if (type == Type::HEALTH) {
        float amount = player->obj.maxHealth * healPct;
        player->obj.health = std::min(player->obj.maxHealth, player->obj.health + amount);
        if (gSound) gSound->playSfx("arrow", 128);
    } else {
        float amount = player->obj.maxMagic * healPct;
        player->obj.magic = std::min(player->obj.maxMagic, player->obj.magic + amount);
        if (gSound) gSound->playSfx("arrow", 128);
    }
}
