#pragma once
#include "GameObject.h"

// Controller lives in Player.h as requested
class Controller {
public:
    struct State {
        bool left = false;
        bool right = false;
        bool up = false;
        bool down = false;
        bool jump = false;
        bool attack = false;
        bool connected = false;
    };

    Controller();
    ~Controller();

    // Open the controller by index (0 = first)
    bool open(int index = 0);
    void close();
    bool isOpen() const;

    // Poll current controller state into `state`
    void update();
    State getState() const;

private:
    void* gpHandle = nullptr; // opaque pointer to SDL gamepad/controller object
    State state;
};

class Player : public GameObject {
public:
    Player(SDL_Renderer* renderer,
        const std::string& spritePath,
        int tw,
        int th);
    int jumpToken;
    void input(const bool* keys);

    // Controller-driven input overload (declared)
    void input(const Controller::State& cs);
};