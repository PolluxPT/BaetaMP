# Third-party notices and project provenance

- **BaetaMP application and launcher**: GPL-3.0-or-later, continuing the license of BaetaMediaPlayer. Full GPL-3.0 text is in LICENSE.
- **SDL ABI shim** (`src/sdl-api.h`): inherited from the RG DS+ ScummVM package, GPL-3.0-or-later, as recorded by the previous project. The native app uses SDL dynamically.
- **mpv public headers** (`src/mpv/`): notices of the mpv developers retained in each header; the public client header uses its permissive copyright/permission notice. A copy of that notice is in `licenses/mpv-headers.txt`. These are headers, not the mpv engine.
- **curl headers** (`src/curl/`): Daniel Stenberg and contributors; copyright/SPDX notices retained. Curl license text is in `licenses/curl.txt` and upstream at https://curl.se/docs/copyright.html.
- **DejaVu Sans font**: original font and notice at `APPS/BaetaMP/fonts/`. See `LICENSE.txt` there.
- **Project logo**: supplied by Filipe Baeta. The original PNG is preserved at `APPS/Imgs/BaetaMP.png`; see `docs/ASSETS.md` for its checksum.
- **Sample film/SRT**: synthetic demonstration distributed with the preceding BaetaMediaPlayer project; no third-party film or IPTV subscription is bundled.

SDL2, SDL2_ttf, libmpv, libcurl, FFmpeg and firmware components are runtime dependencies, not redistributed runtime libraries in this package. Their own licenses remain applicable to their distributions. This project is independent of ANBERNIC, mpv, VLC, Kodi and commercial streaming services; naming them describes hardware/dependencies/features only.
