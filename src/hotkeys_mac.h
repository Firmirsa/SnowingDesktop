// hotkeys_mac.h - global keyboard shortcuts (Control + Option + key).
// Kept apart from raylib: Carbon headers clash with raylib names.
#ifndef HOTKEYS_MAC_H
#define HOTKEYS_MAC_H

enum { HK_PAUSE = 1, HK_MELT, HK_BLIZZARD, HK_HIDE };

// Registers Ctrl+Opt+S (pause), M (melt), B (blizzard), H (hide).
void hotkeys_register(void (*callback)(int id));

#endif
