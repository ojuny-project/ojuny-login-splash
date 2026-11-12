CC ?= cc
BUILD := build
ASSET_DIR ?= build/assets
VERSION := 0.1.0
DEJAVU_FONT ?=
FONT ?= $(DEJAVU_FONT)
EGL_VENDOR_DIR ?=
GL_DRIVER_DIR ?=
RPATH ?=

PKG_CONFIG ?= pkg-config
WAYLAND_CFLAGS := $(shell $(PKG_CONFIG) --cflags wayland-client wayland-egl 2>/dev/null)
WAYLAND_LIBS := $(shell $(PKG_CONFIG) --libs wayland-client 2>/dev/null)

CFLAGS ?= -O2 -g
CFLAGS += -std=c17 -Wall -Wextra -Wpedantic -D_GNU_SOURCE -DVERSION=\"$(VERSION)\"
CPPFLAGS += -DOJUNY_ASSET_DIR=\"$(ASSET_DIR)\" $(WAYLAND_CFLAGS)
ifneq ($(EGL_VENDOR_DIR),)
CPPFLAGS += -DOJUNY_EGL_VENDOR_DIR=\"$(EGL_VENDOR_DIR)\"
endif
ifneq ($(GL_DRIVER_DIR),)
CPPFLAGS += -DOJUNY_GL_DRIVER_DIR=\"$(GL_DRIVER_DIR)\"
endif
ifneq ($(RPATH),)
LDFLAGS += -Wl,-rpath,$(RPATH)
endif
LDLIBS += $(WAYLAND_LIBS) -ldl -lm

PROTOCOL_XML := protocols/wlr-layer-shell-unstable-v1.xml
PROTOCOL_HEADER := $(BUILD)/wlr-layer-shell-unstable-v1-client-protocol.h
PROTOCOL_SOURCE := $(BUILD)/wlr-layer-shell-unstable-v1-protocol.c

SOURCES := \
	src/main.c \
	src/args.c \
	src/assets.c \
	src/anim.c \
	src/runtime.c \
	src/util.c \
	src/display.c \
	src/render_cpu.c \
	src/render_gl.c \
	src/gl.c \
	src/xdg_stub.c

OBJECTS := $(patsubst src/%.c,$(BUILD)/%.o,$(SOURCES)) \
	$(BUILD)/wlr-layer-shell-unstable-v1-protocol.o

BINARY := $(BUILD)/ojuny-login-splash
TEST_ANIM := $(BUILD)/test_anim
TEST_ARGS := $(BUILD)/test_args

.PHONY: all test integration assets clean

all: $(BINARY)

$(BUILD):
	mkdir -p $(BUILD)

$(PROTOCOL_HEADER): $(PROTOCOL_XML) | $(BUILD)
	wayland-scanner client-header $< $@

$(PROTOCOL_SOURCE): $(PROTOCOL_XML) | $(BUILD)
	wayland-scanner private-code $< $@

$(BUILD)/%.o: src/%.c $(PROTOCOL_HEADER) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(BUILD) -c $< -o $@

$(BUILD)/wlr-layer-shell-unstable-v1-protocol.o: $(PROTOCOL_SOURCE) $(PROTOCOL_HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -I$(BUILD) -c $< -o $@

$(BINARY): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_ANIM): tests/test_anim.c src/anim.c src/util.c | $(BUILD)
	$(CC) $(CFLAGS) -Isrc tests/test_anim.c src/anim.c src/util.c -o $@ -lm

$(TEST_ARGS): tests/test_args.c src/args.c | $(BUILD)
	$(CC) $(CFLAGS) -Isrc tests/test_args.c src/args.c -o $@

test: $(TEST_ANIM) $(TEST_ARGS)
	./$(TEST_ANIM)
	./$(TEST_ARGS)

integration: $(BINARY) assets
	bash tests/integration.sh $(BINARY)

assets:
	python3 scripts/gen-assets.py \
		--logo assets/ojuny-logo.png \
		--out-dir $(ASSET_DIR) \
		--font "$(FONT)"

clean:
	rm -rf $(BUILD)
