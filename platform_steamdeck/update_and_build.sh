#!/usr/bin/env bash

set -euo pipefail

pause_on_exit=false
if [[ "${1:-}" == "--pause" ]]; then
  pause_on_exit=true
fi

finish() {
  status=$?
  trap - EXIT
  echo
  if [[ "$status" -eq 0 ]]; then
    echo "Update and build completed successfully."
  else
    echo "Update or build failed with exit code $status."
  fi
  if [[ "$pause_on_exit" == true ]]; then
    echo
    read -r -p "Press Enter to close this window..."
  fi
  exit "$status"
}
trap finish EXIT

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

git pull
cmake --preset steamdeck
cmake --build --preset steamdeck --verbose
