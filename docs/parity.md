# Native completion contract

The standalone native application is complete only when each automated check
and the matching manual scenario pass without depending on another frontend or
desktop runtime.

## Model and rendering

- Load all three bundled Cubism `model3` models.
- Import and remove custom model directories without changing their layout.
- Render drawable order, masks, blend modes, opacity, multiply and screen color.
- Run `motion3` motions, `exp3` expressions, fade timing, and motion audio.
- Preserve mouse, keyboard, and gamepad parameter mappings.
- Draw the model background and at most one key overlay per hand.
- Preserve mirror, mouse mirror, model scale, opacity, radius, and FPS controls.

## Input

- Observe global keyboard press and release events.
- Observe global mouse buttons and coalesced pointer movement.
- Preserve Windows automatic release timing for system keys.
- Support SDL game controllers, sticks, triggers, and hot plug.
- Register behavior shortcuts and application shortcuts.
- Request the required macOS input-monitoring permission.

## Window and application

- Transparent, borderless, draggable main window with per-pixel alpha.
- Click-through, always-on-top, hide-on-hover, and keep-in-monitor behavior.
- Shift plus secondary drag resizing from 10 through 500 percent.
- Tray and context menus, taskbar visibility, and single instance handling.
- Restore window position and size across monitor and DPI changes.
- Autostart, independently configured portable updates, project information,
  and application exit.

## Preferences and data

- General, cat, model, behavior, and shortcut preference views.
- English, Simplified Chinese, Traditional Chinese, Portuguese, and Vietnamese.
- Light, dark, and system themes.
- Persist native settings and custom models without external project paths.
- Build, test, run, and package with only files contained in this repository.
- Persist settings atomically and recover from a truncated write.

## Release gates

- Golden images for idle, key, mouse, controller, motion, and expression states.
- Recorded input traces produce equivalent parameter values in old and new apps.
- Windows 10/11, macOS x64/arm64, and Linux X11 manual matrix passes.
- Wayland reports reduced global-input support instead of silently failing.
- Idle CPU is below 0.3 percent on the reference machine.
- Working set is below 100 MiB with one bundled model active.
- Hidden rendering is suspended and a 24-hour run has no unbounded growth.
- Every hand-written file is at most 300 physical lines.
