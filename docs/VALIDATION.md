# Validation record — BaetaMP 1.3.0

Prepared 2026-10-19. Tests listed below were run locally; GitHub Actions has not yet run in the user's repository.

| Check | Result |
|---|---|
| Native GCC unit suite with ASan/UBSan | PASS |
| Native track-selection suite with ASan/UBSan | PASS |
| Unit/track suites built AArch64 and run through QEMU | PASS |
| ARM64 application build, `-Wall -Wextra -Werror` | PASS |
| Both historical folder names: data merge, conflicts, repeat/fresh install, recognized launcher retirement | PASS |
| Old/new watch-later keys and saved-menu path translation | PASS |
| Real libmpv: embedded audio/subtitle IDs, Off and external SRT | PASS |
| Localhost HTTP: UA/Referer, redirect base, failed-load 403, redacted marker | PASS |
| Release: private config exclusion, exact source/binary/logo, execute permissions, checksum and deterministic ZIP | PASS |
| PT/EN menu branding preview | PASS |

The 1.2 BaetaMediaPlayer core was physically tested by the user on RG DS+, including the track-selection menus. A 1080p film opened but played slowly; this is not a 1080p performance guarantee. BaetaMP 1.3.0 changes naming/artwork, directory migration and repository packaging; the newly named release and its migration await a physical-console check.

Host: Ubuntu 24.04 x86-64; GCC13 AArch64 toolchain and glibc 2.39 sysroot. Build/runtime dependencies were made available in a local SDK for these checks. The console diagnostic target remains Stock V1.0 (20260915), glibc 2.41 and libmpv 0.35.1. Native integration uses host libmpv 0.37 with the legacy loadfile interface. No compiler, SDK or host libraries are included in the repository package.
