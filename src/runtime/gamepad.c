#include "runtime.h"

#include <stdio.h>
#include <string.h>

const char *bongo_gamepad_axis_name(Uint8 axis) {
    const char *names[] = {"LeftStickX", "LeftStickY", "RightStickX", "RightStickY",
        "LeftTrigger2", "RightTrigger2"};
    return axis < sizeof(names) / sizeof(names[0]) ? names[axis] : "UnknownAxis";
}

const char *bongo_gamepad_button_name(Uint8 button) {
    const char *names[] = {"South", "East", "West", "North", "Select", "Mode",
        "Start", "LeftThumb", "RightThumb", "LeftTrigger", "RightTrigger",
        "DPadUp", "DPadDown", "DPadLeft", "DPadRight", "Misc1", "RightPaddle1",
        "LeftPaddle1", "RightPaddle2", "LeftPaddle2", "Touchpad"};
    return button < sizeof(names) / sizeof(names[0]) ? names[button] : "UnknownButton";
}

void bongo_gamepads_set_enabled(BongoApp *app, bool enabled) {
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; ids && i < count; ++i) {
        SDL_Gamepad *gamepad = SDL_GetGamepadFromID(ids[i]);
        if (enabled && !gamepad) SDL_OpenGamepad(ids[i]);
        else if (!enabled && gamepad) SDL_CloseGamepad(gamepad);
    }
    SDL_free(ids);
    if (!enabled) bongo_app_reset_gamepad(app);
}

void bongo_gamepad_event(BongoApp *app, const void *raw) {
    const SDL_Event *event = raw;
    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        if (app->config.current_mode == BONGO_MODE_GAMEPAD)
            SDL_OpenGamepad(event->gdevice.which);
        return;
    }
    if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event->gdevice.which);
        if (gamepad) SDL_CloseGamepad(gamepad);
        bongo_app_reset_gamepad(app);
        return;
    }
    if (app->config.current_mode != BONGO_MODE_GAMEPAD) return;
    BongoInputEvent input = {0};
    if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        input.kind = BONGO_INPUT_GAMEPAD_AXIS;
        input.value = event->gaxis.value < 0
            ? event->gaxis.value / 32768.0f : event->gaxis.value / 32767.0f;
        snprintf(input.name, sizeof(input.name), "%s", bongo_gamepad_axis_name(event->gaxis.axis));
    } else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
        event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        input.kind = BONGO_INPUT_GAMEPAD_BUTTON;
        input.value = event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? 1.0f : 0.0f;
        snprintf(input.name, sizeof(input.name), "%s", bongo_gamepad_button_name(event->gbutton.button));
    } else return;
    input.timestamp_ms = SDL_GetTicks();
    bongo_app_apply_input(app, &input);
}
