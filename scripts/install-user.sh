#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$project_dir/build"
prefix="${XDG_INSTALL_PREFIX:-$HOME/.local}"

"$project_dir/scripts/build.sh"
cmake --install "$build_dir" --prefix "$prefix"

cat <<EOF

KDE Toggle was installed to:
  $prefix/bin/kde-toggle

Launch it from the KDE application menu or run:
  $prefix/bin/kde-toggle

Because it is launched inside Plasma, no manual DISPLAY/WAYLAND_DISPLAY
exports are needed. The installer starts the program after installation; set
KDE_TOGGLE_NO_LAUNCH=1 to install without launching it.
EOF

if [ "${KDE_TOGGLE_NO_LAUNCH:-0}" != 1 ]; then
    if ! pgrep -u "$(id -u)" -x kde-toggle >/dev/null 2>&1; then
        if command -v systemd-run >/dev/null 2>&1; then
            systemd-run --user --unit=kde-toggle --collect --no-block --quiet "$prefix/bin/kde-toggle"
        else
            nohup "$prefix/bin/kde-toggle" >/dev/null 2>&1 &
        fi
        printf '%s\n' "Started $prefix/bin/kde-toggle"
    else
        printf '%s\n' "KDE Toggle is already running"
    fi
fi
