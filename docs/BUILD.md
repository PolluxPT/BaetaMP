# Build and test / Compilar e testar

The release includes its matching C source, public headers, launcher, tests and packaging script. The application dynamically loads firmware libraries; building requires their ABI headers (included), not replacement copies of firmware libraries.

## Ubuntu 24.04 x86-64

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends make gcc gcc-aarch64-linux-gnu libc6-dev-arm64-cross qemu-user python3 ffmpeg libmpv2 libsdl2-2.0-0 libsdl2-ttf-2.0-0 libcurl4t64
make test
make arm64 test-arm64
make test-media test-http
make release test-package
```

Output: `APPS/BaetaMP/bin/baetamp`, `dist/BaetaMP-v1.3.0-RGDSPlus.zip` and `dist/SHA256SUMS.txt`. The repo already ships a compiled ARM64 program for users who do not compile. To force a fresh ARM64 rebuild, run `make -B arm64`.

- `make test`: ASan/UBSan unit/track suites, disposable launcher migration tests and Bash syntax.
- `make test-arm64`: the unit and track suites built for AArch64 and executed through QEMU, with `/usr/aarch64-linux-gnu` sysroot by default.
- `make test-media`: creates an owned temporary MKV with two audio/two subtitle tracks and an external SRT; verifies actual track selection through libmpv with null video/audio.
- `make test-http`: owned localhost HTTP server checks headers, redirects, relative playlist paths and redacted 403 errors; uses offscreen Mesa rendering.
- `make release`: packages explicit public files only, including exact corresponding source. Fixed ZIP timestamps/order make repeated packaging of the same bytes reproducible. Compiler versions can still change binary bytes.
- `make test-package`: checks binary architecture, executable modes, logo/source/binary byte identity, checksums, reproducibility and exclusion of a deliberately created private config marker.

The launcher migration test runs only on x86-64 hosts and stops at the architecture guard before hardware access. Never run that test on the console.

## Alternative cross compiler

Override `CROSS_CC` and `CROSS_FLAGS` in Make; set `QEMU` and `QEMU_LD_PREFIX` for an extracted SDK/sysroot. Runtime on the device still targets the diagnosed stock firmware. `make clean` deletes local build/dist outputs, retaining the shipped program.

## Native UI preview

```sh
make native
mkdir -p build/previews
cd build/previews
SDL_VIDEODRIVER=dummy ../baetamp-native "$(pwd)/../../APPS/BaetaMP" --preview
```

Requires native SDL2/TTF/libmpv. It renders temporary PT/EN PPM menus; this is layout inspection, not a performance test. Use a disposable app directory if you keep personal config in the repo tree: preview mode can write settings when `config/` exists.

## Runtime architecture

The firmware uses SDL/Wayland plus libmpv rendering. On video completion the launcher reopens the UI in a fresh process via exit 42, preserving the previous menu; this behavior fixes a device-specific return-to-menu failure and is retained. Backlight/touch and uinput safeguards remain in the launcher/player. The program does not install system files.

The headers under `src/mpv` come from the public libmpv API; the diagnosed engine is 0.35.1. No mpv engine, FFmpeg or SDL runtime binary is distributed here. Build instructions do not imply compatibility with every future libmpv version: notably, loadfile option syntax targets the diagnosed engine.
