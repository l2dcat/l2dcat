#include "l2dcat/platform.h"
#include "macos_internal.h"

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

static int instance_lock = -1;
static L2DCatPlatform *active_platform;
static NSObject *instance_observer;
static bool instance_show_pending;

static NSWindow *native_window(L2DCatPlatform *platform) {
    return (__bridge NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(platform->window),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
}

static void show_instance(void) {
    if (!active_platform) { instance_show_pending = true; return; }
    [native_window(active_platform) orderFrontRegardless];
    [NSApp activateIgnoringOtherApps:YES];
    instance_show_pending = false;
}

@interface L2DCatInstanceObserver : NSObject
- (void)showWindow:(NSNotification *)notification;
@end
@implementation L2DCatInstanceObserver
- (void)showWindow:(NSNotification *)notification {
    if (![NSThread isMainThread]) {
        [self performSelectorOnMainThread:@selector(showWindow:)
            withObject:notification waitUntilDone:NO];
        return;
    }
    show_instance();
}
@end

static void observe_instance(void) {
    if (instance_observer) return;
    instance_observer = [[L2DCatInstanceObserver alloc] init];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:instance_observer
        selector:@selector(showWindow:) name:@"app.l2dcat.native.show" object:nil
        suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
}
L2DCatResult l2dcat_platform_init(L2DCatPlatform *platform, SDL_Window *window,
    L2DCatInputState *input, L2DCatError *error) {
    (void)error;
    memset(platform, 0, sizeof(*platform));
    platform->window = window;
    platform->input = input;
    active_platform = platform;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    L2DCatError input_error = {0};
    if (!l2dcat_macos_input_start(platform, &input_error))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", input_error.message);
    if (instance_show_pending) show_instance();
    return L2DCAT_OK;
}
void l2dcat_platform_shutdown(L2DCatPlatform *platform) {
    l2dcat_macos_input_stop(platform);
    if (active_platform == platform) active_platform = NULL;
}
void l2dcat_platform_set_click_through(L2DCatPlatform *platform, bool enabled) {
    [native_window(platform) setIgnoresMouseEvents:enabled];
}
bool l2dcat_platform_pointer_local(L2DCatPlatform *platform, double screen_x,
    double screen_y, float *local_x, float *local_y) {
    int x, y, width, height;
    if (!platform || !local_x || !local_y ||
        !SDL_GetWindowPosition(platform->window, &x, &y) ||
        !SDL_GetWindowSize(platform->window, &width, &height)) return false;
    *local_x = (float)(screen_x - x); *local_y = (float)(screen_y - y);
    return *local_x >= 0 && *local_x < width && *local_y >= 0 && *local_y < height;
}
void l2dcat_platform_set_always_on_top(L2DCatPlatform *platform, bool enabled) {
    [native_window(platform) setLevel:enabled ? NSFloatingWindowLevel : NSNormalWindowLevel];
}
void l2dcat_platform_set_taskbar(L2DCatPlatform *platform, bool visible) {
    (void)platform;
    [NSApp setActivationPolicy:visible ? NSApplicationActivationPolicyRegular :
        NSApplicationActivationPolicyAccessory];
}

void l2dcat_platform_set_geometry(L2DCatPlatform *platform,
    int x, int y, int width, int height) {
    if (!platform || !platform->window) return;
    SDL_SetWindowSize(platform->window, width, height);
    SDL_SetWindowPosition(platform->window, x, y);
}
void l2dcat_platform_begin_drag(L2DCatPlatform *platform) {
    NSWindow *window = native_window(platform);
    [window performWindowDragWithEvent:[NSApp currentEvent]];
}
bool l2dcat_platform_global_input_supported(void) {
    return l2dcat_macos_input_supported();
}
bool l2dcat_platform_single_instance_begin(void) {
    char path[96]; snprintf(path, sizeof(path), "/tmp/l2dcat-native-%lu.lock",
        (unsigned long)getuid());
    instance_lock = open(path, O_CREAT | O_RDWR, 0600);
    if (instance_lock < 0 || flock(instance_lock, LOCK_EX | LOCK_NB) == 0) {
        observe_instance(); return true;
    }
    [[NSDistributedNotificationCenter defaultCenter]
        postNotificationName:@"app.l2dcat.native.show" object:nil
        userInfo:nil deliverImmediately:YES];
    close(instance_lock); instance_lock = -1; return false;
}
void l2dcat_platform_single_instance_end(void) {
    if (instance_lock >= 0) close(instance_lock);
    instance_lock = -1;
    if (instance_observer) {
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:instance_observer];
        [instance_observer release]; instance_observer = nil;
    }
}
L2DCatResult l2dcat_platform_set_autostart(bool enabled, L2DCatError *error) {
    @autoreleasepool {
        NSString *directory = [NSHomeDirectory()
            stringByAppendingPathComponent:@"Library/LaunchAgents"];
        NSString *path = [directory
            stringByAppendingPathComponent:@"app.l2dcat.native.plist"];
        NSFileManager *files = [NSFileManager defaultManager];
        if (!enabled) {
            if (![files fileExistsAtPath:path] || [files removeItemAtPath:path error:nil])
                return L2DCAT_OK;
            l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot remove macOS launch agent");
            return L2DCAT_ERROR_IO;
        }
        NSString *executable = [[NSBundle mainBundle] executablePath];
        if (!executable || ![files createDirectoryAtPath:directory
            withIntermediateDirectories:YES attributes:nil error:nil]) {
            l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot create macOS launch agent directory");
            return L2DCAT_ERROR_IO;
        }
        NSDictionary *plist = @{ @"Label": @"app.l2dcat.native",
            @"ProgramArguments": @[executable], @"RunAtLoad": @YES };
        if ([plist writeToFile:path atomically:YES]) return L2DCAT_OK;
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot write macOS launch agent");
        return L2DCAT_ERROR_IO;
    }
}
bool l2dcat_platform_is_elevated(void) { return geteuid() == 0; }
L2DCatMenuAction l2dcat_platform_context_menu(L2DCatPlatform *platform,
    const L2DCatMenuLabels *labels) {
    return l2dcat_macos_context_menu(platform, labels);
}
bool l2dcat_platform_restart(void) {
    const char *executable = [[[NSBundle mainBundle] executablePath] fileSystemRepresentation];
    if (!executable) return false;
    pid_t child = fork();
    if (child == 0) { execl(executable, executable, NULL); _exit(127); }
    return child > 0;
}
L2DCatResult l2dcat_platform_embedded_assets(const char *target, L2DCatError *error) {
    (void)target; (void)error; return L2DCAT_ERROR_PLATFORM;
}
#endif
