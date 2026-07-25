#!/bin/sh

LOG=/tmp/bitmap16dx.log
: >"$LOG" 2>/dev/null || LOG=/dev/null

if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
    runtime_uid=$(id -u 2>/dev/null || echo 1000)
    if [ -d "/run/user/$runtime_uid" ]; then
        XDG_RUNTIME_DIR="/run/user/$runtime_uid"
    elif [ -d "/run/user/1000" ]; then
        XDG_RUNTIME_DIR="/run/user/1000"
    fi
    [ -n "${XDG_RUNTIME_DIR:-}" ] && export XDG_RUNTIME_DIR
fi

wayland_ready=0
if [ -n "${WAYLAND_DISPLAY:-}" ] &&
   [ -n "${XDG_RUNTIME_DIR:-}" ] &&
   [ -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]; then
    wayland_ready=1
elif [ -n "${XDG_RUNTIME_DIR:-}" ]; then
    for candidate in wayland-0 wayland-1; do
        if [ -S "$XDG_RUNTIME_DIR/$candidate" ]; then
            WAYLAND_DISPLAY=$candidate
            export WAYLAND_DISPLAY
            wayland_ready=1
            break
        fi
    done
fi

if [ -z "${SDL_VIDEODRIVER:-}" ]; then
    if [ "$wayland_ready" = 1 ]; then
        SDL_VIDEODRIVER=wayland
    elif [ -e /dev/dri/card0 ]; then
        SDL_VIDEODRIVER=kmsdrm
    else
        SDL_VIDEODRIVER=offscreen
    fi
    export SDL_VIDEODRIVER
fi

echo "[bitmap16dx] driver=$SDL_VIDEODRIVER uid=$(id -u)" >>"$LOG" 2>&1
exec /usr/share/APPLaunch/apps/bitmap16dx/bitmap16dx_desktop \
    "$@" >>"$LOG" 2>&1
