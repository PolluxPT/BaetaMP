#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -u
umask 077
parent=$(CDPATH= cd -P "$(dirname "$0")" 2>/dev/null && pwd) || exit 1
app="$parent/BaetaMP"
export RGDS_LEGACY_APP="$parent/BaetaMediaPlayer"
export RGDS_LEGACY_APP_2="$parent/MediaPlayer_RGDSPlus"
merge_data(){
    local from="$1" into="$2" entry target
    mkdir -p "$into" || return 1
    for entry in "$from"/*; do
        target="$into/${entry##*/}"
        if [ -d "$entry" ] && [ ! -L "$entry" ] && [ -d "$target" ] && [ ! -L "$target" ]; then
            merge_data "$entry" "$target" || return 1
        elif [ ! -e "$target" ] && [ ! -L "$target" ]; then
            mv "$entry" "$target" || return 1
        fi
    done
}
shopt -s nullglob dotglob
mkdir -p "$app/config/retomar" "$app/config/cache" "$app/Filmes" "$app/IPTV" || exit 1
migration_count=0
for legacy_app in "$RGDS_LEGACY_APP" "$RGDS_LEGACY_APP_2"; do
    if [ -d "$legacy_app" ] && [ ! -L "$legacy_app" ]; then
        for folder in config Filmes IPTV; do
            if [ -d "$legacy_app/$folder" ] && [ ! -L "$legacy_app/$folder" ]; then
                merge_data "$legacy_app/$folder" "$app/$folder" || exit 1
            fi
        done
        migration_backup="$app/config/migracao-${legacy_app##*/}"
        if [ -e "$migration_backup" ]; then migration_backup="$migration_backup-$$"; fi
        mv "$legacy_app" "$migration_backup" || exit 1
        migration_count=$((migration_count+1))
    fi
done
exec > "$app/config/Arranque.txt" 2>&1
printf 'BaetaMP for RGDS+ 1.3.2\nData: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')"
# Only recognized old app launchers are retired. Other apps are left alone.
for old in BaetaMediaPlayer MediaPlayer; do
    script="$parent/$old.sh"
    if [ -f "$script" ] && grep -q 'RGDS_RESTART_AFTER_VIDEO' "$script" && grep -Eq 'BaetaMediaPlayer|MediaPlayer_RGDSPlus' "$script"; then
        retired="$app/config/lancadores-anteriores"
        mkdir -p "$retired" || exit 1
        target="$retired/$old.sh.txt"
        if [ -e "$target" ]; then target="$target-$$"; fi
        mv "$script" "$target" || exit 1
        for icon in "$parent/$old.png" "$parent/Imgs/$old.png"; do
            if [ -f "$icon" ]; then
                target="$retired/${old}-$(basename "$(dirname "$icon")").png"
                if [ -e "$target" ]; then target="$target-$$"; fi
                mv "$icon" "$target" || exit 1
            fi
        done
    fi
done
printf 'Pastas antigas migradas: %s\n' "$migration_count"
uname -smr
if [ "$(uname -m)" != aarch64 ]; then printf 'Requer Linux ARM64.\n'; exit 1; fi
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/var/run}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export SDL_VIDEODRIVER=wayland
export SDL_RENDER_DRIVER=software
export SDL_AUDIODRIVER=alsa
export SDL_VIDEO_ALLOW_SCREENSAVER=1
export SDL_MOUSE_TOUCH_EVENTS=0
export SDL_TOUCH_MOUSE_EVENTS=0
export XDG_CONFIG_HOME="$app/config"
export XDG_CACHE_HOME="$app/config/cache"
export LD_LIBRARY_PATH=/usr/lib:/lib:/mnt/vendor/lib
export RGDS_RESTART_AFTER_VIDEO=1
unset LD_PRELOAD
if [ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]; then printf 'Wayland indisponivel.\n'; exit 1; fi
if [ ! -f "$app/bin/baetamp" ]; then printf 'Falta o executavel.\n'; exit 1; fi
chmod +x "$app/bin/baetamp" || exit 1
if [ -x /lib/ld-linux-aarch64.so.1 ]; then
    /lib/ld-linux-aarch64.so.1 --list "$app/bin/baetamp" || exit 1
fi
printf '\n=== Inibicao de inatividade anunciada pelo compositor ===\n'
export RGDS_IDLE_INHIBIT_PROTOCOL=0
if command -v timeout >/dev/null && command -v wayland-info >/dev/null; then
    protocols=$(timeout -k 1 4 wayland-info 2>&1 | sed -n '/idle_inhibit/p; /screensaver/p; /weston/p')
    printf '%s\n' "$protocols"
    case "$protocols" in *zwp_idle_inhibit_manager*) export RGDS_IDLE_INHIBIT_PROTOCOL=1;; esac
fi
printf 'Idle-inhibit anunciado: %s\n' "$RGDS_IDLE_INHIBIT_PROTOCOL"
saved_volume=''
saved_touch=''
saved_upper=''
saved_lower=''
for var in volume touch upper lower; do
    case "$var" in
        volume) node=/sys/class/anbernic_misc/openbor_volume;;
        touch) node=/sys/class/anbernic_misc/tpctrl;;
        upper) node=/sys/class/backlight/backlight1/brightness;;
        lower) node=/sys/class/backlight/backlight/brightness;;
    esac
    if [ -r "$node" ]; then value=$(cat "$node"); case "$value" in ''|*[!0-9]*) :;; *) printf -v "saved_$var" '%s' "$value";; esac; fi
done
restore_volume() {
    [ -z "$saved_volume" ] || printf '%s\n' "$saved_volume" > /sys/class/anbernic_misc/openbor_volume
}
restore() {
    [ -z "$saved_touch" ] || printf '%s\n' "$saved_touch" > /sys/class/anbernic_misc/tpctrl
    [ -z "$saved_upper" ] || printf '%s\n' "$saved_upper" > /sys/class/backlight/backlight1/brightness
    [ -z "$saved_lower" ] || printf '%s\n' "$saved_lower" > /sys/class/backlight/backlight/brightness
}
child=''
guardian=''
cleanup(){
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi
    restore
    restore_volume
    if [ -n "$guardian" ]; then kill -TERM "$guardian" 2>/dev/null || :; wait "$guardian" 2>/dev/null || :; fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
(
    trap '' HUP
    while kill -0 "$$" 2>/dev/null; do sleep 1; done
    restore
    restore_volume
) &
guardian=$!
if [ "$saved_touch" = 0 ] || [ "$saved_touch" = 1 ]; then printf '0\n' > /sys/class/anbernic_misc/tpctrl; fi
# Reuse only the calibration coefficients already validated on this console.
export RGDS_TOUCH_CALIBRATION="$parent/ScummVM_RGDSPlus/config/touch-calibration.txt"
printf '\n=== Interface e reproducao ===\n'
while :; do
    "$app/bin/baetamp" "$app" &
    child=$!
    wait "$child"
    result=$?
    child=''
    printf '\nCodigo de saida: %s\n' "$result"
    [ "$result" = 42 ] || break
    restore
    if [ "$saved_touch" = 0 ] || [ "$saved_touch" = 1 ]; then printf '0\n' > /sys/class/anbernic_misc/tpctrl; fi
    printf 'Reabrir interface apos video (processo novo).\n'
done
exit "$result"
