# l2dcat Native

This repository is the standalone native l2dcat desktop application. Its
source, runtime resources, tests, build files, and release packaging are all
self-contained at the repository root.

## Requirements

- CMake 3.24+
- Windows: Visual Studio 2022 Community **or** Visual Studio 2022 Build Tools
  (install one, not both)
- MSVC v143 x64/x86 build tools and a Windows 10 or Windows 11 SDK
- The "Desktop development with C++" workload; the CMake tools component is
  recommended, while MFC, ATL, .NET, Spectre libraries, and the full IDE are
  not required for command-line builds
- Linux/macOS: a C11 and C++17 compiler plus the platform development packages
- Live2D Cubism SDK for Native, placed at
  `vendor/CubismSdkForNative`

The Cubism SDK is not redistributed by this repository. Download it from the
official Live2D site after accepting its license. The expected directory must
contain `Core`, `Framework`, and their original SDK files.

Cubism SDK 5 r.5 ships GLEW and GLFW as downloadable sample dependencies. On
Windows, run the SDK-provided script once after extraction:

```powershell
cd vendor\CubismSdkForNative\Samples\OpenGL\thirdParty\scripts
.\setup_glew_glfw.bat
```

The native Windows build requires these paths:

```text
vendor/CubismSdkForNative/Core/include/Live2DCubismCore.h
vendor/CubismSdkForNative/Core/lib/windows/x86_64/143/Live2DCubismCore_MT.lib
vendor/CubismSdkForNative/Framework/CMakeLists.txt
vendor/CubismSdkForNative/Samples/OpenGL/thirdParty/glew/build/cmake/CMakeLists.txt
```

Without that SDK, CMake builds the diagnostic renderer only. The window,
overlays, preferences, and input auditing remain available, but the Live2D
model cannot render or react to mouse movement, motions, or expressions.

## Configure

### Windows Cubism build

Run this from the repository root. A normal PowerShell terminal is sufficient;
the Visual Studio CMake generator locates MSVC automatically.

```powershell
cmake -S . -B build-cubism -G "Visual Studio 17 2022" -A x64 `
  -DL2DCAT_REQUIRE_CUBISM=ON `
  -DL2DCAT_WARNINGS_AS_ERRORS=ON
cmake --build build-cubism --config Release --parallel 2
ctest --test-dir build-cubism -C Release --output-on-failure
```

The executable is written to `build-cubism/Release/l2dcat.exe`.

### Diagnostic or non-Windows build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The default build downloads pinned open-source dependencies into `build`.
Set `L2DCAT_FETCH_DEPS=OFF` to use installed SDL3 and yyjson packages.

## GitHub Actions

Every push, pull request, tag, and manual run executes `.github/workflows/ci.yml`.
Cppcheck, warning-clean compilation, the 300-line policy, legacy-name scanning,
and CTest must pass before the build matrix starts. Successful runs publish
downloadable Windows x86-64, Linux x86-64, and macOS artifacts from the run's
Artifacts section; no signing secret is required for these CI packages. The
public CI artifacts use the diagnostic renderer unless a licensed Cubism SDK
is provisioned on the runner.

## Distribution

`cmake --install build --prefix out` creates a portable staging directory.
Windows is packaged as one Authenticode-signed executable, Linux as a signed
AppImage, and macOS as a signed and notarized `.app.zip`. Windows embeds
built-in assets; other platforms install the same native resource tree beside
the application. Settings and imported models stay in the user data directory.

`cmake --build build --target native-release` writes the platform artifact
and its SHA-256 protected manifest to `build/native-release`. A release build
is rejected unless Cubism and the platform signing configuration are present:

Set `L2DCAT_RELEASE_BASE_URL` and `L2DCAT_UPDATE_MANIFEST_BASE_URL` to infrastructure
owned by this independent project. Both are empty by default, so the application
never contacts an upstream or third-party update channel unless explicitly configured.

- Windows: `L2DCAT_WINDOWS_SIGN_IDENTITY` and optionally `L2DCAT_SIGNTOOL`.
- macOS: `L2DCAT_MACOS_SIGN_IDENTITY` and `L2DCAT_MACOS_NOTARY_PROFILE`.
- Linux: `L2DCAT_LINUX_SIGN_KEY`, AppImage tools,
  `L2DCAT_LINUX_UPDATE_PUBLIC_KEY`, and `L2DCAT_LINUX_UPDATE_PRIVATE_KEY`.

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
300 physical lines. Run `cmake --build build --target check-lines` to enforce
the policy.

## Verification

The native test and audit commands do not depend on any abandoned desktop
experiments:

```powershell
ctest --test-dir build --output-on-failure
& .\cmake\VisualAudit.ps1 -Exe .\build\l2dcat.exe `
  -OutputDir .\build\visual-audit-full -SkipMain
& .\cmake\SoakAudit.ps1 -Exe .\build\l2dcat.exe `
  -OutputDir .\build\soak-audit -Mode hidden -DurationSeconds 60
& .\cmake\DropImportAudit.ps1 -Exe .\build\l2dcat.exe `
  -OutputDir .\build\drop-import-audit
& .\cmake\WindowsStyleAudit.ps1 -Exe .\build\l2dcat.exe -Mode taskbar
```

`VisualAudit.ps1` requires a same-run idle baseline, a `cubism-native` runtime
log, successful parameter assertions, and a measurable image difference before
any Live2D scenario can pass. Without the licensed Cubism SDK, diagnostic
screenshots are retained for troubleshooting but are deliberately reported as
blocked. The current evidence ledger is in `docs/feature-audit.md`.
