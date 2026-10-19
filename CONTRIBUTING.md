# Contributing

Please include the device model, firmware, app version, PT/EN setting and a minimal description of the failure. Remove private media URLs, cookies, authorization headers and personal favorites before sharing logs.

Use generated media or example.org/localhost fixtures. Do not contribute personal IPTV subscriptions or media files. Preserve the tested exit-42 restart behavior unless a replacement has been verified on the target hardware.

Run `make test`, ARM64 build/tests and the relevant media/network/package checks for your change. Native and emulated tests do not establish real-console performance. Keep Portuguese and English application-owned text in sync in `src/translations.h`; file/channel titles stay unchanged. Preserve third-party notices and GPL-3.0-or-later licensing.
