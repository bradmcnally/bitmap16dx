#!/usr/bin/env bash

set -euo pipefail

pause_on_exit=false
if [[ "$#" -eq 1 && "${1:-}" == "--pause" ]]; then
  pause_on_exit=true
elif [[ "$#" -ne 0 ]]; then
  echo "Usage: $0 [--pause]" >&2
  exit 2
fi

finish() {
  status=$?
  trap - EXIT
  echo
  if [[ "$status" -eq 0 ]]; then
    echo "Update, checks, and installation completed successfully."
  else
    echo "Update or build failed with exit code $status. The installed app was not replaced."
  fi
  if [[ "$pause_on_exit" == true ]]; then
    echo
    read -r -p "Press Enter to close this window..." || true
  fi
  exit "$status"
}
trap finish EXIT

if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  echo "Run this script inside the Steam Deck's Linux dev container." >&2
  exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

git pull --ff-only
# Reset old cached direct-feed configuration; an explicit environment override
# remains available for developers running their own feed.
configure_args=(--preset steamdeck -DBUILD_TESTING=ON -UBITMAP16_LOSPEC_FEED_URL)
if [[ "${BITMAP16_LOSPEC_FEED_URL+x}" == x ]]; then
  configure_args+=("-DBITMAP16_LOSPEC_FEED_URL=$BITMAP16_LOSPEC_FEED_URL")
fi
cmake "${configure_args[@]}"
cmake --build --preset steamdeck --parallel
ctest --test-dir build_steamdeck --output-on-failure

built_executable="$repo_root/build_steamdeck/bitmap16dx_desktop"
case "$(LC_ALL=C file --brief "$built_executable")" in
  *ELF*"x86-64"*) ;;
  *) echo "Build output is not a Linux x86-64 executable; refusing to install it." >&2; exit 1 ;;
esac

steam_install_dir="${BITMAP16_STEAM_DECK_INSTALL_DIR:-$HOME/Applications/bitmap16dx}"
mkdir -p "$steam_install_dir/licenses"
install -m 644 LICENSE "$steam_install_dir/licenses/bitmap16dx-LICENSE"
install -m 644 third_party/picojson/picojson.h "$steam_install_dir/licenses/picojson.h"
source_revision="$(git rev-parse --short HEAD)"
if [[ -n "$(git status --porcelain)" ]]; then
  source_revision="$source_revision (local changes)"
fi
{
  echo "BitMap16 DX - Steam Deck"
  echo "Source: $source_revision"
  echo "Built: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
  echo "Target: Linux x86-64, Steam Deck 320x200 at 4x scale"
} > "$steam_install_dir/build-info.txt"
# Replace the same executable used by the documented Steam shortcut only after
# build and checks pass. Rename keeps an interrupted copy from damaging it.
install -m 755 "$built_executable" "$steam_install_dir/.bitmap16dx_desktop.new"
mv -f "$steam_install_dir/.bitmap16dx_desktop.new" "$steam_install_dir/bitmap16dx_desktop"
echo "Installed: $steam_install_dir/bitmap16dx_desktop"
echo "Launch the existing non-Steam game shortcut to test this build."
