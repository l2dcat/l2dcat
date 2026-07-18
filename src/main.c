#include "l2dcat/app.h"
#include "l2dcat/platform.h"
#include <SDL3/SDL_main.h>

int main(int argc, char **argv) {
    int helper = l2dcat_platform_update_helper(argc, argv);
    if (helper >= 0) return helper;
    return l2dcat_app_run(argc, argv);
}
