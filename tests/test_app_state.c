#include "test.h"
#include "bongo/app.h"
#include "bongo/overlay.h"

#include <stdio.h>
#include <string.h>

typedef struct ParameterValue {
    char id[BONGO_ID_CAP];
    float value;
} ParameterValue;

static ParameterValue parameters[128];
static size_t parameter_count;
static bool left_hand, right_hand;
int bongo_test_failures;

static float parameter(const char *id) {
    for (size_t i = parameter_count; i > 0; --i)
        if (strcmp(parameters[i - 1].id, id) == 0) return parameters[i - 1].value;
    return -999.0f;
}

bool bongo_live2d_set_parameter(BongoLive2D *live2d, const char *id, float value) {
    (void)live2d;
    if (parameter_count >= sizeof(parameters) / sizeof(parameters[0])) return false;
    snprintf(parameters[parameter_count].id,
        sizeof(parameters[parameter_count].id), "%s", id);
    parameters[parameter_count++].value = value;
    return true;
}

bool bongo_live2d_parameter(BongoLive2D *live2d, const char *id,
    BongoParameterRange *range) {
    (void)live2d; (void)id;
    if (!range) return false;
    *range = (BongoParameterRange){-1.0f, 1.0f, 0.0f};
    return true;
}

int bongo_overlay_key(BongoOverlay *overlay, const char *name, bool pressed) {
    (void)overlay;
    if (strcmp(name, "KeyA") == 0 || strcmp(name, "DPadLeft") == 0)
        left_hand = pressed;
    else if (strcmp(name, "RightArrow") == 0 || strcmp(name, "South") == 0)
        right_hand = pressed;
    return 0;
}

bool bongo_overlay_hand_active(const BongoOverlay *overlay, bool right) {
    (void)overlay; return right ? right_hand : left_hand;
}

static BongoInputEvent input(BongoInputKind kind, const char *name, float value) {
    BongoInputEvent event = {.kind = kind, .value = value};
    snprintf(event.name, sizeof(event.name), "%s", name);
    return event;
}

int main(void) {
    BongoApp app = {0};
    app.live2d = (BongoLive2D *)(uintptr_t)1;
    app.overlay = (BongoOverlay *)(uintptr_t)1;

    BongoInputEvent event = input(BONGO_INPUT_MOUSE_DOWN, "Middle", 1.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter_count == 0);
    event = input(BONGO_INPUT_MOUSE_DOWN, "Left", 1.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("ParamMouseLeftDown") == 1.0f);
    event = input(BONGO_INPUT_MOUSE_DOWN, "Right", 1.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("ParamMouseRightDown") == 1.0f);
    event = input(BONGO_INPUT_MOUSE_UP, "Right", 0.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("ParamMouseRightDown") == 0.0f);

    event = input(BONGO_INPUT_KEY_DOWN, "KeyA", 1.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamLeftHandDown") == 1.0f);
    event = input(BONGO_INPUT_KEY_DOWN, "RightArrow", 1.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamRightHandDown") == 1.0f);
    event = input(BONGO_INPUT_KEY_UP, "KeyA", 0.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamLeftHandDown") == 0.0f);
    event = input(BONGO_INPUT_KEY_UP, "RightArrow", 0.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamRightHandDown") == 0.0f);

    event = input(BONGO_INPUT_GAMEPAD_AXIS, "LeftStickX", 0.5f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamStickLX") == 0.5f);
    CHECK(parameter("CatParamStickShowLeftHand") == 1.0f);
    CHECK(parameter("CatParamLeftHandDown") == 1.0f);
    event = input(BONGO_INPUT_GAMEPAD_BUTTON, "LeftThumb", 1.0f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamStickLeftDown") == 1.0f);
    event = input(BONGO_INPUT_GAMEPAD_AXIS, "LeftStickY", -0.5f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamStickLY") == -0.5f);
    event = input(BONGO_INPUT_GAMEPAD_AXIS, "RightStickX", -0.75f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamStickRX") == -0.75f);
    event = input(BONGO_INPUT_GAMEPAD_AXIS, "RightStickY", 0.25f);
    bongo_app_apply_input(&app, &event);
    CHECK(parameter("CatParamStickRY") == 0.25f);

    bongo_app_reset_gamepad(&app);
    CHECK(parameter("CatParamStickLX") == 0.0f);
    CHECK(parameter("CatParamStickLeftDown") == 0.0f);
    CHECK(parameter("CatParamStickShowLeftHand") == 0.0f);
    CHECK(parameter("CatParamLeftHandDown") == 0.0f);
    return bongo_test_failures ? 1 : 0;
}
