# BongoCat Native

This directory is the standalone native BongoCat desktop application. Its
source, runtime resources, tests, build files, and release packaging are all
self-contained under `c/`.

## Requirements

- CMake 3.24+
- Ninja or another CMake generator
- A C11 and C++17 compiler
- Live2D Cubism SDK for Native, placed at
  `vendor/CubismSdkForNative`

The Cubism SDK is not redistributed by this repository. Download it from the
official Live2D site after accepting its license. The expected directory must
contain `Core`, `Framework`, and their original SDK files.

## Configure

```powershell
cmake -S c -B c/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build c/build
ctest --test-dir c/build --output-on-failure
```

The default build downloads pinned open-source dependencies into `c/build`.
Set `BONGO_FETCH_DEPS=OFF` to use installed SDL3 and yyjson packages.

## Distribution

`cmake --install c/build --prefix c/out` creates a portable staging directory.
Windows is packaged as one Authenticode-signed executable, Linux as a signed
AppImage, and macOS as a signed and notarized `.app.zip`. Windows embeds
built-in assets; other platforms install the same native resource tree beside
the application. Settings and imported models stay in the user data directory.

`cmake --build c/build --target native-release` writes the platform artifact
and its SHA-256 protected manifest to `c/build/native-release`. A release build
is rejected unless Cubism and the platform signing configuration are present:

Set `BONGO_RELEASE_BASE_URL` and `BONGO_UPDATE_MANIFEST_BASE_URL` to infrastructure
owned by this independent project. Both are empty by default, so the application
never contacts an upstream or third-party update channel unless explicitly configured.

- Windows: `BONGO_WINDOWS_SIGN_IDENTITY` and optionally `BONGO_SIGNTOOL`.
- macOS: `BONGO_MACOS_SIGN_IDENTITY` and `BONGO_MACOS_NOTARY_PROFILE`.
- Linux: `BONGO_LINUX_SIGN_KEY`, AppImage tools,
  `BONGO_LINUX_UPDATE_PUBLIC_KEY`, and `BONGO_LINUX_UPDATE_PRIVATE_KEY`.

Windows updates are accepted only when both the installed app and replacement
have trusted Authenticode signatures using the same publisher key. macOS checks
the replacement against the installed app's designated signing requirement.
Linux verifies an Ed25519 signature over the release version, platform,
SHA-256, and size before the AppImage can be installed. Generate keys with:

```sh
openssl genpkey -algorithm Ed25519 -out update-private.pem
openssl pkey -in update-private.pem -pubout -outform DER -out update-public.der
```

## Source policy

All hand-written source, header, CMake, and test files must stay at or below
300 physical lines. Run `cmake --build c/build --target check-lines` to enforce
the policy.

## Verification

The native test and audit commands are independent of the abandoned desktop
experiments elsewhere in the parent directory:

```powershell
ctest --test-dir c/build --output-on-failure
& c/cmake/VisualAudit.ps1 -Exe c/build/BongoCat.exe `
  -OutputDir c/build/visual-audit-full -SkipMain
& c/cmake/SoakAudit.ps1 -Exe c/build/BongoCat.exe `
  -OutputDir c/build/soak-audit -Mode hidden -DurationSeconds 60
& c/cmake/DropImportAudit.ps1 -Exe c/build/BongoCat.exe `
  -OutputDir c/build/drop-import-audit
& c/cmake/WindowsStyleAudit.ps1 -Exe c/build/BongoCat.exe -Mode taskbar
```

`VisualAudit.ps1` requires a same-run idle baseline, a `cubism-native` runtime
log, successful parameter assertions, and a measurable image difference before
any Live2D scenario can pass. Without the licensed Cubism SDK, diagnostic
screenshots are retained for troubleshooting but are deliberately reported as
blocked. The current evidence ledger is in `docs/feature-audit.md`.
