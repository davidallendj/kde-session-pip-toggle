#!/bin/sh
set -eu

prefix="${XDG_INSTALL_PREFIX:-$HOME/.local}"
autostart="${XDG_CONFIG_HOME:-$HOME/.config}/autostart/kde-pip-session.desktop"

if command -v systemctl >/dev/null 2>&1; then
    systemctl --user stop kde-pip-session.service 2>/dev/null || true
fi

if command -v kde-pip-session >/dev/null 2>&1; then
    pkill -x kde-pip-session 2>/dev/null || true
fi

rm -f "$prefix/bin/kde-pip-session"
rm -f "$prefix/share/applications/kde-pip-session.desktop"
rm -f "$prefix/libexec/kde-pip-session/nested-plasma-session"
rm -f "$prefix/libexec/kde-pip-session/kde-pip-session-decoration"
rmdir "$prefix/libexec/kde-pip-session" 2>/dev/null || true
rm -f "$autostart"

printf '%s\n' "Removed KDE PiP Session from $prefix"
