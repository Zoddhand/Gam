#pragma once
#include "MapObject.h"

class Door : public MapObject {
public:
    Door(int tileX, int tileY, int tileIndex);
    ~Door() override;

    void update(GameObject& obj, Map& map) override;
    void draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const override;

    // mark door opened permanently
    void open(bool playSound = true);

    bool isOpen() const { return opened || unlocked; }

    // Engine must call this to set the level the door belongs to
    void setLevel(int levelID) { level = levelID; }

private:
    bool opened = false; // true when door finished opening and becomes passable
    bool unlocked = false; // true when key consumed and door is passable while anim plays
    bool wasPlayerTouching = false; // simple edge detection to trigger once per touch
    int level = -1; // level id this door belongs to
};