# Native feature audit

This is the living completion ledger for the independent native application.
`PASS` requires direct automated or visual evidence. `PENDING` means the code is
present but the required runtime environment has not been exercised. `BLOCKED`
means an external prerequisite is absent. A diagnostic drawing never counts as
a Live2D visual pass.

## Current evidence

- Systematic Windows audit: `docs/system-audit-2026-07-17.md` — PASS.
- Asset audit: `build/asset-audit-final/assets.csv` — PASS, 141/141 PNGs.
- Overlay audit: `build/overlay-audit-coalesced-final/overlay-audit.csv` — PASS, 8/8 scenarios.
- Release build: `cmake --build c/build -j 2` — PASS, warnings are errors.
- Native tests: `ctest --test-dir c/build --output-on-failure` — PASS, 4/4.
- Static analysis: Cppcheck warning/performance/portability scan — PASS.
- Source policy: `check-lines` — PASS, every checked source is at most 300 lines.
- Preferences matrix: `c/build/visual-final-after-window-fix/audit.csv` — PASS, 50/50.
- Chinese preferences working set: 92.92 MiB stable — PASS, below 100 MiB.
- Thirty-second visible soak: 69.992 MiB, -0.004 MiB growth, 0.2071% CPU — PASS.
- Five-minute hidden soak: 56.371 MiB peak, -3.476 MiB growth, 0.0182% CPU — PASS.
- Thirty-second zh-CN preferences soak: 81.43 MiB, 0.246 MiB growth — PASS.
- Desktop binary SHA-256 matches `c/build/l2dcat.exe` — PASS.
- Cubism Native SDK under `c/vendor/CubismSdkForNative` — BLOCKED, absent.
- Tauri-parity mouse mapping and frame-rate-independent 0.75 damping tests — PASS.
- Windows pointer-hook runtime audit: 24 coordinate samples, process exit 0 — PASS.

## Models and rendering

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Three bundled model packages | PASS | Model tests validate MOC3, textures, settings, motions and expressions. |
| Background and key overlays | PASS | `OverlayAudit.ps1` covers standard, keyboard and gamepad states, exact release recovery and 1000-transition stress. |
| Custom model import/remove | PASS | Valid import/remove smoke passes; malformed JSON, missing cover, missing texture and path-traversal manifests are rejected without partial installation (`cmake/ThemeImportAudit.ps1`). |
| Model selection | PASS | Model catalog tests and model-card interaction implementation. |
| Real Cubism model rendering | BLOCKED | Requires licensed Cubism SDK and `renderer=cubism-native`. |
| Masks, blend modes and drawable order | BLOCKED | Must be rendered and screenshot-checked with Cubism. |
| Motion playback and fades | BLOCKED | Runtime assertion exists; real Cubism execution is required. |
| Expression playback | BLOCKED | Runtime assertion exists; real Cubism execution is required. |
| Motion audio | PENDING | Audio path is implemented; manual audible check remains. |
| Mirror visual result | BLOCKED | Must differ from the real-model idle golden image. |

## Input and behavior

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Keyboard left/right hands | PASS | App-state unit assertions cover press and release. |
| Mouse left/right buttons | PASS | App-state unit assertions cover down and up. |
| Gamepad buttons and four axes | PASS | App-state assertions cover values, hands and reset. |
| Global Windows keyboard input | PASS | Injected A/right-arrow events change rendered overlays. |
| Global Windows mouse input | PASS | Injected left/right down/up events reach the audit queue. |
| Global Windows pointer movement | PASS | Low-level hook coordinates are captured by the runtime mouse audit; smoothing and parameter mapping have deterministic unit coverage. |
| Pointer-driven Live2D tracking | BLOCKED | Global pointer collection, seven-parameter mapping, mirror behavior and Tauri-parity damping are implemented and unit-tested; visual proof requires the absent Cubism Native SDK. |
| Windows automatic key release | PASS | Core input tests cover scheduled releases. |
| Gamepad hot plug | PENDING | SDL implementation exists; physical-device test remains. |
| Behavior shortcuts | PENDING | Parsing and dispatch are tested indirectly; Cubism result is blocked. |
| macOS input permission | PENDING | Native implementation exists; macOS manual verification remains. |

## Window and application

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Transparent borderless main window | PASS | Win32 caption, thick frame, system menu and caption buttons are explicitly removed; desktop and framebuffer captures show no outline or close button. |
| Preferences window | PASS | 50 current screenshots, no blank frames. |
| Click-through and always-on-top | PASS | Visible HWND has both native style flags after menu smoke. |
| Hide on hover | PASS | Enter/hide/pass-through/leave/restore chain is asserted. |
| Keep inside monitor | PASS | Smoke moves the window off-screen and verifies clamping. |
| Menu scale and opacity | PASS | Native size and opacity values are asserted. |
| Shift plus right-drag resize | PASS | Synthetic gesture verifies scale, size and release. |
| Taskbar visibility | PASS | Native TOOLWINDOW/APPWINDOW styles are asserted. |
| System tray | PASS | Native SDL tray handle and all state callbacks are asserted. |
| Right-click context menu | PASS | Real Windows `#32768` popup was opened through the production `TrackPopupMenu` path; selecting Preferences opened the settings window (`cmake/ContextMenuAudit.ps1`). |
| Single instance | PASS | Second process exits 0 while the first remains alive. |
| Autostart | PENDING | Platform implementations exist; OS login test remains. |
| Independent update channel | PASS | Empty by default; HTTPS and signature validation tests pass. |
| Hidden rendering suspension | PASS | Main loop skips model updates while hidden. |

## Preferences, localization and data

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Cat, general, model, shortcuts and about pages | PASS | 50-view matrix. |
| Model card grid and direct-drawn icons | PASS | Modern compact navigation and responsive cards verified in `build/visual-modern-final` (50 views). |
| English, Simplified/Traditional Chinese, Portuguese, Vietnamese | PASS | Locale-shape tests and screenshot matrix. |
| Light and dark themes | PASS | Both are covered by the 50-view matrix. |
| System theme | PASS | Auto-mode screenshot resolves to the current Windows dark theme. |
| Click model import | PASS | Import/remove smoke exercises the shared importer. |
| Drag model import | PASS | Native WM_DROPFILES audit imports one complete model directory. |
| Atomic configuration persistence | PASS | Configuration tests include truncated-write recovery. |
| No Electron, Tauri or upstream update dependency | PASS | Scoped source/config search is clean. |

## Performance and platform gates

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Working set below 100 MiB | PASS | Preferences 81.027 MiB; visible 70.672 MiB; 1000-transition stress 70.039 MiB peak. |
| Diagnostic idle CPU below 0.3 percent | PASS | Static fallback visible average is 0.0324%; hidden average is 0.0182%. |
| Cubism idle CPU below 0.3 percent | BLOCKED | Requires the Cubism SDK build. |
| Five-minute bounded-memory runs | PASS | Visible, preferences and hidden modes remain bounded. |
| 24-hour bounded-memory run | PENDING | The harness is ready, but a full uninterrupted 24-hour result is still required. |
| Windows 10 and 11 | PENDING | Current machine passes; second supported Windows version remains. |
| macOS x64 and arm64 | PENDING | Requires macOS runners and signed application builds. |
| Linux X11 | PENDING | Requires Linux runner with XInput2/Xfixes. |
| Linux Wayland reduced-mode reporting | PENDING | Requires a Wayland session. |

The application is not release-complete while any `BLOCKED` or release-gate
`PENDING` item remains.
