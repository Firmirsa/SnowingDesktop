# Snow on Desktop
#
#   make            build build/Snow on Desktop.app (macOS) or build/snow-demo (Linux)
#   make run        build and launch
#   make demo       Linux/macOS: build the scripted fake-desktop demo
#   make zip        macOS: zipped .app for a GitHub release
#   V=1             show the raw compiler commands instead of the pretty build
#
# raylib is fetched and built automatically into vendor/ (no Homebrew needed).

RAYLIB_VERSION := 5.5
RAYLIB_DIR     := vendor/raylib
RAYLIB_LIB     := $(RAYLIB_DIR)/src/libraylib.a
APP_NAME       := Snow on Desktop
BUNDLE         := build/$(APP_NAME).app
VERSION        ?= 0.1.0

CC      ?= cc
CFLAGS  += -O2 -std=c11 -Wall -Wextra -Wno-missing-field-initializers -I$(RAYLIB_DIR)/src
CORE    := src/main.c src/snow.c src/render.c

UNAME := $(shell uname -s)

ifeq ($(UNAME),Darwin)
  ARCHS        ?= -arch arm64 -arch x86_64
  MACOS_MIN    := -mmacosx-version-min=11.0
  CFLAGS       += $(ARCHS) $(MACOS_MIN)
  FRAMEWORKS   := -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL \
                  -framework CoreGraphics -framework ServiceManagement
  RAYLIB_FLAGS := CUSTOM_CFLAGS="$(ARCHS) $(MACOS_MIN) -DGL_SILENCE_DEPRECATION"
  DEMO_LIBS    := $(FRAMEWORKS)
  default: app
else
  DEMO_LIBS    := -lGL -lm -lpthread -ldl -lrt -lX11
  RAYLIB_FLAGS :=
  default: demo
endif

.PHONY: default app run demo zip clean distclean raw-raylib-src raw-raylib raw-app raw-run raw-zip raw-demo

# Public targets show a pretty build (scripts/build.sh). V=1 shows raw commands.
ifeq ($(V),1)
app: raw-app
run: raw-run
zip: raw-zip
demo: raw-demo
else
app run zip demo:
	@bash scripts/build.sh $@
endif

# ---- raw targets (called by scripts/build.sh) -------------------------------
$(RAYLIB_DIR)/src/raylib.h:
	git clone --progress --depth 1 --branch $(RAYLIB_VERSION) https://github.com/raysan5/raylib.git $(RAYLIB_DIR)

$(RAYLIB_LIB): $(RAYLIB_DIR)/src/raylib.h
	$(MAKE) -C $(RAYLIB_DIR)/src PLATFORM=PLATFORM_DESKTOP $(RAYLIB_FLAGS) -j4

raw-raylib-src: $(RAYLIB_DIR)/src/raylib.h
raw-raylib: $(RAYLIB_LIB)

build:
	mkdir -p build

# macOS app
build/SnowOnDesktop: $(CORE) src/platform_mac.m src/*.h $(RAYLIB_LIB) | build
	$(CC) $(CFLAGS) -fno-objc-arc $(CORE) src/platform_mac.m $(RAYLIB_LIB) $(FRAMEWORKS) -o $@

raw-app: build/SnowOnDesktop
	rm -rf "$(BUNDLE)"
	mkdir -p "$(BUNDLE)/Contents/MacOS" "$(BUNDLE)/Contents/Resources"
	cp build/SnowOnDesktop "$(BUNDLE)/Contents/MacOS/SnowOnDesktop"
	sed 's/@VERSION@/$(VERSION)/g' macos/Info.plist > "$(BUNDLE)/Contents/Info.plist"
	if [ -f macos/AppIcon.icns ]; then cp macos/AppIcon.icns "$(BUNDLE)/Contents/Resources/"; fi
	codesign --force --deep --sign - "$(BUNDLE)"

raw-run: raw-app
	open "$(BUNDLE)"

raw-zip: raw-app
	rm -f "build/SnowOnDesktop-$(VERSION)-macOS.zip"
	cd build && ditto -c -k --keepParent "$(APP_NAME).app" "SnowOnDesktop-$(VERSION)-macOS.zip"

# scripted demo (fake desktop)
build/snow-demo: $(CORE) src/platform_demo.c src/*.h $(RAYLIB_LIB) | build
	$(CC) $(CFLAGS) $(CORE) src/platform_demo.c $(RAYLIB_LIB) $(DEMO_LIBS) -lm -o $@

raw-demo: build/snow-demo

clean:
	rm -rf build

distclean: clean
	rm -rf vendor
