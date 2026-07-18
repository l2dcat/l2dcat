#include "l2dcat/app.h"
#include "l2dcat/overlay.h"

#include <math.h>
#include <string.h>

static bool stick_active(float x, float y, bool pressed) {
    return pressed || fabsf(x) > 0.001f || fabsf(y) > 0.001f;
}

static void update_hands(L2DCatApp *app) {
    bool left_stick = stick_active(app->left_stick_x,
        app->left_stick_y, app->left_stick_pressed);
    bool right_stick = stick_active(app->right_stick_x,
        app->right_stick_y, app->right_stick_pressed);
    l2dcat_live2d_set_parameter(app->live2d, "CatParamStickShowLeftHand", left_stick);
    l2dcat_live2d_set_parameter(app->live2d, "CatParamStickShowRightHand", right_stick);
    l2dcat_live2d_set_parameter(app->live2d, "CatParamLeftHandDown",
        left_stick || l2dcat_overlay_hand_active(app->overlay, false));
    l2dcat_live2d_set_parameter(app->live2d, "CatParamRightHandDown",
        right_stick || l2dcat_overlay_hand_active(app->overlay, true));
}

static void apply_key(L2DCatApp *app, const char *name, bool pressed) {
    if (l2dcat_overlay_key(app->overlay, name, pressed) < 0) return;
    update_hands(app);
    app->dirty = true;
}

static void set_axis(L2DCatApp *app, const char *id, float input) {
    L2DCatParameterRange range;
    if (!l2dcat_live2d_parameter(app->live2d, id, &range)) return;
    float value = input * range.maximum;
    if (value < range.minimum) value = range.minimum;
    if (value > range.maximum) value = range.maximum;
    l2dcat_live2d_set_parameter(app->live2d, id, value);
}

static void apply_gamepad(L2DCatApp *app, const L2DCatInputEvent *event) {
    const char *id = NULL;
    if (strcmp(event->name, "LeftStickX") == 0) {
        id = "CatParamStickLX"; app->left_stick_x = event->value;
    } else if (strcmp(event->name, "LeftStickY") == 0) {
        id = "CatParamStickLY"; app->left_stick_y = event->value;
    } else if (strcmp(event->name, "RightStickX") == 0) {
        id = "CatParamStickRX"; app->right_stick_x = event->value;
    } else if (strcmp(event->name, "RightStickY") == 0) {
        id = "CatParamStickRY"; app->right_stick_y = event->value;
    } else if (strcmp(event->name, "LeftThumb") == 0) {
        app->left_stick_pressed = event->value > 0.0f;
        l2dcat_live2d_set_parameter(app->live2d, "CatParamStickLeftDown",
            app->left_stick_pressed);
    } else if (strcmp(event->name, "RightThumb") == 0) {
        app->right_stick_pressed = event->value > 0.0f;
        l2dcat_live2d_set_parameter(app->live2d, "CatParamStickRightDown",
            app->right_stick_pressed);
    } else apply_key(app, event->name, event->value > 0.05f);
    if (id) set_axis(app, id, event->value);
    update_hands(app);
    app->dirty = true;
}

void l2dcat_app_reset_gamepad(L2DCatApp *app) {
    if (!app || !app->live2d) return;
    app->left_stick_x = app->left_stick_y = 0.0f;
    app->right_stick_x = app->right_stick_y = 0.0f;
    app->left_stick_pressed = app->right_stick_pressed = false;
    const char *parameters[] = {"CatParamStickLX", "CatParamStickLY",
        "CatParamStickRX", "CatParamStickRY", "CatParamStickLeftDown",
        "CatParamStickRightDown"};
    for (size_t i = 0; i < sizeof(parameters) / sizeof(parameters[0]); ++i)
        l2dcat_live2d_set_parameter(app->live2d, parameters[i], 0.0f);
    update_hands(app);
    app->dirty = true;
}

void l2dcat_app_apply_input(L2DCatApp *app, const L2DCatInputEvent *event) {
    if (!app || !event || !app->live2d) return;
    switch (event->kind) {
    case L2DCAT_INPUT_KEY_DOWN: apply_key(app, event->name, true); break;
    case L2DCAT_INPUT_KEY_UP: apply_key(app, event->name, false); break;
    case L2DCAT_INPUT_MOUSE_DOWN:
    case L2DCAT_INPUT_MOUSE_UP: {
        if (strcmp(event->name, "Left") != 0 && strcmp(event->name, "Right") != 0)
            break;
        bool down = event->kind == L2DCAT_INPUT_MOUSE_DOWN;
        const char *id = strcmp(event->name, "Left") == 0
            ? "ParamMouseLeftDown" : "ParamMouseRightDown";
        l2dcat_live2d_set_parameter(app->live2d, id, down ? 1.0f : 0.0f);
        app->dirty = true;
        break;
    }
    case L2DCAT_INPUT_GAMEPAD_BUTTON:
    case L2DCAT_INPUT_GAMEPAD_AXIS: apply_gamepad(app, event); break;
    default: break;
    }
}
