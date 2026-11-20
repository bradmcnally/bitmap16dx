#!/bin/bash
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH:-}"
if [[ $# -ne 2 || ( $1 != package && $1 != install ) || $2 == -* ]]; then
  printf 'Usage: %s {package|install} user@host\n' "$0" >&2
  exit 2
fi

mode=$1
device=$2
repo=$(cd "$(dirname "$0")/.." && pwd)
package_dir="$repo/build_zero_device"
version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$repo/app-builder.json")
if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+-[0-9]+$ ]]; then
  printf 'Invalid package version in app-builder.json\n' >&2
  exit 1
fi
package_name="bitmap16dx_${version}_arm64.deb"
package="$package_dir/$package_name"

if [[ $mode == package ]]; then
  mkdir -p "$package_dir"
  download=$(mktemp "$package_dir/.bitmap16dx.XXXXXX")
  release_config=$(mktemp -d "$package_dir/.lospec.XXXXXX")
  trap 'rm -f "$download"; rm -rf "$release_config"' EXIT
  mkdir -p "$release_config/build_zero_device"
  release_key=${BITMAP16_LOSPEC_KEY_FILE:-"$HOME/.local/share/bitmap16dx/lospec-key.txt"}
  feed_url=${BITMAP16_LOSPEC_FEED_URL-https://bitmap16dx-palettes.brad-9f9.workers.dev/palettes.json}
  printf '%s' "$feed_url" >"$release_config/build_zero_device/lospec-feed-url.txt"
  if [[ -z $feed_url && -f $release_key ]]; then
    cp "$release_key" "$release_config/build_zero_device/lospec-key.txt"
    chmod 600 "$release_config/build_zero_device/lospec-key.txt"
  else
    : >"$release_config/build_zero_device/lospec-key.txt"
  fi

  tar --format=ustar --no-xattrs -C "$repo" -cf - \
    CMakeLists.txt CMakePresets.json LICENSE app-builder.json \
    platform_desktop platform_zero src third_party \
    -C "$release_config" build_zero_device/lospec-key.txt build_zero_device/lospec-feed-url.txt |
    ssh -T "$device" '
      set -eu
      mkdir -p "$HOME/bitmap16dx-test"
      cd "$HOME/bitmap16dx-test"
      tar -xf -
      feed_url=$(cat build_zero_device/lospec-feed-url.txt)
      cmake --preset zero-device \
        -DBITMAP16_LOSPEC_KEY_FILE="$PWD/build_zero_device/lospec-key.txt" \
        -DBITMAP16_LOSPEC_FEED_URL="$feed_url" >&2
      cmake --build --preset zero-device >&2
      package_dir=$(mktemp -d)
      trap '\''rm -rf "$package_dir"'\'' EXIT
      cpack --config build_zero_device/CPackConfig.cmake -B "$package_dir" >&2
      set -- "$package_dir"/bitmap16dx_*_arm64.deb
      test "$#" -eq 1
      test -f "$1"
      cat "$1"
    ' >"$download"

  ar -t "$download" >/dev/null
  staged_package="$release_config/$package_name"
  mv "$download" "$staged_package"
  python3 "$repo/platform_zero/tools/check_store.py" --deb "$staged_package"
  mv -f "$staged_package" "$package"
  printf 'Package ready: %s\n' "$package"
else
  if [[ ! -f $package ]]; then
    printf 'No package at %s. Run the package command first.\n' "$package" >&2
    exit 1
  fi

  scp "$package" "$device:/tmp/bitmap16dx-install.deb"
  ssh -t "$device" 'sudo apt install --reinstall /tmp/bitmap16dx-install.deb'
fi
