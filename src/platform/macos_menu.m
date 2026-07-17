#include "macos_internal.h"

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include <stdio.h>

@interface BongoMenuTarget : NSObject { NSInteger selected_; }
- (void)choose:(id)sender;
- (NSInteger)selected;
@end

@implementation BongoMenuTarget
- (void)choose:(id)sender { selected_ = [sender tag]; }
- (NSInteger)selected { return selected_; }
@end

static NSString *text(const char *value) {
    return value ? [NSString stringWithUTF8String:value] : @"";
}

static NSMenuItem *add_item(NSMenu *menu, BongoMenuTarget *target,
    const char *label, NSInteger tag, bool checked) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:text(label)
        action:@selector(choose:) keyEquivalent:@""];
    [item setTarget:target]; [item setTag:tag];
    [item setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
    [menu addItem:item]; [item release]; return item;
}

static void add_scale_menu(NSMenu *menu, BongoMenuTarget *target,
    const char *label, bool opacity) {
    NSMenuItem *root = [[NSMenuItem alloc] initWithTitle:text(label)
        action:nil keyEquivalent:@""];
    NSMenu *submenu = [[NSMenu alloc] initWithTitle:text(label)];
    const int values[] = {25,50,75,100,125,150,200};
    int first = opacity ? 0 : 1, count = opacity ? 4 : 6;
    for (int i = 0; i < count; ++i) {
        int value = values[first + i]; char title[16]; snprintf(title, sizeof(title), "%d%%", value);
        NSInteger tag = opacity ? BONGO_MENU_OPACITY_25 + i : BONGO_MENU_SCALE_50 + i;
        add_item(submenu, target, title, tag, false);
    }
    [root setSubmenu:submenu]; [menu addItem:root];
    [submenu release]; [root release];
}

BongoMenuAction bongo_macos_context_menu(BongoPlatform *platform,
    const BongoMenuLabels *labels) {
    (void)platform;
    if (!labels) return BONGO_MENU_NONE;
    BongoMenuTarget *target = [[BongoMenuTarget alloc] init];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    add_item(menu, target, labels->preferences, BONGO_MENU_PREFERENCES, false);
    add_item(menu, target, labels->hide, BONGO_MENU_HIDE, false);
    [menu addItem:[NSMenuItem separatorItem]];
    add_item(menu, target, labels->pass_through, BONGO_MENU_PASS_THROUGH,
        labels->pass_through_checked);
    add_item(menu, target, labels->always_on_top, BONGO_MENU_ALWAYS_ON_TOP,
        labels->always_on_top_checked);
    add_scale_menu(menu, target, labels->window_size, false);
    add_scale_menu(menu, target, labels->opacity, true);
    [menu addItem:[NSMenuItem separatorItem]];
    add_item(menu, target, labels->restart, BONGO_MENU_RESTART, false);
    add_item(menu, target, labels->exit, BONGO_MENU_EXIT, false);
    [menu popUpMenuPositioningItem:nil atLocation:[NSEvent mouseLocation] inView:nil];
    BongoMenuAction result = (BongoMenuAction)[target selected];
    [menu release]; [target release]; return result;
}
#endif
