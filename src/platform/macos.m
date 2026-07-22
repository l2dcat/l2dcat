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

/* SDL 3.2's pinned Cocoa tray layout; kept local to the platform adapter. */
typedef struct L2DCatSDLTrayMenu { NSMenu *menu; } L2DCatSDLTrayMenu;
typedef struct L2DCatSDLTray {
    NSStatusBar *status_bar; NSStatusItem *status_item; L2DCatSDLTrayMenu *menu;
} L2DCatSDLTray;

@interface L2DCatTrayTarget : NSObject {
    NSStatusItem *item_; NSMenu *menu_; L2DCatTrayClick callback_; void *userdata_;
}
- (id)initWithItem:(NSStatusItem *)item menu:(NSMenu *)menu
    callback:(L2DCatTrayClick)callback userdata:(void *)userdata;
- (void)clicked:(id)sender;
- (void)unbind;
@end

@implementation L2DCatTrayTarget
- (id)initWithItem:(NSStatusItem *)item menu:(NSMenu *)menu
    callback:(L2DCatTrayClick)callback userdata:(void *)userdata {
    self = [super init];
    if (self) { item_ = item; menu_ = menu; callback_ = callback; userdata_ = userdata; }
    return self;
}
- (void)clicked:(id)sender {
    (void)sender;
    NSEvent *event = [NSApp currentEvent];
    if ([event type] == NSEventTypeLeftMouseUp) callback_(userdata_);
    else [NSMenu popUpContextMenu:menu_ withEvent:event forView:[item_ button]];
}
- (void)unbind {
    [[item_ button] setTarget:nil]; [[item_ button] setAction:nil];
    [item_ setMenu:menu_];
}
@end

static L2DCatTrayTarget *tray_target;

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
void l2dcat_platform_raise_window(SDL_Window *window) {
    if (!window) return;
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    NSWindow *native = (__bridge NSWindow *)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    [NSApp activateIgnoringOtherApps:YES];
    [native makeKeyAndOrderFront:nil];
}

bool l2dcat_platform_set_geometry(L2DCatPlatform *platform,
    int x, int y, int width, int height) {
    if (!platform || !platform->window) return false;
    int current_width, current_height;
    if ((!SDL_GetWindowSize(platform->window, &current_width, &current_height) ||
        current_width != width || current_height != height) &&
        !SDL_SetWindowSize(platform->window, width, height)) return false;
    int current_x, current_y;
    if (!SDL_GetWindowPosition(platform->window, &current_x, &current_y) ||
        current_x != x || current_y != y)
        SDL_SetWindowPosition(platform->window, x, y);
    return true;
}
void l2dcat_platform_begin_drag(L2DCatPlatform *platform) {
    NSWindow *window = native_window(platform);
    [window performWindowDragWithEvent:[NSApp currentEvent]];
}
bool l2dcat_platform_global_input_supported(void) {
    return l2dcat_macos_input_supported();
}

void l2dcat_platform_set_tray_left_click(void *tray, L2DCatTrayClick callback,
    void *userdata) {
    L2DCatSDLTray *native = tray;
    if (tray_target) {
        [tray_target unbind]; [tray_target release]; tray_target = nil;
    }
    if (!native || !callback || !native->status_item || !native->menu) return;
    tray_target = [[L2DCatTrayTarget alloc] initWithItem:native->status_item
        menu:native->menu->menu callback:callback userdata:userdata];
    [native->status_item setMenu:nil];
    [[native->status_item button] setTarget:tray_target];
    [[native->status_item button] setAction:@selector(clicked:)];
    [[native->status_item button] sendActionOn:NSEventMaskLeftMouseUp |
        NSEventMaskRightMouseUp];
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
