# KDE PiP Session

<div align="center">
  <video src="kde-pip-toggle-demo.webm" controls muted loop width="900">
    Your browser does not support embedded video. [Download the demo video](kde-pip-toggle-demo.webm).
  </video>
</div>


KDE PiP Session is a KDE Plasma 6 prototype inspired by Yakuake. A global shortcut toggles a separate **Nested Plasma desktop** session inside a window.

The nested mode is the desktop-inside-a-window experiment. When KDE PiP Session starts, it initializes the session from the saved settings and places its window fully off-screen without animation. The first toggle slides it into view; later toggles slide that same window out and back in. Settings and state are persisted between runs. Closing the nested window ends that session, so the next toggle creates a new one.

After toggling the nested session, the settings window remains available; minimize or close it explicitly when desired. The application also continues running in the system tray.

## Requirements

- KDE Plasma 6 running on Wayland
- KWin 6 with windowed nested-compositor support
- Qt 6 development packages
- KDE Frameworks 6 GlobalAccel development package
- CMake and a C++ compiler
- `dbus-run-session` and `plasmashell`

The nested mode was developed against KWin 6.8.2, Qt 6.12, and KF6 GlobalAccel 6.31.

## Build

    cd ~/Work/kde-pip-session
    cmake -S . -B build
    cmake --build build -j"$(nproc)"

Or use the reusable build script:

    ./scripts/build.sh

Run the prototype:

    ./build/kde-pip-session

User installation (recommended for graphical-session testing):

    ./scripts/install-user.sh

This builds and installs to `~/.local`, adds a KDE application-menu entry, and starts the installed program. Launch **KDE PiP Session** from the Plasma application menu afterward; this inherits the graphical session automatically, so no manual DISPLAY/WAYLAND_DISPLAY exports are needed. You can also run `~/.local/bin/kde-pip-session` from a KDE terminal. Set `KDE_PIP_SESSION_NO_LAUNCH=1` when invoking the installer if you only want to install without starting it.

To uninstall the user installation:

    ./scripts/uninstall-user.sh

The project does not start on login by default. Enable **Start on login** in the settings window when desired; it creates a user-scoped KDE autostart entry.

## Configure the shortcut

Open the settings window and use **Global shortcut**:

1. Click the shortcut field.
2. Press the desired key combination.
3. The shortcut is saved and registered immediately.
4. Clear the field to disable the global shortcut.

The default is **Meta+grave**. If KDE reports that a shortcut is unavailable, choose another combination.

The nested session has its own custom client-side decoration with KDE-themed icons for **Settings**, **Toggle**, **Pin**, and **Hide** (with text fallbacks when an icon is unavailable). It is always anchored to the same visible edge as **Slide from**: bottom/top use a horizontal bar, while left/right use a vertical bar. Drag the bar's resize grip to change height for top/bottom or width for left/right. The **Meta+Shift+T** global shortcut shows or hides that nested-session decoration and the preference is saved. The settings window uses its normal desktop decoration and is separate from the nested-session controls. Changes to size, edge, animation, and decoration state apply immediately to an existing nested session; the nested window is always hidden when the controller starts.

## Configure the nested window

The controller is dedicated to the nested Plasma session. These controls apply to it:

- **Nested width / height** - the compositor window size. Each has a synchronized spinbox and slider; dragging a slider previews the value in the spinbox, and the nested window is resized when the slider is released. Percentage mode is enabled by default, with defaults of **100% width** and **70% height**. Disable it to enter pixel sizes.
- **Slide from** - Bottom, Top, Left, or Right. Bottom is the default.
- **Animate slide** - enable or disable the slide transition.
- **Animation duration** - transition length from 50 to 2000 milliseconds (500 ms by default).
- **Global shortcut** - opens or hides the persistent nested window. Clicking another outer window moves it off-screen automatically unless **Pin** is enabled; the nested window is never minimized by this behavior.
- **Meta+Shift+T** - shows or hides the nested-session decoration.
- **Settings / Toggle / Pin / Hide** - controls in the nested-session decoration. The decoration occupies the full Slide-from edge of the nested screen and follows edge changes immediately; its resize grip changes the perpendicular dimension. **Pin** prevents outside clicks from hiding the nested window.
- **Start on login** - launches KDE PiP Session automatically when KDE starts.

The window is kept above other windows while it is visible. The slide animation is implemented through a short KWin script that moves the Wayland-managed nested-compositor window on the outer desktop.

## Test the nested Plasma desktop

1. Start KDE PiP Session:

       cd ~/Work/kde-pip-session
       ./build/kde-pip-session

2. Leave KDE PiP Session running. Its nested session starts from the saved settings, fully off-screen and hidden.
3. Leave **Slide from** set to **Bottom**, or choose another edge.
4. Choose a size, such as **1280 x 800**.
5. Press **Toggle nested session** or the configured global shortcut. The settings window remains open; minimize or close it explicitly if desired.
6. If the nested session was not already initialized, wait a few seconds for the separate Plasma Shell to initialize. The first toggle slides the nested desktop in.
7. Press the configured global shortcut again. The same window should slide out.
8. Press the shortcut again. It should slide back in without starting another session.
9. Use the tray menu to show the settings again if needed, then close the nested window using its window close button. The nested session ends.
10. Press the shortcut again. A new nested session should be created.

The nested compositor is started approximately as:

    dbus-run-session kwin_wayland \
      --xwayland \
      --width 1280 \
      --height 800 \
      --no-global-shortcuts \
      --no-lockscreen \
      --exit-with-session nested-plasma-session

The helper starts Plasma Shell on a separate D-Bus session so it does not take over the real desktop's Plasma Shell or KWin services.

### What to expect

- This is a second desktop session, not an existing virtual desktop from the main session.
- Applications launched inside it belong to the nested session.
- The nested session can have its own virtual desktops and panel.
- The outer KWin compositor manages the nested window.
- It may take several seconds for Plasma Shell and its services to appear.
- The nested session uses the host XDG config, data, state, and cache locations directly, so its Plasma theme, panels, wallpaper, and application settings match the host session. Nested-session changes can therefore affect the host configuration.
- A nested-session Brave desktop override uses a separate profile under `~/.config/kde-pip-session/brave-nested`, preventing Brave single-instance forwarding from opening windows in the outer session.
- If the nested session crashes, a stale process may remain. Inspect processes before terminating anything.

## How it works

- **Global shortcut:** KDE Frameworks GlobalAccel registers the saved nested-session shortcut and the Meta+Shift+T decoration shortcut while KDE PiP Session is running.
- **Nested mode:** starts a windowed `kwin_wayland` with a separate D-Bus session and Plasma Shell helper.
- **Persistence:** the helper records the nested KWin PID in `$XDG_RUNTIME_DIR/kde-pip-session-nested.pid`. The controller uses that PID to find the corresponding outer-KWin window.
- **Nested decoration:** a small Qt helper runs inside the nested session, so its controls are part of that session rather than the settings window.
- **Outside-click behavior:** a KWin script watches outer-window activation and moves the nested window off-screen unless it is pinned.
- **Session data overlay:** the nested Plasma session uses a small temporary data-directory overlay for its generated desktop entries while inheriting the host XDG locations, avoiding the old recursive-copy startup failure and stale applet configuration mismatch.
- **Animation:** a temporary KWin script changes the nested window's geometry over the configured duration, then unloads itself. The animation can be disabled.
- **Settings:** dimensions, percentage mode, edge, animation, shortcut, pin state, decoration visibility, and autostart are stored with Qt `QSettings`.
- **Autostart:** the Start on login option writes `~/.config/autostart/kde-pip-session.desktop` and removes it when disabled.

## Troubleshooting

### No nested window appears

Run the compositor chain directly:

    cd ~/Work/kde-pip-session
    timeout 15s dbus-run-session -- kwin_wayland \
      --xwayland --width 900 --height 600 \
      --no-global-shortcuts --no-lockscreen \
      --exit-with-session ./scripts/nested-plasma-session

If `kwin_wayland` rejects `--width` or `--height`, the installed KWin does not support this nested mode.

### The window appears but does not slide

Confirm that the program is running inside the real KDE Wayland session. The animation uses the outer KWin scripting D-Bus interface and cannot work from an unrelated SSH/text-only session.

### The nested window is black or empty

Wait several seconds for Plasma Shell to initialize, then inspect the terminal output from the direct command. A nested Plasma session shares user configuration, so a conflicting session service may also prevent the shell from starting cleanly.

### The shortcut does nothing

Confirm KDE PiP Session is still running and try the tray menu's **Toggle nested session** action. A shortcut conflict is separate from the nested compositor.

### The nested session survives unexpectedly

Find the processes:

    pgrep -af 'kwin_wayland|plasmashell|dbus-run-session'

Only terminate processes belonging to the nested test. Do not kill the outer desktop's KWin or Plasma Shell.

## Clean up

To remove the build directory:

    rm -rf ~/Work/kde-pip-session/build

Use the uninstall script instead:

    ./scripts/uninstall-user.sh

It removes the installed executable, helper, application-menu entry, and KDE autostart entry. It does not remove the source directory, build directory, or saved settings.
