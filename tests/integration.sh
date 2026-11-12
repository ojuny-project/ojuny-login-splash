#!/usr/bin/env bash
set -eu

binary=${1:-./build/ojuny-login-splash}

if [ ! -x "$binary" ]; then
  echo "build the binary first" >&2
  exit 1
fi

runtime=$(mktemp -d)
config=$(mktemp)
sway_pid=""
trap 'if [ -n "$sway_pid" ]; then kill "$sway_pid" 2>/dev/null || true; wait "$sway_pid" 2>/dev/null || true; fi; rm -rf "$runtime" "$config"' EXIT

printf 'output * bg #000000 solid_color\n' >"$config"

export XDG_RUNTIME_DIR="$runtime"
export XDG_SESSION_TYPE=wayland
export WLR_BACKENDS=headless
export WLR_HEADLESS_OUTPUTS=2
export WLR_LIBINPUT_NO_DEVICES=1
export WLR_RENDERER=pixman

sway -c "$config" >/dev/null 2>&1 &
sway_pid=$!

for _ in $(seq 1 100); do
  socket=$(find "$runtime" -maxdepth 1 -name 'wayland-*' -type s -print -quit 2>/dev/null || true)
  if [ -n "$socket" ]; then
    break
  fi
  sleep 0.05
done

if [ -z "${socket:-}" ]; then
  echo "headless sway did not start" >&2
  exit 1
fi

export WAYLAND_DISPLAY
WAYLAND_DISPLAY=$(basename "$socket")

echo "timer test"
"$binary" --seconds 0.5

echo "quit test"
"$binary" --seconds 30 &
pid=$!
sleep 0.3
"$binary" --quit
wait "$pid"

echo "multi-instance quit test"
"$binary" --seconds 30 &
first=$!
"$binary" --seconds 30 &
second=$!
sleep 0.3
"$binary" --quit
wait "$first"
wait "$second"

echo "fade test"
"$binary" --seconds 1 --fade 0.4

echo "software test"
LIBGL_ALWAYS_SOFTWARE=1 "$binary" --seconds 0.5

if command -v wtype >/dev/null 2>&1; then
  echo "escape test"
  "$binary" --seconds 30 &
  pid=$!
  sleep 0.3
  if wtype -k Escape; then
    wait "$pid"
  else
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    echo "wtype failed, skipping escape assertion" >&2
  fi
else
  echo "wtype not found, skipping escape test" >&2
fi

echo "integration tests passed"
