#include "bongo/platform.h"
#include "bongo/path.h"
#include "linux_internal.h"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <X11/Xlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

static int instance_lock = -1;
static BongoPlatform *active_platform;

static void publish_instance_window(SDL_Window *window) {
    if (instance_lock < 0 || !window) return;
    Window id = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (!id) return;
    char text[32]; int length = snprintf(text, sizeof(text), "%lu\n", (unsigned long)id);
    if (length <= 0 || (size_t)length >= sizeof(text)) return;
    if (ftruncate(instance_lock, 0) == 0 && lseek(instance_lock, 0, SEEK_SET) == 0) {
        if (write(instance_lock, text, (size_t)length) == (ssize_t)length) fsync(instance_lock);
    }
}

static void restore_instance_window(void) {
    if (instance_lock < 0) return;
    char text[32] = {0}; lseek(instance_lock, 0, SEEK_SET);
    ssize_t length = read(instance_lock, text, sizeof(text) - 1);
    Window window = length > 0 ? (Window)strtoul(text, NULL, 10) : 0;
    Display *display = window ? XOpenDisplay(NULL) : NULL;
    if (!display) return;
    XMapRaised(display, window);
    Atom active = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    XEvent event = {0}; event.xclient.type = ClientMessage;
    event.xclient.window = window; event.xclient.message_type = active;
    event.xclient.format = 32; event.xclient.data.l[0] = 2;
    XSendEvent(display, DefaultRootWindow(display), False,
        SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display); XCloseDisplay(display);
}

static bool executable_path(char output[BONGO_PATH_CAP]) {
    const char *appimage = getenv("APPIMAGE");
    if (appimage && appimage[0]) {
        int length = snprintf(output, BONGO_PATH_CAP, "%s", appimage);
        return length >= 0 && length < BONGO_PATH_CAP;
    }
    ssize_t length = readlink("/proc/self/exe", output, BONGO_PATH_CAP - 1);
    if (length <= 0 || length >= BONGO_PATH_CAP) return false;
    output[length] = '\0'; return true;
}

BongoResult bongo_platform_init(BongoPlatform *platform, SDL_Window *window,
    BongoInputState *input, BongoError *error) {
    (void)error;
    memset(platform, 0, sizeof(*platform));
    platform->window = window;
    platform->input = input;
    active_platform = platform;
    publish_instance_window(window);
    BongoError input_error = {0};
    if (!bongo_linux_x11_start(platform, &input_error) && input_error.message[0])
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", input_error.message);
    return BONGO_OK;
}
void bongo_platform_shutdown(BongoPlatform *platform) {
    bongo_linux_x11_stop(platform);
    if (active_platform == platform) active_platform = NULL;
}
void bongo_platform_set_click_through(BongoPlatform *platform, bool enabled) {
    bongo_linux_x11_click_through(platform, enabled);
}
void bongo_platform_set_always_on_top(BongoPlatform *platform, bool enabled) {
    SDL_SetWindowAlwaysOnTop(platform->window, enabled);
}
void bongo_platform_set_taskbar(BongoPlatform *platform, bool visible) {
    bongo_linux_x11_taskbar(platform, visible);
}
void bongo_platform_begin_drag(BongoPlatform *platform) {
    bongo_linux_x11_begin_drag(platform);
}
bool bongo_platform_global_input_supported(void) {
    return bongo_linux_x11_supported(active_platform);
}
bool bongo_platform_single_instance_begin(void) {
    char path[96]; snprintf(path, sizeof(path), "/tmp/bongocat-native-%lu.lock",
        (unsigned long)getuid());
    instance_lock = open(path, O_CREAT | O_RDWR, 0600);
    if (instance_lock < 0 || flock(instance_lock, LOCK_EX | LOCK_NB) == 0) return true;
    restore_instance_window(); close(instance_lock); instance_lock = -1; return false;
}
void bongo_platform_single_instance_end(void) {
    if (instance_lock >= 0) close(instance_lock);
    instance_lock = -1;
}
BongoResult bongo_platform_set_autostart(bool enabled, BongoError *error) {
    const char *base = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    char config[BONGO_PATH_CAP], directory[BONGO_PATH_CAP], path[BONGO_PATH_CAP];
    if (base && base[0]) snprintf(config, sizeof(config), "%s", base);
    else if (home && bongo_path_join(config, sizeof(config), home, ".config")) {}
    else return BONGO_ERROR_PLATFORM;
    if (!bongo_path_join(directory, sizeof(directory), config, "autostart") ||
        !bongo_path_join(path, sizeof(path), directory, "BongoCat.desktop"))
        return BONGO_ERROR_IO;
    if (!enabled) {
        if (remove(path) == 0 || errno == ENOENT) return BONGO_OK;
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot remove Linux autostart entry");
        return BONGO_ERROR_IO;
    }
    char executable[BONGO_PATH_CAP];
    if (!executable_path(executable) || strchr(executable, '"') ||
        !SDL_CreateDirectory(directory)) {
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot prepare Linux autostart entry");
        return BONGO_ERROR_IO;
    }
    FILE *file = fopen(path, "wb");
    if (!file) return BONGO_ERROR_IO;
    bool written = fprintf(file, "[Desktop Entry]\nType=Application\nName=BongoCat\n"
        "Exec=\"%s\"\nTerminal=false\nX-GNOME-Autostart-enabled=true\n", executable) > 0;
    if (fclose(file) != 0) written = false;
    if (written) return BONGO_OK;
    remove(path); bongo_error_set(error, BONGO_ERROR_IO, "Cannot write Linux autostart entry");
    return BONGO_ERROR_IO;
}
bool bongo_platform_is_elevated(void) { return geteuid() == 0; }
BongoMenuAction bongo_platform_context_menu(BongoPlatform *platform,
    const BongoMenuLabels *labels) {
    return bongo_linux_context_menu(platform, labels);
}
bool bongo_platform_restart(void) {
    char executable[BONGO_PATH_CAP];
    if (!executable_path(executable)) return false;
    pid_t child = fork();
    if (child == 0) { execl(executable, executable, NULL); _exit(127); }
    return child > 0;
}
BongoResult bongo_platform_embedded_assets(const char *target, BongoError *error) {
    (void)target; (void)error; return BONGO_ERROR_PLATFORM;
}
bool bongo_platform_schedule_update(const char *staged, BongoError *error) {
    if (!staged) return false;
    char executable[BONGO_PATH_CAP], helper[BONGO_PATH_CAP], parent[32];
    const char *appimage = getenv("APPIMAGE");
    ssize_t length = appimage && appimage[0] ? (ssize_t)snprintf(executable,
        sizeof(executable), "%s", appimage) : readlink("/proc/self/exe",
        executable, sizeof(executable) - 1);
    if (length <= 0 || (size_t)length >= sizeof(executable)) {
        bongo_error_set(error, BONGO_ERROR_PLATFORM, "Cannot locate current executable");
        return false;
    }
    executable[length] = '\0';
    if (snprintf(helper, sizeof(helper), "%s.helper", staged) >= (int)sizeof(helper) ||
        !SDL_CopyFile("/proc/self/exe", helper) || chmod(helper, 0755) != 0) {
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot create update helper: %s",
            SDL_GetError());
        return false;
    }
    snprintf(parent, sizeof(parent), "%ld", (long)getpid());
    pid_t child = fork();
    if (child == 0) {
        execl(helper, helper, "--bongo-apply-update", staged, executable, parent, NULL);
        _exit(127);
    }
    if (child < 0) {
        remove(helper);
        bongo_error_set(error, BONGO_ERROR_PLATFORM, "Cannot launch update helper");
        return false;
    }
    return true;
}

static int apply_update(char **argv) {
    const char *staged = argv[2], *target = argv[3], *helper = argv[0];
    pid_t parent = (pid_t)strtol(argv[4], NULL, 10);
    for (int i = 0; i < 600 && kill(parent, 0) == 0; ++i) usleep(100000);
    char replacement[BONGO_PATH_CAP], backup[BONGO_PATH_CAP];
    if (snprintf(replacement, sizeof(replacement), "%s.new", target) >=
        (int)sizeof(replacement) || snprintf(backup, sizeof(backup), "%s.old", target) >=
        (int)sizeof(backup) || !SDL_CopyFile(staged, replacement) ||
        chmod(replacement, 0755) != 0) {
        remove(replacement);
        return 1;
    }
    remove(backup);
    if (rename(target, backup) != 0 || rename(replacement, target) != 0) {
        rename(backup, target); remove(replacement); return 1;
    }
    remove(staged);
    execl(target, target, "--bongo-cleanup-helper", helper, backup, NULL);
    remove(target); rename(backup, target);
    return 1;
}

int bongo_platform_update_helper(int argc, char **argv) {
    if (argc == 5 && strcmp(argv[1], "--bongo-apply-update") == 0)
        return apply_update(argv);
    if (argc == 4 && strcmp(argv[1], "--bongo-cleanup-helper") == 0) {
        remove(argv[2]); remove(argv[3]);
        return -1;
    }
    return -1;
}
#endif
