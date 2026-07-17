#include "bongo/app.h"
#include "bongo/platform.h"

int main(int argc, char **argv) {
    int helper = bongo_platform_update_helper(argc, argv);
    if (helper >= 0) return helper;
    return bongo_app_run(argc, argv);
}
