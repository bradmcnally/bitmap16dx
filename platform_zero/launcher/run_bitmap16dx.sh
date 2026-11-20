#!/bin/sh

LOG=/tmp/bitmap16dx.log
: >"$LOG" 2>/dev/null || LOG=/dev/null
echo "[bitmap16dx] native LCD and keypad uid=$(id -u)" >>"$LOG"

exec /usr/share/APPLaunch/apps/bitmap16dx/bitmap16dx_desktop \
    "$@" >>"$LOG" 2>&1
