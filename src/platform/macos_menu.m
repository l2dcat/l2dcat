#include "macos_internal.h"

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include <stdio.h>

@interface L2DCatMenuTarget : NSObject { NSInteger selected_; }
- (void)choose:(id)sender;
- (NSInteger)selected;
@end

@implementation L2DCatMenuTarget
- (void)choose:(id)sender { selected_ = [sender tag]; }
- (NSInteger)selected { return selected_; }
@end

static NSString *text(const char *value) {
    return value ? [NSString stringWithUTF8String:value] : @"";
}

static NSMenuItem *add_item(NSMenu *menu, L2DCatMenuTarget *target,
    const char *label, NSInteger tag, bool checked) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:text(label)
        action:@selector(choose:) keyEquivalent:@""];
    [item setTarget:target]; [item setTag:tag];
    [item setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
    [menu addItem:item]; [item release]; return item;
}

static void add_scale_menu(NSMenu *menu, L2DCatMenuTarget *target,
    const char *label, bool opacity) {
    NSMenuItem *root = [[NSMenuItem alloc] initWithTitle:text(label)
        action:nil keyEquivalent:@""];
    NSMenu *submenu = [[NSMenu alloc] initWithTitle:text(label)];
    const int values[] = {10,20,30,40,50,60,70,80,90,100};
    int count = opacity ? 10 : 16;
    for (int i = 0; i < count; ++i) {
        int value = opacity ? values[i] : 50 + i * 10;
        char title[16]; snprintf(title, sizeof(title), "%d%%", value);
        NSInteger tag = opacity ? L2DCAT_MENU_OPACITY_10 + i : L2DCAT_MENU_SCALE_50 + i;
        add_item(submenu, target, title, tag, false);
    }
    [root setSubmenu:submenu]; [menu addItem:root];
    [submenu release]; [root release];
}

L2DCatMenuAction l2dcat_macos_context_menu(L2DCatPlatform *platform,
    const L2DCatMenuLabels *labels) {
    (void)platform;
    if (!labels) return L2DCAT_MENU_NONE;
    L2DCatMenuTarget *target = [[L2DCatMenuTarget alloc] init];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    add_item(menu, target, labels->preferences, L2DCAT_MENU_PREFERENCES, false);
    add_item(menu, target, labels->hide, L2DCAT_MENU_HIDE, false);
    [menu addItem:[NSMenuItem separatorItem]];
    add_item(menu, target, labels->pass_through, L2DCAT_MENU_PASS_THROUGH,
        labels->pass_through_checked);
    add_item(menu, target, labels->always_on_top, L2DCAT_MENU_ALWAYS_ON_TOP,
        labels->always_on_top_checked);
    add_scale_menu(menu, target, labels->window_size, false);
    add_scale_menu(menu, target, labels->opacity, true);
    NSMenuItem *modelRoot = [[NSMenuItem alloc] initWithTitle:text(labels->model)
        action:nil keyEquivalent:@""];
    NSMenu *models = [[NSMenu alloc] initWithTitle:text(labels->model)];
    for (size_t i = 0; i < labels->model_count; ++i)
        add_item(models, target, labels->model_names[i], L2DCAT_MENU_MODEL_FIRST + i,
            i == labels->current_model);
    [modelRoot setSubmenu:models]; [menu addItem:modelRoot];
    [models release]; [modelRoot release];
    [menu addItem:[NSMenuItem separatorItem]];
    add_item(menu, target, labels->exit, L2DCAT_MENU_EXIT, false);
    [menu popUpMenuPositioningItem:nil atLocation:[NSEvent mouseLocation] inView:nil];
    L2DCatMenuAction result = (L2DCatMenuAction)[target selected];
    [menu release]; [target release]; return result;
}
#endif
