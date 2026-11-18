#!/bin/sh
set -eu

prefix="${XDG_INSTALL_PREFIX:-$HOME/.local}"
autostart="${XDG_CONFIG_HOME:-$HOME/.config}/autostart/kde-toggle.desktop"

if command -v systemctl >/dev/null 2>&1; then
    systemctl --user stop kde-toggle.service 2>/dev/null || true
fi

if command -v kde-toggle >/dev/null 2>&1; then
    pkill -x kde-toggle 2>/dev/null || true
fi

rm -f "$prefix/bin/kde-toggle"
rm -f "$prefix/share/applications/kde-toggle.desktop"
rm -f "$prefix/libexec/kde-toggle/nested-plasma-session"
rmdir "$prefix/libexec/kde-toggle" 2>/dev/null || true
rm -f "$autostart"

printf '%s\n' "Removed KDE Toggle from $prefix"
