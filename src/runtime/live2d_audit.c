#include "runtime.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void input(L2DCatApp *app, L2DCatInputKind kind, const char *name, float value) {
    L2DCatInputEvent event = {.kind = kind, .value = value};
    snprintf(event.name, sizeof(event.name), "%s", name);
    l2dcat_app_apply_input(app, &event);
}

static bool motion(L2DCatApp *app, const char *scenario) {
    if (strcmp(scenario, "motion-0") == 0)
        return l2dcat_live2d_start_motion(app->live2d, "CAT_motion", 0);
    if (strcmp(scenario, "motion-1") == 0)
        return l2dcat_live2d_start_motion(app->live2d, "CAT_motion", 1);
    if (strncmp(scenario, "expression-", 11) == 0)
        return l2dcat_live2d_set_expression(app->live2d, atoi(scenario + 11));
    return true;
}

static bool pointer(L2DCatApp *app, bool mirror) {
    SDL_Rect bounds;
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (!display || !SDL_GetDisplayBounds(display, &bounds)) return false;
    app->config.model.mouse_mirror = mirror;
    l2dcat_app_apply_mouse_position(app, bounds.x + bounds.w * 0.9,
        bounds.y + bounds.h * 0.1, 1.0f / 60.0f);
    return true;
}

static bool apply(L2DCatApp *app, const char *scenario) {
    if (strncmp(scenario, "switch:", 7) == 0)
        return l2dcat_app_select_model(app, scenario + 7);
    if (strcmp(scenario, "mirror") == 0) app->config.model.mirror = true;
    else if (strcmp(scenario, "mouse-move") == 0) return pointer(app, false);
    else if (strcmp(scenario, "mouse-move-mirror") == 0) return pointer(app, true);
    else if (strcmp(scenario, "key-left") == 0)
        input(app, L2DCAT_INPUT_KEY_DOWN, "KeyA", 1.0f);
    else if (strcmp(scenario, "key-right") == 0)
        input(app, L2DCAT_INPUT_KEY_DOWN, "RightArrow", 1.0f);
    else if (strcmp(scenario, "key-left-release") == 0) {
        input(app, L2DCAT_INPUT_KEY_DOWN, "KeyA", 1.0f);
        input(app, L2DCAT_INPUT_KEY_UP, "KeyA", 0.0f);
    } else if (strcmp(scenario, "keys-both-release") == 0) {
        input(app, L2DCAT_INPUT_KEY_DOWN, "KeyA", 1.0f);
        input(app, L2DCAT_INPUT_KEY_DOWN, "RightArrow", 1.0f);
        input(app, L2DCAT_INPUT_KEY_UP, "KeyA", 0.0f);
        input(app, L2DCAT_INPUT_KEY_UP, "RightArrow", 0.0f);
    } else if (strcmp(scenario, "key-stress") == 0) {
        for (int i = 0; i < 250; ++i) {
            input(app, L2DCAT_INPUT_KEY_DOWN, "KeyA", 1.0f);
            input(app, L2DCAT_INPUT_KEY_DOWN, "RightArrow", 1.0f);
            input(app, L2DCAT_INPUT_KEY_UP, "KeyA", 0.0f);
            input(app, L2DCAT_INPUT_KEY_UP, "RightArrow", 0.0f);
        }
    }
    else if (strcmp(scenario, "keys-both") == 0) {
        input(app, L2DCAT_INPUT_KEY_DOWN, "KeyA", 1.0f);
        input(app, L2DCAT_INPUT_KEY_DOWN, "RightArrow", 1.0f);
    } else if (strcmp(scenario, "mouse-left") == 0)
        input(app, L2DCAT_INPUT_MOUSE_DOWN, "Left", 1.0f);
    else if (strcmp(scenario, "mouse-right") == 0)
        input(app, L2DCAT_INPUT_MOUSE_DOWN, "Right", 1.0f);
    else if (strcmp(scenario, "gamepad-buttons") == 0) {
        input(app, L2DCAT_INPUT_GAMEPAD_BUTTON, "DPadLeft", 1.0f);
        input(app, L2DCAT_INPUT_GAMEPAD_BUTTON, "South", 1.0f);
    } else if (strcmp(scenario, "gamepad-sticks") == 0) {
        input(app, L2DCAT_INPUT_GAMEPAD_AXIS, "LeftStickX", .75f);
        input(app, L2DCAT_INPUT_GAMEPAD_AXIS, "LeftStickY", -.5f);
        input(app, L2DCAT_INPUT_GAMEPAD_AXIS, "RightStickX", -.65f);
        input(app, L2DCAT_INPUT_GAMEPAD_AXIS, "RightStickY", .5f);
        input(app, L2DCAT_INPUT_GAMEPAD_BUTTON, "LeftThumb", 1.0f);
        input(app, L2DCAT_INPUT_GAMEPAD_BUTTON, "RightThumb", 1.0f);
    } else return motion(app, scenario);
    app->dirty = true;
    return true;
}

static void parameter(FILE *file, L2DCatApp *app, const char *id) {
    L2DCatParameterRange range;
    if (l2dcat_live2d_parameter(app->live2d, id, &range))
        fprintf(file, "parameter.%s=%.4f [%.4f,%.4f]\n", id,
            range.value, range.minimum, range.maximum);
    else fprintf(file, "parameter.%s=unavailable\n", id);
}

static bool value(L2DCatApp *app, const char *id, float *output) {
    L2DCatParameterRange range;
    if (!l2dcat_live2d_parameter(app->live2d, id, &range)) return false;
    if (output) *output = range.value;
    return true;
}

static bool active(L2DCatApp *app, const char *id) {
    float current = 0.0f;
    return value(app, id, &current) && current > 0.5f;
}

static bool signed_value(L2DCatApp *app, const char *id, bool positive) {
    float current = 0.0f;
    return value(app, id, &current) && (positive ? current > 0.05f : current < -0.05f);
}

static bool assertions(L2DCatApp *app, const char *scenario, bool operation) {
    if (!operation) return false;
    if (strncmp(scenario, "switch:", 7) == 0)
        return strcmp(app->config.current_model, scenario + 7) == 0;
    if (strcmp(scenario, "idle") == 0 || strcmp(scenario, "mirror") == 0 ||
        strcmp(scenario, "key-left-release") == 0 ||
        strcmp(scenario, "keys-both-release") == 0 ||
        strcmp(scenario, "key-stress") == 0 ||
        strncmp(scenario, "motion-", 7) == 0 ||
        strncmp(scenario, "expression-", 11) == 0) return true;
    if (strcmp(scenario, "key-left") == 0)
        return active(app, "CatParamLeftHandDown");
    if (strcmp(scenario, "key-right") == 0)
        return active(app, "CatParamRightHandDown");
    if (strcmp(scenario, "keys-both") == 0 ||
        strcmp(scenario, "gamepad-buttons") == 0)
        return active(app, "CatParamLeftHandDown") &&
            active(app, "CatParamRightHandDown");
    if (strcmp(scenario, "mouse-left") == 0)
        return active(app, "ParamMouseLeftDown");
    if (strcmp(scenario, "mouse-right") == 0)
        return active(app, "ParamMouseRightDown");
    if (strcmp(scenario, "mouse-move") == 0)
        return signed_value(app, "ParamAngleX", false) &&
            signed_value(app, "ParamAngleY", true);
    if (strcmp(scenario, "mouse-move-mirror") == 0)
        return signed_value(app, "ParamAngleX", true) &&
            signed_value(app, "ParamAngleY", true);
    if (strcmp(scenario, "gamepad-sticks") == 0)
        return signed_value(app, "CatParamStickLX", true) &&
            signed_value(app, "CatParamStickLY", false) &&
            signed_value(app, "CatParamStickRX", false) &&
            signed_value(app, "CatParamStickRY", true) &&
            active(app, "CatParamLeftHandDown") &&
            active(app, "CatParamRightHandDown");
    return false;
}

void l2dcat_live2d_audit_run(L2DCatApp *app) {
    if (!app || !app->smoke_live2d_scenario[0]) return;
    uint64_t started = SDL_GetTicksNS();
    bool result = apply(app, app->smoke_live2d_scenario);
    double duration_ms = (SDL_GetTicksNS() - started) / 1000000.0;
    char path[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(path, sizeof(path), app->data_root, "live2d-audit.txt")) return;
    FILE *file = l2dcat_file_open(path, "wb");
    if (!file) return;
    bool verified = assertions(app, app->smoke_live2d_scenario, result);
    fprintf(file, "scenario=%s\nmodel=%s\nmode=%s\noperation=%s\nassertions=%s\n"
        "duration_ms=%.3f\n",
        app->smoke_live2d_scenario, app->config.current_model,
        l2dcat_mode_name(app->config.current_mode), result ? "accepted" : "rejected",
        verified ? "passed" : "failed", duration_ms);
#ifdef L2DCAT_HAS_CUBISM
    fputs("renderer=cubism-native\n", file);
    if (!verified) app->exit_code = 1;
#else
    fputs("renderer=diagnostic; visual-model-result=blocked\n", file);
#endif
    const char *parameters[] = {"CatParamLeftHandDown", "CatParamRightHandDown",
        "CatParamStickLX", "CatParamStickLY", "CatParamStickRX", "CatParamStickRY",
        "CatParamStickLeftDown", "CatParamStickRightDown",
        "CatParamStickShowLeftHand", "CatParamStickShowRightHand",
        "ParamMouseLeftDown", "ParamMouseRightDown", "ParamMouseX", "ParamMouseY",
        "ParamAngleX", "ParamAngleY", "ParamAngleZ", "ParamEyeBallX", "ParamEyeBallY"};
    for (size_t i = 0; i < sizeof(parameters) / sizeof(parameters[0]); ++i)
        parameter(file, app, parameters[i]);
    fclose(file);
}
