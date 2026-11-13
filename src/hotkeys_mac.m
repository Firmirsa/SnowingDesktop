// hotkeys_mac.m - global hotkeys with Carbon's RegisterEventHotKey, which
// works without the Accessibility permission.
#import <Carbon/Carbon.h>
#include "hotkeys_mac.h"

static void (*hotkey_cb)(int);

static OSStatus handler(EventHandlerCallRef next, EventRef ev, void *user) {
    (void)next; (void)user;
    EventHotKeyID hk;
    if (GetEventParameter(ev, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof hk, NULL, &hk) != noErr)
        return eventNotHandledErr;
    if (hotkey_cb) hotkey_cb((int)hk.id);
    return noErr;
}

void hotkeys_register(void (*callback)(int id)) {
    hotkey_cb = callback;
    EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
    InstallApplicationEventHandler(&handler, 1, &spec, NULL, NULL);
    struct { UInt32 key, id; } keys[] = {
        {kVK_ANSI_S, HK_PAUSE}, {kVK_ANSI_M, HK_MELT}, {kVK_ANSI_B, HK_BLIZZARD}, {kVK_ANSI_H, HK_HIDE},
    };
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        EventHotKeyRef ref;
        EventHotKeyID hid = {'snow', keys[i].id};
        RegisterEventHotKey(keys[i].key, controlKey | optionKey, hid, GetApplicationEventTarget(), 0, &ref);
    }
}
