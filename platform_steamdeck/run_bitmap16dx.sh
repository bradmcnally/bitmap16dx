#!/usr/bin/env bash

set -euo pipefail

app_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
app_binary="$app_dir/bitmap16dx_desktop.bin"
container="${BITMAP16_STEAM_DECK_CONTAINER:-dev}"

# Steam runs on the host, while build dependencies live in Distrobox.
# Direct invocation inside the same container needs no nested entry.
if [[ "${CONTAINER_ID:-}" == "$container" ]]; then
  exec "$app_binary" "$@"
fi

exec distrobox enter --name "$container" --no-tty -- "$app_binary" "$@"
