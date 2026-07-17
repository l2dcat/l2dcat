# Windows systematic audit — 2026-07-17

## Scope

This audit covers the independent native C application on the available Windows host:
bundled assets, diagnostic fixed-image states, global input isolation, model import,
window chrome, preferences, localization, texture lifetime, memory, CPU, tests and
desktop delivery. Cubism rendering and non-Windows execution remain external gates.

## Defects found and corrected

| Defect | Correction | Proof |
|---|---|---|
| Keyboard fallback retained raised-paw marks | CPU-composite complete idle/left/right/both frames and erase the matching raised-paw area | `build/overlay-audit-coalesced-final/overlay-audit.csv` |
| Standard and gamepad had the same duplicate-paw defect | Paw cleanup now applies to every composed fallback model | Standard and gamepad scenarios pass the overlay audit |
| Main window exposed Win32 resize chrome and caption button | Removed SDL resizable flag and explicitly cleared caption, thick frame, system menu, minimize and maximize styles | Three modes pass `WindowsStyleAudit.ps1` |
| Transparent-window screenshots could show black DWM transition frames | Main views use desktop capture; authoritative comparisons use application framebuffer BMPs | Internal frame files in the overlay audit |
| Shared audit data could reuse stale/locked frames | Every process has an isolated data root; frame copies wait and retry while locked | Repeated isolated overlay audit is deterministic |
| Real global input could contaminate internal scenario baselines | Internal scenarios ignore external hooks; dedicated external-input audits remain unchanged | Idle/release comparisons are exactly zero |
| Recreating/uploading a texture for every input transition peaked at 689 MiB | Reuse one texture and coalesce all input changes into one upload before the next rendered frame | 1000 transitions peak at 70.039 MiB |
| Diagnostic idle loop woke at 60 FPS without frame changes | Static fallback waits 100 ms while SDL events still wake immediately | Visible CPU reduced from 0.3231% to 0.0324% |

## Automated evidence

| Area | Result |
|---|---|
| Asset integrity | PASS — 3 models, 141 PNGs, 126 overlays, 0 invalid dimensions/Alpha, 0 missing required files |
| Overlay states | PASS — standard left; keyboard left/right/both; gamepad both |
| Release recovery | PASS — single and dual release frames exactly equal idle |
| Texture stress | PASS — 250 rounds / 1000 transitions return exactly to idle |
| Stress memory | PASS — 70.039 MiB peak, 60.590 MiB final |
| Window styles | PASS — default, taskbar and menu modes have no caption/frame/menu/buttons |
| Theme/model import | PASS — valid accepted; malformed, missing cover/texture and traversal rejected |
| Preferences visuals | PASS — 5 languages × 2 themes × 5 pages = 50/50 |
| Visible performance | PASS — 70.672 MiB peak, -0.055 MiB growth, 0.0324% CPU |
| Unit tests | PASS — 4/4 |
| Static analysis | PASS — warning/performance/portability scan across 55 source files |
| Source policy | PASS — all checked source files are at most 300 lines |

## Repeatable commands

- `cmake/AssetAudit.ps1`
- `cmake/OverlayAudit.ps1`
- `cmake/WindowsStyleAudit.ps1`
- `cmake/ThemeImportAudit.ps1`
- `cmake/VisualAudit.ps1`
- `cmake/SoakAudit.ps1`

## External gates

- Licensed Cubism Native SDK: real dynamic model, masks, blend modes, motions and expressions.
- Physical gamepad hot-plug and audible speaker confirmation.
- Uninterrupted 24-hour result and a second supported Windows version.
- macOS and Linux real-machine execution.

No diagnostic fixed image is counted as a Cubism rendering pass.
