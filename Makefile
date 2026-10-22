# SPDX-License-Identifier: GPL-3.0-or-later
CC ?= cc
CROSS_CC ?= aarch64-linux-gnu-gcc
QEMU ?= qemu-aarch64
QEMU_LD_PREFIX ?= /usr/aarch64-linux-gnu
CPPFLAGS ?= -Isrc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations
CROSS_FLAGS ?=
LDLIBS = -ldl -lm -pthread
HEADERS = $(wildcard src/*.h src/mpv/*.h src/curl/*.h)
.PHONY: all arm64 native test test-arm64 test-media test-http test-package release clean
all: arm64
arm64: APPS/BaetaMP/bin/baetamp
APPS/BaetaMP/bin/baetamp: src/player.c $(HEADERS)
	mkdir -p $(@D)
	$(CROSS_CC) $(CPPFLAGS) $(CFLAGS) $(CROSS_FLAGS) $< $(LDLIBS) -o $@
native: build/baetamp-native
build/baetamp-native: src/player.c $(HEADERS)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
build/unit-native: tests/unit.c src/player.c $(HEADERS)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fsanitize=address,undefined $< $(LDLIBS) -o $@
build/tracks-native: tests/tracks.c src/player.c $(HEADERS)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fsanitize=address,undefined $< $(LDLIBS) -o $@
test: build/unit-native build/tracks-native
	ASAN_OPTIONS=detect_leaks=0 ./build/unit-native
	ASAN_OPTIONS=detect_leaks=0 ./build/tracks-native
	python3 tests/migration.py
	python3 tests/volume_restore.py
	bash -n APPS/BaetaMP.sh
build/unit-arm64: tests/unit.c src/player.c $(HEADERS)
	mkdir -p build
	$(CROSS_CC) $(CPPFLAGS) $(CFLAGS) $(CROSS_FLAGS) $< $(LDLIBS) -o $@
build/tracks-arm64: tests/tracks.c src/player.c $(HEADERS)
	mkdir -p build
	$(CROSS_CC) $(CPPFLAGS) $(CFLAGS) $(CROSS_FLAGS) $< $(LDLIBS) -o $@
test-arm64: build/unit-arm64 build/tracks-arm64
	$(QEMU) -L $(QEMU_LD_PREFIX) ./build/unit-arm64
	$(QEMU) -L $(QEMU_LD_PREFIX) ./build/tracks-arm64
build/tracks-integration-native: tests/tracks_integration.c src/player.c $(HEADERS)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fsanitize=address,undefined $< $(LDLIBS) -o $@
build/http-native: tests/http.c src/player.c $(HEADERS)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
test-media: build/tracks-integration-native
	python3 tools/test_media.py
test-http: native build/http-native
	python3 tests/http_integration.py
release:
	python3 tools/package_release.py
test-package:
	python3 tests/package.py
clean:
	rm -rf build dist
# The shipped ARM64 binary is deliberately retained by clean.
