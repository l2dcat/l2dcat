#include "bongo/platform.h"
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
static BongoPlatform *active_platform;
static NSObject *instance_observer;
static bool instance_show_pending;

static NSWindow *native_window(BongoPlatform *platform) {
    return (__bridge NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(platform->window),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
}

static void show_instance(void) {
    if (!active_platform) { instance_show_pending = true; return; }
    [native_window(active_platform) orderFrontRegardless];
    [NSApp activateIgnoringOtherApps:YES];
    instance_show_pending = false;
}

@interface BongoInstanceObserver : NSObject
- (void)showWindow:(NSNotification *)notification;
@end
@implementation BongoInstanceObserver
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
    instance_observer = [[BongoInstanceObserver alloc] init];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:instance_observer
        selector:@selector(showWindow:) name:@"app.bongocat.native.show" object:nil
        suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
}
BongoResult bongo_platform_init(BongoPlatform *platform, SDL_Window *window,
    BongoInputState *input, BongoError *error) {
    (void)error;
    memset(platform, 0, sizeof(*platform));
    platform->window = window;
    platform->input = input;
    active_platform = platform;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    BongoError input_error = {0};
    if (!bongo_macos_input_start(platform, &input_error))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", input_error.message);
    if (instance_show_pending) show_instance();
    return BONGO_OK;
}
void bongo_platform_shutdown(BongoPlatform *platform) {
    bongo_macos_input_stop(platform);
    if (active_platform == platform) active_platform = NULL;
}
void bongo_platform_set_click_through(BongoPlatform *platform, bool enabled) {
    [native_window(platform) setIgnoresMouseEvents:enabled];
}
void bongo_platform_set_always_on_top(BongoPlatform *platform, bool enabled) {
    [native_window(platform) setLevel:enabled ? NSFloatingWindowLevel : NSNormalWindowLevel];
}
void bongo_platform_set_taskbar(BongoPlatform *platform, bool visible) {
    (void)platform;
    [NSApp setActivationPolicy:visible ? NSApplicationActivationPolicyRegular :
        NSApplicationActivationPolicyAccessory];
}
void bongo_platform_begin_drag(BongoPlatform *platform) {
    NSWindow *window = native_window(platform);
    [window performWindowDragWithEvent:[NSApp currentEvent]];
}
bool bongo_platform_global_input_supported(void) {
    return bongo_macos_input_supported();
}
bool bongo_platform_single_instance_begin(void) {
    char path[96]; snprintf(path, sizeof(path), "/tmp/bongocat-native-%lu.lock",
        (unsigned long)getuid());
    instance_lock = open(path, O_CREAT | O_RDWR, 0600);
    if (instance_lock < 0 || flock(instance_lock, LOCK_EX | LOCK_NB) == 0) {
        observe_instance(); return true;
    }
    [[NSDistributedNotificationCenter defaultCenter]
        postNotificationName:@"app.bongocat.native.show" object:nil
        userInfo:nil deliverImmediately:YES];
    close(instance_lock); instance_lock = -1; return false;
}
void bongo_platform_single_instance_end(void) {
    if (instance_lock >= 0) close(instance_lock);
    instance_lock = -1;
    if (instance_observer) {
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:instance_observer];
        [instance_observer release]; instance_observer = nil;
    }
}
BongoResult bongo_platform_set_autostart(bool enabled, BongoError *error) {
    @autoreleasepool {
        NSString *directory = [NSHomeDirectory()
            stringByAppendingPathComponent:@"Library/LaunchAgents"];
        NSString *path = [directory
            stringByAppendingPathComponent:@"app.bongocat.native.plist"];
        NSFileManager *files = [NSFileManager defaultManager];
        if (!enabled) {
            if (![files fileExistsAtPath:path] || [files removeItemAtPath:path error:nil])
                return BONGO_OK;
            bongo_error_set(error, BONGO_ERROR_IO, "Cannot remove macOS launch agent");
            return BONGO_ERROR_IO;
        }
        NSString *executable = [[NSBundle mainBundle] executablePath];
        if (!executable || ![files createDirectoryAtPath:directory
            withIntermediateDirectories:YES attributes:nil error:nil]) {
            bongo_error_set(error, BONGO_ERROR_IO, "Cannot create macOS launch agent directory");
            return BONGO_ERROR_IO;
        }
        NSDictionary *plist = @{ @"Label": @"app.bongocat.native",
            @"ProgramArguments": @[executable], @"RunAtLoad": @YES };
        if ([plist writeToFile:path atomically:YES]) return BONGO_OK;
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot write macOS launch agent");
        return BONGO_ERROR_IO;
    }
}
bool bongo_platform_is_elevated(void) { return geteuid() == 0; }
BongoMenuAction bongo_platform_context_menu(BongoPlatform *platform,
    const BongoMenuLabels *labels) {
    return bongo_macos_context_menu(platform, labels);
}
bool bongo_platform_restart(void) {
    const char *executable = [[[NSBundle mainBundle] executablePath] fileSystemRepresentation];
    if (!executable) return false;
    pid_t child = fork();
    if (child == 0) { execl(executable, executable, NULL); _exit(127); }
    return child > 0;
}
BongoResult bongo_platform_embedded_assets(const char *target, BongoError *error) {
    (void)target; (void)error; return BONGO_ERROR_PLATFORM;
}
#endif
