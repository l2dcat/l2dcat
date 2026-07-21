#include "runtime.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"
#include "l2dcat/preferences.h"

#include <stdio.h>
#include <string.h>

static void mark(L2DCatApp *app, const char *name) {
    char path[L2DCAT_PATH_CAP];
    l2dcat_path_join(path, sizeof(path), app->data_root, "runtime-flow-stage.txt");
    FILE *file = l2dcat_file_open(path, "wb");
    if (file) { fputs(name, file); fclose(file); }
}

static void scale(L2DCatApp *app, float value) {
    l2dcat_window_cancel_wheel_animation(app);
    l2dcat_window_set_scale(app, value);
}

static void opacity(L2DCatApp *app, float value) {
    app->config.window.opacity_percent = value;
    SDL_SetWindowOpacity(app->window, value / 100.0f);
}

static void model(L2DCatApp *app, size_t index) {
    if (index < app->models.count) l2dcat_app_select_model(app, app->models.entries[index].id);
}

static void model_mode(L2DCatApp *app, L2DCatModelMode mode) {
    for (size_t i = 0; i < app->models.count; ++i)
        if (app->models.entries[i].mode == mode) { model(app, i); return; }
}

void l2dcat_runtime_flow_update(L2DCatApp *app, uint64_t now) {
    if (!app || !app->smoke_runtime_flow) return;
    if (!app->smoke_runtime_flow_ns) {
        app->smoke_runtime_flow_ns = now;
        snprintf(app->smoke_runtime_model, sizeof(app->smoke_runtime_model), "%s",
            app->config.current_model);
        mark(app, "startup"); return;
    }
    uint64_t elapsed = now - app->smoke_runtime_flow_ns;
    unsigned target = (unsigned)(elapsed / 1500000000ull);
    if (target <= app->smoke_runtime_stage) return;
    app->smoke_runtime_stage = target;
    switch (target) {
    case 1: mark(app, "idle"); break;
    case 2: scale(app, 125.0f); mark(app, "scale-up"); break;
    case 3: scale(app, 75.0f); mark(app, "scale-down"); break;
    case 4: opacity(app, 50.0f); mark(app, "opacity-50"); break;
    case 5: opacity(app, 100.0f); mark(app, "opacity-100"); break;
    case 6: model_mode(app, L2DCAT_MODE_KEYBOARD); mark(app, "model-keyboard"); break;
    case 7: model_mode(app, L2DCAT_MODE_STANDARD); mark(app, "model-standard"); break;
    case 8: model_mode(app, L2DCAT_MODE_GAMEPAD); mark(app, "model-gamepad"); break;
    case 9: l2dcat_preferences_show(app->preferences); mark(app, "settings-first"); break;
    case 10: mark(app, "settings-idle"); break;
    case 11: l2dcat_preferences_close(app->preferences); mark(app, "settings-closed"); break;
    case 12: l2dcat_preferences_show(app->preferences); mark(app, "settings-reopen"); break;
    case 13:
        l2dcat_preferences_close(app->preferences);
        l2dcat_app_select_model(app, app->smoke_runtime_model);
        mark(app, "recovery"); break;
    case 14: model_mode(app, L2DCAT_MODE_GAMEPAD); mark(app, "gamepad-repeat"); break;
    case 15:
        l2dcat_app_select_model(app, app->smoke_runtime_model);
        mark(app, "final-recovery"); break;
    default: app->running = false; break;
    }
}
