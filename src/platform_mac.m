// platform_mac.m - macOS overlay: a transparent, click-through window above
// everything, the list of on-screen windows from CoreGraphics, and a menu bar
// item to control the snow.
//
// No special permission is needed: window *bounds* are readable without the
// Screen Recording permission (only window titles would require it).
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ServiceManagement/ServiceManagement.h>
#include <string.h>
#include <unistd.h>

#include "platform.h"
#include "raylib.h"

static NSWindow *overlay;
static AppSettings *settings;
static NSStatusItem *status_item;
static pid_t self_pid;
static CGRect display;       // main display, global CoreGraphics coordinates
static float refresh_timer;

// ---- menu bar item ---------------------------------------------------------
enum { TAG_LIGHT = 1, TAG_NORMAL, TAG_BLIZZARD, TAG_PAUSE, TAG_HIDE, TAG_LOGIN };
enum { HK_PAUSE = 1, HK_MELT, HK_BLIZZARD, HK_HIDE };

static bool french;
#define L(en, fr) (french ? (fr) : (en))

static bool login_enabled(void) {
    if (@available(macOS 13.0, *)) return [SMAppService mainAppService].status == SMAppServiceStatusEnabled;
    return false;
}

static void toggle_blizzard(void) {
    settings->intensity = settings->intensity >= 2.0f ? 1.0f : 2.8f;
    settings->paused = false;
}

@interface SnowMenu : NSObject <NSMenuDelegate>
@end

@implementation SnowMenu
- (void)light:(id)sender    { settings->intensity = 0.45f; settings->paused = false; }
- (void)normal:(id)sender   { settings->intensity = 1.0f;  settings->paused = false; }
- (void)blizzard:(id)sender { settings->intensity = 2.8f;  settings->paused = false; }
- (void)pause:(id)sender    { settings->paused = !settings->paused; }
- (void)hide:(id)sender     { settings->hidden = !settings->hidden; }
- (void)melt:(id)sender     { settings->melt_req = true; }
- (void)quit:(id)sender     { settings->quit_req = true; }
- (void)login:(id)sender {
    if (@available(macOS 13.0, *)) {
        NSError *err = nil;
        SMAppService *svc = [SMAppService mainAppService];
        if (svc.status == SMAppServiceStatusEnabled) [svc unregisterAndReturnError:&err];
        else [svc registerAndReturnError:&err];
        if (err) NSLog(@"Snow on Desktop: launch at login: %@", err);
    }
}
- (void)menuNeedsUpdate:(NSMenu *)menu {
    for (NSMenuItem *it in menu.itemArray) {
        bool on = false;
        switch (it.tag) {
            case TAG_LIGHT:    on = !settings->paused && settings->intensity < 0.7f; break;
            case TAG_NORMAL:   on = !settings->paused && settings->intensity >= 0.7f && settings->intensity < 2.0f; break;
            case TAG_BLIZZARD: on = !settings->paused && settings->intensity >= 2.0f; break;
            case TAG_PAUSE:    on = settings->paused; break;
            case TAG_HIDE:     on = settings->hidden; break;
            case TAG_LOGIN:    on = login_enabled(); break;
            default: continue;
        }
        it.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    }
}
@end

static SnowMenu *menu_target;

// key: shortcut shown in the menu, always with Control + Option (global hotkey)
static void add_item(NSMenu *m, NSString *title, SEL action, NSInteger tag, NSString *key) {
    NSMenuItem *it = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key ? key : @""];
    it.target = menu_target;
    it.tag = tag;
    if (key && ![key isEqualToString:@"q"])
        it.keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagOption;
    [m addItem:it];
    [it release];
}

static void build_menu(void) {
    NSString *lang = [NSLocale preferredLanguages].firstObject;
    french = lang && [lang hasPrefix:@"fr"];

    menu_target = [[SnowMenu alloc] init];
    status_item = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength] retain];
    status_item.button.title = @"❄︎";
    status_item.button.toolTip = @"Snow on Desktop";

    NSMenu *m = [[NSMenu alloc] initWithTitle:@"Snow"];
    m.delegate = menu_target;
    m.autoenablesItems = NO;
    add_item(m, L(@"Light snow", @"Neige légère"), @selector(light:), TAG_LIGHT, nil);
    add_item(m, L(@"Snow", @"Neige"), @selector(normal:), TAG_NORMAL, nil);
    add_item(m, L(@"Blizzard", @"Tempête de neige"), @selector(blizzard:), TAG_BLIZZARD, @"b");
    [m addItem:[NSMenuItem separatorItem]];
    add_item(m, L(@"Stop snowfall", @"Arrêter la neige"), @selector(pause:), TAG_PAUSE, @"s");
    add_item(m, L(@"Melt all snow", @"Faire fondre la neige"), @selector(melt:), 0, @"m");
    add_item(m, L(@"Hide", @"Masquer"), @selector(hide:), TAG_HIDE, @"h");
    [m addItem:[NSMenuItem separatorItem]];
    if (@available(macOS 13.0, *))
        add_item(m, L(@"Launch at login", @"Lancer au démarrage"), @selector(login:), TAG_LOGIN, nil);
    add_item(m, L(@"Quit Snow on Desktop", @"Quitter Snow on Desktop"), @selector(quit:), 0, @"q");
    status_item.menu = m;
    [m release];
}

// ---- global hotkeys (Control + Option + key) -------------------------------
// Carbon hotkeys work without the Accessibility permission.
static OSStatus hotkey_handler(EventHandlerCallRef next, EventRef ev, void *user) {
    (void)next; (void)user;
    EventHotKeyID hk;
    if (GetEventParameter(ev, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof hk, NULL, &hk) != noErr)
        return eventNotHandledErr;
    switch (hk.id) {
        case HK_PAUSE:    settings->paused = !settings->paused; break;
        case HK_MELT:     settings->melt_req = true; break;
        case HK_BLIZZARD: toggle_blizzard(); break;
        case HK_HIDE:     settings->hidden = !settings->hidden; break;
    }
    return noErr;
}

static void register_hotkeys(void) {
    EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
    InstallApplicationEventHandler(&hotkey_handler, 1, &spec, NULL, NULL);
    struct { UInt32 key, id; } keys[] = {
        {kVK_ANSI_S, HK_PAUSE}, {kVK_ANSI_M, HK_MELT}, {kVK_ANSI_B, HK_BLIZZARD}, {kVK_ANSI_H, HK_HIDE},
    };
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        EventHotKeyRef ref;
        EventHotKeyID hid = {'snow', keys[i].id};
        RegisterEventHotKey(keys[i].key, controlKey | optionKey, hid, GetApplicationEventTarget(), 0, &ref);
    }
}

// ---- overlay window --------------------------------------------------------
static void fit_overlay(ScreenInfo *out) {
    NSScreen *scr = [NSScreen screens].firstObject; // the one with the menu bar
    NSRect f = scr.frame;
    if (!NSEqualRects(overlay.frame, f)) [overlay setFrame:f display:YES];
    display = CGDisplayBounds(CGMainDisplayID());
    out->width = (float)f.size.width;
    out->height = (float)f.size.height;
    out->ground_y = (float)f.size.height; // snow drifts at the very bottom, over the Dock
}

unsigned int platform_window_flags(void) {
    return FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_TOPMOST |
           FLAG_WINDOW_MOUSE_PASSTHROUGH | FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_ALWAYS_RUN |
           FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
}

float platform_fixed_dt(void) { return 0.0f; }

ScreenInfo platform_init(void *native_window, AppSettings *s, int argc, char **argv) {
    (void)argc; (void)argv;
    settings = s;
    self_pid = getpid();
    overlay = (NSWindow *)native_window;

    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory]; // no Dock icon
    overlay.opaque = NO;
    overlay.backgroundColor = [NSColor clearColor];
    overlay.hasShadow = NO;
    overlay.ignoresMouseEvents = YES;
    overlay.level = NSStatusWindowLevel;
    overlay.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                                 NSWindowCollectionBehaviorStationary |
                                 NSWindowCollectionBehaviorIgnoresCycle |
                                 NSWindowCollectionBehaviorFullScreenAuxiliary;
    if ([overlay respondsToSelector:@selector(setSharingType:)]) overlay.sharingType = NSWindowSharingReadOnly;

    ScreenInfo info;
    fit_overlay(&info);
    [overlay orderFrontRegardless];
    build_menu();
    register_hotkeys();
    SetExitKey(KEY_NULL);
    return info;
}

void platform_frame(float dt, ScreenInfo *screen) {
    refresh_timer -= dt;
    if (refresh_timer <= 0.0f) { // resolution / arrangement changes
        refresh_timer = 2.0f;
        fit_overlay(screen);
    }
}

static bool num(CFDictionaryRef d, CFStringRef key, CFNumberType type, void *out) {
    CFNumberRef n = CFDictionaryGetValue(d, key);
    return n && CFNumberGetValue(n, type, out);
}

// System processes that own invisible layer-0 windows (Stage Manager strip
// hit areas, Dock, Control Center...). Snow must not land on those.
static bool is_system_owner(CFDictionaryRef d) {
    static const char *owners[] = {"WindowManager", "Dock", "Window Server", "Control Center",
                                   "ControlCenter", "Notification Center", "NotificationCenter",
                                   "SystemUIServer", "Spotlight", "screencaptureui", "TextInputMenuAgent"};
    CFStringRef name = CFDictionaryGetValue(d, kCGWindowOwnerName);
    if (!name) return false;
    char buf[128];
    if (!CFStringGetCString(name, buf, sizeof buf, kCFStringEncodingUTF8)) return false;
    for (size_t i = 0; i < sizeof owners / sizeof owners[0]; i++)
        if (strcmp(buf, owners[i]) == 0) return true;
    return false;
}

int platform_windows(SnowWindow *out, int max) {
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (!list) return 0;
    int n = 0;
    CFIndex count = CFArrayGetCount(list);
    for (CFIndex i = 0; i < count && n < max; i++) { // already front-to-back
        CFDictionaryRef d = CFArrayGetValueAtIndex(list, i);
        int layer = -1, pid = 0;
        double alpha = 1.0;
        uint32_t wid = 0;
        if (!num(d, kCGWindowLayer, kCFNumberIntType, &layer) || layer != 0) continue;
        if (num(d, kCGWindowOwnerPID, kCFNumberIntType, &pid) && pid == self_pid) continue;
        if (is_system_owner(d)) continue;
        if (num(d, kCGWindowAlpha, kCFNumberDoubleType, &alpha) && alpha < 0.2) continue;
        if (!num(d, kCGWindowNumber, kCFNumberSInt32Type, &wid)) continue;
        CFDictionaryRef b = CFDictionaryGetValue(d, kCGWindowBounds);
        CGRect r;
        if (!b || !CGRectMakeWithDictionaryRepresentation(b, &r)) continue;
        // mostly off-screen windows (e.g. Stage Manager parks inactive windows
        // off the left edge): only the sliver that sticks out would catch snow
        CGRect vis = CGRectIntersection(r, display);
        if (CGRectIsNull(vis) || vis.size.width * vis.size.height < 0.3 * r.size.width * r.size.height) continue;
        out[n++] = (SnowWindow){wid, (float)(r.origin.x - display.origin.x), (float)(r.origin.y - display.origin.y),
                                (float)r.size.width, (float)r.size.height};
    }
    CFRelease(list);
    return n;
}

bool platform_cursor(float *x, float *y) {
    CGEventRef e = CGEventCreate(NULL);
    if (!e) return false;
    CGPoint p = CGEventGetLocation(e);
    CFRelease(e);
    *x = (float)(p.x - display.origin.x);
    *y = (float)(p.y - display.origin.y);
    return true;
}

void platform_draw_background(void) {}
void platform_draw_foreground(void) {}
void platform_after_frame(void) {}

void platform_shutdown(void) {
    if (status_item) [[NSStatusBar systemStatusBar] removeStatusItem:status_item];
}
