# ❄️ User guide — Snow on Desktop

[Version française](GUIDE.fr.md)

## Contents
1. [Install](#1-install)
2. [Launch](#2-launch)
3. [The ❄ menu](#3-the--menu)
4. [Playing with the snow](#4-playing-with-the-snow)
5. [Launch at login](#5-launch-at-login)
6. [Quit, update, uninstall](#6-quit-update-uninstall)
7. [Troubleshooting](#7-troubleshooting)
8. [For developers](#8-for-developers)

---

## 1. Install

### Option A: download the app (easiest)
1. Go to [Releases](https://github.com/Firmirsa/SnowingDesktop/releases/latest) and download `SnowOnDesktop-x.y.z-macOS.zip`.
2. Double-click the zip, then drag **Snow on Desktop.app** into **Applications**.
3. **First launch**: the app isn't notarized by Apple (that costs $99/year), so macOS is cautious.
   - **Right-click the app → Open → Open**.
   - If macOS still blocks it: **System Settings → Privacy & Security**, scroll down, click **Open Anyway**.
   - Or in Terminal:
     ```bash
     xattr -dr com.apple.quarantine "/Applications/Snow on Desktop.app"
     ```

You only need to do this once.

### Option B: build from source
You only need Apple's command line tools (once):
```bash
xcode-select --install
```
Then:
```bash
git clone https://github.com/Firmirsa/SnowingDesktop.git
cd SnowingDesktop
make run
```
`make` downloads and builds the graphics library (raylib) by itself. The first build takes about 30 seconds, later ones about 3 seconds.

**Requirements**: macOS 11 Big Sur or later, Apple Silicon or Intel Mac.

---

## 2. Launch

- **Downloaded app**: open **Snow on Desktop** from Applications, Launchpad or Spotlight (`⌘ Space`, type "Snow").
- **From source**: `make run` in the project folder.

Then:
- a **❄ appears in the menu bar**, top right, next to the clock;
- snow starts falling and piling up on your windows;
- **there is no Dock icon**: that's expected, everything goes through the ❄.

The app asks for **no permissions**. It only reads where windows are, never their content or titles.

---

## 3. The ❄ menu

Click the **❄ in the menu bar**. The menu follows your Mac's language (English or French).

| Item | What it does |
|---|---|
| **Light snow** | A few flakes, calm mood |
| **Snow** | Normal snowfall (default) |
| **Blizzard** | Lots of snow, piles up fast |
| **Stop snowfall** | No new flakes; snow already on your windows stays. Click again to resume |
| **Melt all snow** | Every pile melts away in a few seconds |
| **Hide** | Hides everything without quitting. Click again to show |
| **Launch at login** | Starts the app when you turn on your Mac (macOS 13+) |
| **Quit Snow on Desktop** | Closes the app (`⌘Q` while the menu is open) |

A ✓ shows the active setting.

---

## 4. Playing with the snow

| Action | What happens |
|---|---|
| Let it snow | Snow piles up on top of every window, spills over the rounded corners, and drifts at the bottom of the screen |
| **Move a window** gently | The snow stays on it and travels along |
| **Shake a window** or stop it suddenly | The snow keeps its momentum and flies off |
| **Close a window** | Its snow crumbles and falls on the windows below |
| **Swipe the cursor** quickly through a pile | You sweep it away in a spray |
| **Move the cursor** through falling flakes | The air pushes them around |
| Put a window on top of another | Hidden snow stays hidden and comes back when uncovered |
| Switch desktops (Spaces) | Your snow waits for you, it doesn't fall |

The snow **never blocks your clicks**: you click right through it.

💡 **Tip for a video**: set **Blizzard** for 1 or 2 minutes, then shake a window.

---

## 5. Launch at login

❄ menu → **Launch at login** (a ✓ appears). Click again to turn it off.
You can also manage it in **System Settings → General → Login Items**.

> Available on macOS 13 Ventura and later. Works best with the app in Applications.

---

## 6. Quit, update, uninstall

**Quit**: ❄ menu → **Quit Snow on Desktop**.

**Update**
- Downloaded app: quit it, download the new version from Releases, replace the old one in Applications.
- From source:
  ```bash
  git pull && make run
  ```
  Quit the old version first, or you'll get two snowfalls on top of each other.

**Uninstall**
1. ❄ menu → uncheck **Launch at login** if it's on.
2. ❄ menu → **Quit**.
3. Move **Snow on Desktop** from Applications to the Trash.

The app leaves no other files on your Mac.

---

## 7. Troubleshooting

**I can't see the ❄ in the menu bar**
On MacBooks with a notch, macOS hides menu bar icons when there are too many: they end up behind the notch. Remove some icons (hold `⌘` and drag them out), or use an app like Ice or Bartender.

**"Snow on Desktop can't be opened because the developer cannot be verified"**
See [First launch](#option-a-download-the-app-easiest): right-click → Open.

**Two snowfalls at once**
The app was launched twice. Quit both from the ❄, or in Terminal:
```bash
killall SnowOnDesktop
```

**Snow floats next to the Stage Manager icons**
Known issue: macOS keeps invisible windows there. It's harmless and it's on the list to fix.

**Snow only on one screen**
Expected for now: only the main display (the one with the menu bar) is supported. Multiple monitors are planned.

**`make` fails**
- Install the command line tools: `xcode-select --install`.
- Check the end of `build/build.log`, or rerun with raw output: `make V=1`.
- To rebuild from scratch: `make distclean && make`.

**My Mac gets warm**
Switch to **Light snow** or **Stop snowfall**, and open an issue on GitHub with your Mac model.

---

## 8. For developers

| Command | Effect |
|---|---|
| `make` | builds `build/Snow on Desktop.app` |
| `make run` | builds and launches |
| `make zip` | zip ready for a Release |
| `make demo` | scripted fake desktop (also works on Linux) |
| `./build/snow-demo --record demo.mp4` | renders the demo video (needs `ffmpeg`) |
| `make V=1` | raw compiler commands |
| `make clean` | removes `build/` |
| `make distclean` | also removes raylib (`vendor/`) |

Code layout: see *How it works* in the [README](../README.md#-how-it-works).

**Publishing a version**: push a tag, and GitHub Actions builds the app and creates the Release.
```bash
git tag v0.2.0 && git push origin v0.2.0
```
