#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$project_dir/build"
prefix="${XDG_INSTALL_PREFIX:-$HOME/.local}"

"$project_dir/scripts/build.sh"
cmake --install "$build_dir" --prefix "$prefix"

cat <<EOF

KDE PiP Session was installed to:
  $prefix/bin/kde-pip-session

Launch it from the KDE application menu or run:
  $prefix/bin/kde-pip-session

Because it is launched inside Plasma, no manual DISPLAY/WAYLAND_DISPLAY
exports are needed. The installer starts the program after installation; set
KDE_PIP_SESSION_NO_LAUNCH=1 to install without launching it.
EOF

if [ "${KDE_PIP_SESSION_NO_LAUNCH:-0}" != 1 ]; then
    if ! pgrep -u "$(id -u)" -x kde-pip-session >/dev/null 2>&1; then
        if command -v systemd-run >/dev/null 2>&1; then
            systemd-run --user --unit=kde-pip-session --collect --no-block --quiet "$prefix/bin/kde-pip-session"
        else
            nohup "$prefix/bin/kde-pip-session" >/dev/null 2>&1 &
        fi
        printf '%s\n' "Started $prefix/bin/kde-pip-session"
    else
        printf '%s\n' "KDE PiP Session is already running"
    fi
fi
