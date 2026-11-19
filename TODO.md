# Objective

Create a nested plasma session in a window that slides from one of the 4 screen edges in a fashion very similar to Yakuake. The nested session has a settings window where the user can specify the window width, height, slide animation duration, edge to slide from, global shortcut, option for screen percentages, startup on login, and more to come. 

# Rules and guidelines

1. AI agents should update their current tasks and state under the "Progress" section of this document.
2. AI agents should update the remaining tasks under the "Upcoming" section this document.
3. AI agents should NOT update any other sections of this document unless explicitly requested by a user.
4. AI agents should refer to the both sections mentioned above BEFORE performing any new tasks.



# Progress (edited by agents)

- Made the CSD span all virtual desktops, matching the nested window.
- Removed all minimize/unminimize operations; hidden state is controlled only by off-screen geometry, with the existing slide as the sole transition.
- Uses a startup handshake: the wrapper publishes the compositor PID and waits until the initial off-screen/hide script is armed before `exec` creates the window. Detection checks every 25 ms as a fallback.
- The startup KWin script now subscribes to `workspace.windowAdded` before scanning existing windows, so the compositor window is moved off-screen at first announcement rather than missed before it exists.

# Upcoming (edited by agents)

- Verify the startup behavior in the live Plasma session, especially that no frame is painted before KWin applies the off-screen placement and that the window is fully outside the active area (no sliver).
