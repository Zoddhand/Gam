#include "Controller.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>

Controller::Controller() : gpHandle(nullptr), prevJump(false), prevAttack(false), prevAttackCharged(false) {}
Controller::~Controller() { close(); }

bool Controller::open(int index) {
    close();
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids || count <= 0) {
        if (ids) SDL_free(ids);
        return false;
    }

    int useIndex = index;
    if (useIndex < 0 || useIndex >= count) useIndex = 0;
    SDL_JoystickID instance_id = ids[useIndex];
    SDL_free(ids);

    SDL_Gamepad* gp = SDL_OpenGamepad(instance_id);
    if (!gp) {
        SDL_Log("SDL_OpenGamepad failed: %s", SDL_GetError());
        return false;
    }

    gpHandle = static_cast<void*>(gp);
    return true;
}

void Controller::close() {
    if (gpHandle) {
        SDL_Gamepad* gp = static_cast<SDL_Gamepad*>(gpHandle);
        SDL_CloseGamepad(gp);
        gpHandle = nullptr;
    }
}

bool Controller::isOpen() const { return gpHandle != nullptr; }

void Controller::update() {
    SDL_Gamepad* gp = static_cast<SDL_Gamepad*>(gpHandle);
    if (!gp) {
        state.connected = false;
        // reset edge flags when disconnected
        state.jump = state.attack = state.attackCharged = false;
        state.jumpPressed = state.attackPressed = state.attackChargedPressed = false;
        prevJump = prevAttack = prevAttackCharged = false;
        return;
    }

    state.connected = true;

    // raw button states
    state.jump = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH) != 0;
    state.attack = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_WEST) != 0;
    // map charged attack to NORTH button (Y on many controllers)
    state.attackCharged = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_NORTH) != 0;

    // compute rising-edge presses
    state.jumpPressed = (state.jump && !prevJump);
    state.attackPressed = (state.attack && !prevAttack);
    state.attackChargedPressed = (state.attackCharged && !prevAttackCharged);

    // update previous raw states
    prevJump = state.jump;
    prevAttack = state.attack;
    prevAttackCharged = state.attackCharged;

    bool dleft = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) != 0;
    bool dright = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) != 0;
    bool dup = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) != 0;
    bool ddown = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) != 0;

    const int DEADZONE = 8000;
    Sint16 ax = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 ay = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);

    bool stickLeft = ax < -DEADZONE;
    bool stickRight = ax > DEADZONE;
    bool stickUp = ay < -DEADZONE;
    bool stickDown = ay > DEADZONE;

    state.left = dleft || stickLeft;
    state.right = dright || stickRight;
    state.up = dup || stickUp;
    state.down = ddown || stickDown;
}

Controller::State Controller::getState() const { return state; }
