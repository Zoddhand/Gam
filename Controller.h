#pragma once
#include <SDL3/SDL.h>

class Controller {
public:
    struct State {
        bool left = false;
        bool right = false;
        bool up = false;
        bool down = false;
        bool jump = false;        // current raw button state
        bool attack = false;      // current raw button state
        bool attackCharged = false;      // current raw button state
        bool jumpPressed = false; // rising-edge (pressed this frame)
        bool attackPressed = false; // rising-edge
        bool attackChargedPressed = false; // rising-edge for charged attack
        bool connected = false;
    };

    Controller();
    ~Controller();

    bool open(int index = 0);
    void close();
    bool isOpen() const;

    void update();
    State getState() const;

private:
    void* gpHandle = nullptr;
    State state;

    // previous raw button states used to detect edges
    bool prevJump = false;
    bool prevAttack = false;
    bool prevAttackCharged = false;
};
