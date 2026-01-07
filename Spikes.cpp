#include "Spikes.h"
#include "Player.h"
#include <SDL3/SDL.h>

Spikes::Spikes(int tileX, int tileY, int tileIndex)
    : MapObject(tileX, tileY, tileIndex)
{
}

Spikes::~Spikes() = default;

void Spikes::update(GameObject& obj, Map& /*map*/)
{
    if (!active) return;
	SDL_FRect pr = { obj.getRect().x + 4,obj.getRect().y + 4,obj.getRect().w / 2,obj.getRect().h / 2 };
    SDL_FRect tr = getRect();
    if (SDL_HasRectIntersectionFloat(&pr, &tr)) {
        // default behaviour: instant kill; change to damage if you add health
        obj.obj.alive = false;
        //active = false;
    }
}