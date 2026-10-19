<img src="APPS/Imgs/BaetaMP.png" width="210" alt="BaetaMP — Media Player">

[⬇ Descarregar / Download](https://github.com/PolluxPT/BaetaMP/releases/latest)

# BaetaMP · Media Player for RGDS+

[Português](README.md) · [Build instructions](docs/BUILD.md)

A standalone film, subtitle and IPTV player for the **Anbernic RG DS+ running stock Linux**. Maintained by **Filipe Baeta**. Previously called BaetaMediaPlayer; **MP means Media Player**.

## Install

Download **BaetaMP-v1.3.0-RGDSPlus.zip** from Releases. Extract it and copy the **contents of APPS** to `Roms/APPS` on the Games/TF2 card. Open BaetaMP in APPS. No compilation is required. Use the attached release ZIP for installation; **Code → Download ZIP** is the development project.

The official logo is `APPS/Imgs/BaetaMP.png`, launcher `APPS/BaetaMP.sh`, program/data folder `APPS/BaetaMP/`.

**Upgrade:** keep old folders until the first BaetaMP launch. The launcher merges `BaetaMediaPlayer` and `MediaPlayer_RGDSPlus` user data without overwriting destination files. Conflicts/old files are retained in `BaetaMP/config/migracao-*`; recognized old launchers are archived. Saved film positions from either old folder name remain recognized.

## Features

- Local films and external subtitles; synthetic 720p demonstration included.
- Local/remote M3U/M3U8 playlists and direct media URLs, including radio streams.
- Combined A–Z favorites with confirmed removal.
- Audio/subtitle lists showing available language/title metadata and current selection; subtitles Off.
- Saved local-film positions with Continue / Play from Beginning.
- Persistent PT/EN interface, original/stretch/crop image modes and physical volume controls.
- Lower-screen toggle while playing on the upper screen, screen-saver inhibition request, confirmed exit and one startup log.

## Interface

![BaetaMP interface](docs/screenshots/menu.png)

## Controls

D-pad + A or lower-screen touch navigates menus. During playback, A pauses and B stops; B inside a track list only goes back. Left/right and L/R seek ten seconds in films; L/R change channels in network lists/favorites. X opens audio and Y opens subtitles, following the physical buttons confirmed on this console. START opens Options for upper-screen playback, SELECT toggles the lower backlight, VOL+/− adjusts volume.

Track selection is also available through **Subtitles / Options → Choose audio / Choose subtitles**. Choose PT/EN in **Options → Language**.

## Target and limitations

Diagnosed target: **RG DS+ Stock V1.0 (20260915), ARM64 Linux/Wayland**, using firmware SDL2/TTF, libmpv 0.35.1 and libcurl. These runtime libraries are not bundled or replaced. Other handhelds/firmwares are unverified.

**720p is the intended target.** Performance depends on codec, bitrate and decoding support. A 1080p file opened in user testing but played slowly. This is not a DRM/login client for commercial services. Stream availability depends on each URL/service; RTP worked in user tests, SIC/TVI did not. Burned-in subtitles cannot be selected as tracks. No IPTV playlists, account details or credentials are included.

## Build and license

See [BUILD.md](docs/BUILD.md) and [VALIDATION.md](docs/VALIDATION.md). `make test` runs native checks, `make arm64 test-arm64` builds/tests ARM64, `make test-media test-http` checks media/network integration, and `make release test-package` produces and validates the release ZIP.

Application code: **GPL-3.0-or-later**. See LICENSE and THIRD_PARTY_NOTICES.md. Original third-party notices are retained. The supplied logo is preserved in `APPS/Imgs/BaetaMP.png`.
