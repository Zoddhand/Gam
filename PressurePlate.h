#pragma once
#include "MapObject.h"

class PressurePlate : public MapObject {
public:
    PressurePlate(int tileX, int tileY, int tileIndex);
    ~PressurePlate();

    void update(GameObject& obj, Map& map) override;
    void draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const override;

    bool isTriggered() const { return triggered; }

    enum class TargetAction {
        NONE,
        FIRE_ARROWTRAPS,
        OPEN_DOOR,
        DROP_FALLINGTRAP
    };

    void setTargetAction(TargetAction a, int tx = -1, int ty = -1) { targetAction = a; targetTx = tx; targetTy = ty; }
    TargetAction getTargetAction() const { return targetAction; }

private:
    bool triggered = false;
    bool prevTriggered = false;
    TargetAction targetAction = TargetAction::NONE;
    int targetTx = -1;
    int targetTy = -1;
    int animTimer = 0;
    int animInterval = 12;
};
