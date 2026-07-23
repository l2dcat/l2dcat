param([string]$Exe = "", [string]$OutputDir = "")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build\BongoCatNeo.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build\theme-import-audit" }
$Exe = [IO.Path]::GetFullPath($Exe)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$source = Join-Path $root "resources\assets\models\standard"
Add-Type -AssemblyName System.Drawing
Remove-Item -LiteralPath $OutputDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

function New-Fixture([string]$Name) {
    $path = Join-Path $OutputDir $Name
    Copy-Item -LiteralPath $source -Destination $path -Recurse
    return $path
}

function New-PixelPng([string]$Path, [int]$Red, [int]$Green, [int]$Blue, [int]$Alpha) {
    $bitmap = [Drawing.Bitmap]::new(1, 1, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bitmap.SetPixel(0, 0, [Drawing.Color]::FromArgb($Alpha, $Red, $Green, $Blue))
    $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

$fixtures = @()
$fixtures += [pscustomobject]@{ Name="valid"; Path=(New-Fixture "valid"); Valid=$true; Count=1 }
$malformed = New-Fixture "malformed-json"
Set-Content -LiteralPath (Join-Path $malformed "cat.model3.json") -Value "{" -NoNewline
$fixtures += [pscustomobject]@{ Name="malformed-json"; Path=$malformed; Valid=$false; Count=0 }
$missingCover = New-Fixture "missing-cover"
Remove-Item -LiteralPath (Join-Path $missingCover "resources\cover.png")
$fixtures += [pscustomobject]@{ Name="missing-cover"; Path=$missingCover; Valid=$true; Count=1 }
$missingBackground = New-Fixture "missing-background"
Remove-Item -LiteralPath (Join-Path $missingBackground "resources\background.png")
$fixtures += [pscustomobject]@{ Name="missing-background"; Path=$missingBackground; Valid=$true; Count=1 }
$nativeConfig = New-Fixture "native-unrelated-config"
New-Item -ItemType Directory -Path (Join-Path $nativeConfig "hand") | Out-Null
Set-Content -LiteralPath (Join-Path $nativeConfig "config.json") -Value "{}" -NoNewline
$fixtures += [pscustomobject]@{ Name="native-unrelated-config"; Path=$nativeConfig
    Valid=$true; Count=1 }
$nativeInvalid = New-Fixture "native-invalid-config"
New-Item -ItemType Directory -Path (Join-Path $nativeInvalid "hand") | Out-Null
Set-Content -LiteralPath (Join-Path $nativeInvalid "config.json") -Value "{" -NoNewline
$fixtures += [pscustomobject]@{ Name="native-invalid-config"; Path=$nativeInvalid
    Valid=$true; Count=1 }
$missingTexture = New-Fixture "missing-texture"
Remove-Item -LiteralPath (Join-Path $missingTexture "demomodel.1024\texture_00.png")
$fixtures += [pscustomobject]@{ Name="missing-texture"; Path=$missingTexture; Valid=$false; Count=0 }
$corruptTexture = New-Fixture "corrupt-texture"
[IO.File]::WriteAllBytes((Join-Path $corruptTexture "demomodel.1024\texture_00.png"),
    [byte[]](0x89, 0x50, 0x4e, 0x47))
$fixtures += [pscustomobject]@{ Name="corrupt-texture"; Path=$corruptTexture
    Valid=$false; Count=0 }
$traversal = New-Fixture "path-traversal"
$manifest = Get-Content (Join-Path $traversal "cat.model3.json") -Raw |
    ConvertFrom-Json
$manifest.FileReferences.Moc = "../outside.moc3"
$manifest | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $traversal "cat.model3.json")
$fixtures += [pscustomobject]@{ Name="path-traversal"; Path=$traversal; Valid=$false; Count=0 }

$package = Join-Path $OutputDir "nested-package"
$imageRoot = Join-Path $package "img"
New-Item -ItemType Directory -Force -Path $imageRoot | Out-Null
Copy-Item (Join-Path $source "cat.model3.json") (Join-Path $imageRoot "cat.model3.json")
foreach ($mode in @("standard", "keyboard", "gamepad")) {
    $modeSource = Join-Path $root "resources\assets\models\$mode"
    $modeRoot = Join-Path $imageRoot $mode
    $modelRoot = Join-Path $modeRoot "cat_model"
    New-Item -ItemType Directory -Force -Path $modeRoot | Out-Null
    Copy-Item -LiteralPath $modeSource -Destination $modelRoot -Recurse
    Remove-Item -LiteralPath (Join-Path $modelRoot "resources") -Recurse -Force
    Copy-Item (Join-Path $modeSource "resources\cover.png") (Join-Path $modeRoot "cat.png")
    $backgroundName = if ($mode -eq "standard") { "mousebg.png" } else { "bg.png" }
    Copy-Item (Join-Path $modeSource "resources\background.png") `
        (Join-Path $modeRoot $backgroundName)
}
$fixtures += [pscustomobject]@{ Name="nested-package"; Path=$package; Valid=$true; Count=3 }

$legacy = Join-Path $OutputDir "legacy-package"
$legacyImage = Join-Path $legacy "img"
New-Item -ItemType Directory -Force -Path $legacyImage | Out-Null
@{
    standard = @{ hand = @(@(65, 66), (,16), (,16)) }
    keyboard = @{ lefthand = @(@(65, 66), (,16)); righthand = @((,37), (,16))
        keyboard = @((,66), (,37)) }
    gamepad = @{ lefthand = ,@(10, 11); righthand = ,@(0)
        keyboard = @((,0), (,11)) }
} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $legacy "config.json")
foreach ($mode in @("standard", "keyboard", "gamepad")) {
    $modeSource = Join-Path $root "resources\assets\models\$mode"
    $modeRoot = Join-Path $legacyImage $mode
    $modelRoot = Join-Path $modeRoot "cat_model"
    New-Item -ItemType Directory -Force -Path $modeRoot | Out-Null
    Copy-Item -LiteralPath $modeSource -Destination $modelRoot -Recurse
    Remove-Item -LiteralPath (Join-Path $modelRoot "resources") -Recurse -Force
    Copy-Item (Join-Path $modeSource "resources\cover.png") (Join-Path $modeRoot "cat.png")
    $backgroundName = if ($mode -eq "standard") { "mousebg.png" } else { "bg.png" }
    Copy-Item (Join-Path $modeSource "resources\background.png") `
        (Join-Path $modeRoot $backgroundName)
    if ($mode -eq "standard") {
        New-Item -ItemType Directory -Force -Path (Join-Path $modeRoot "hand") | Out-Null
        Copy-Item (Join-Path $modeSource "resources\left-keys\KeyA.png") `
            (Join-Path $modeRoot "hand\0.png")
        New-PixelPng (Join-Path $modeRoot "hand\1.png") 255 0 0 255
        New-PixelPng (Join-Path $modeRoot "hand\2.png") 0 255 0 255
        New-Item -ItemType Directory -Force `
            -Path (Join-Path $modeRoot "mousebg-assets") | Out-Null
        Copy-Item (Join-Path $modeSource "resources\cover.png") `
            (Join-Path $modeRoot "mousebg-assets\mousebg.png")
        continue
    }
    foreach ($directory in @("keyboard", "lefthand", "righthand")) {
        New-Item -ItemType Directory -Force -Path (Join-Path $modeRoot $directory) | Out-Null
    }
    $leftName = if ($mode -eq "gamepad") { "DPadLeft.png" } else { "KeyA.png" }
    $rightName = if ($mode -eq "gamepad") { "South.png" } else { "LeftArrow.png" }
    Copy-Item (Join-Path $modeSource "resources\left-keys\$leftName") `
        (Join-Path $modeRoot "lefthand\0.png")
    Copy-Item (Join-Path $modeSource "resources\right-keys\$rightName") `
        (Join-Path $modeRoot "righthand\0.png")
    $leftKeyboard = if ($mode -eq "gamepad") {
        "resources\right-keys\North.png"
    } else { "resources\left-keys\KeyB.png" }
    $rightKeyboard = if ($mode -eq "gamepad") {
        "resources\left-keys\DPadUp.png"
    } else { "resources\right-keys\UpArrow.png" }
    Copy-Item (Join-Path $modeSource $leftKeyboard) `
        (Join-Path $modeRoot "keyboard\0.png")
    Copy-Item (Join-Path $modeSource $rightKeyboard) `
        (Join-Path $modeRoot "keyboard\1.png")
    if ($mode -eq "keyboard") {
        New-PixelPng (Join-Path $modeRoot "lefthand\0.png") 255 0 0 128
        New-PixelPng (Join-Path $modeRoot "keyboard\0.png") 0 0 255 128
        New-PixelPng (Join-Path $modeRoot "lefthand\1.png") 255 64 0 255
        New-PixelPng (Join-Path $modeRoot "righthand\1.png") 0 255 64 255
    }
}
$standardKeyHash = (Get-FileHash (Join-Path $legacyImage `
    "standard\hand\0.png") -Algorithm SHA256).Hash
$shiftLeftHash = (Get-FileHash (Join-Path $legacyImage `
    "standard\hand\1.png") -Algorithm SHA256).Hash
$shiftRightHash = (Get-FileHash (Join-Path $legacyImage `
    "standard\hand\2.png") -Algorithm SHA256).Hash
$keyboardDirectHash = (Get-FileHash (Join-Path $legacyImage `
    "keyboard\lefthand\0.png") -Algorithm SHA256).Hash
$gamepadDirectHash = (Get-FileHash (Join-Path $legacyImage `
    "gamepad\lefthand\0.png") -Algorithm SHA256).Hash
$keyboardShiftLeftHash = (Get-FileHash (Join-Path $legacyImage `
    "keyboard\lefthand\1.png") -Algorithm SHA256).Hash
$keyboardShiftRightHash = (Get-FileHash (Join-Path $legacyImage `
    "keyboard\righthand\1.png") -Algorithm SHA256).Hash
$legacyBackgroundHashes = @{
    standard = (Get-FileHash (Join-Path $legacyImage "standard\mousebg.png") -Algorithm SHA256).Hash
    keyboard = (Get-FileHash (Join-Path $legacyImage "keyboard\bg.png") -Algorithm SHA256).Hash
    gamepad = (Get-FileHash (Join-Path $legacyImage "gamepad\bg.png") -Algorithm SHA256).Hash
}
$legacyCoverHashes = @{
    standard = (Get-FileHash (Join-Path $legacyImage "standard\cat.png") -Algorithm SHA256).Hash
    keyboard = (Get-FileHash (Join-Path $legacyImage "keyboard\cat.png") -Algorithm SHA256).Hash
    gamepad = (Get-FileHash (Join-Path $legacyImage "gamepad\cat.png") -Algorithm SHA256).Hash
}
$fixtures += [pscustomobject]@{ Name="legacy-package"; Path=$legacy
    Valid=$true; Count=3; Legacy=$true; StandardKeyHash=$standardKeyHash
    ShiftLeftHash=$shiftLeftHash; ShiftRightHash=$shiftRightHash
    KeyboardShiftLeftHash=$keyboardShiftLeftHash
    KeyboardShiftRightHash=$keyboardShiftRightHash
    KeyboardDirectHash=$keyboardDirectHash; GamepadDirectHash=$gamepadDirectHash
    BackgroundHashes=$legacyBackgroundHashes; CoverHashes=$legacyCoverHashes }
$legacyMalformed = Join-Path $OutputDir "legacy-malformed-mode"
Copy-Item -LiteralPath $legacy -Destination $legacyMalformed -Recurse
@{ standard = @{ hand = "broken" } } | ConvertTo-Json -Depth 4 | Set-Content `
    (Join-Path $legacyMalformed "config.json")
$fixtures += [pscustomobject]@{ Name="legacy-malformed-mode"; Path=$legacyMalformed
    Valid=$false; Count=0 }
$legacyInvalid = Join-Path $OutputDir "legacy-invalid-json"
Copy-Item -LiteralPath $legacy -Destination $legacyInvalid -Recurse
Set-Content -LiteralPath (Join-Path $legacyInvalid "config.json") -Value "{" -NoNewline
$fixtures += [pscustomobject]@{ Name="legacy-invalid-json"; Path=$legacyInvalid
    Valid=$false; Count=0 }
$legacyCorrupt = Join-Path $OutputDir "legacy-corrupt-input"
Copy-Item -LiteralPath $legacy -Destination $legacyCorrupt -Recurse
[IO.File]::WriteAllBytes((Join-Path $legacyCorrupt "img\standard\hand\0.png"),
    [byte[]](0x89, 0x50, 0x4e, 0x47))
$fixtures += [pscustomobject]@{ Name="legacy-corrupt-input"; Path=$legacyCorrupt
    Valid=$false; Count=0 }
$legacyCorruptSecond = Join-Path $OutputDir "legacy-corrupt-second-model"
Copy-Item -LiteralPath $legacy -Destination $legacyCorruptSecond -Recurse
[IO.File]::WriteAllBytes((Join-Path $legacyCorruptSecond "img\keyboard\lefthand\0.png"),
    [byte[]](0x89, 0x50, 0x4e, 0x47))
$fixtures += [pscustomobject]@{ Name="legacy-corrupt-second-model"
    Path=$legacyCorruptSecond; Valid=$false; Count=0 }

$localPreview = Join-Path $OutputDir "local-preview-precedence"
$localModel = Join-Path $localPreview "model"
New-Item -ItemType Directory -Force -Path $localPreview | Out-Null
Copy-Item -LiteralPath $source -Destination $localModel -Recurse
$parentResources = Join-Path $localPreview "resources"
New-Item -ItemType Directory -Force -Path $parentResources | Out-Null
Copy-Item (Join-Path $root "resources\assets\models\keyboard\resources\cover.png") `
    (Join-Path $parentResources "cover.png")
$expectedCoverHash = (Get-FileHash (Join-Path $source "resources\cover.png") -Algorithm SHA256).Hash
$fixtures += [pscustomobject]@{ Name="local-preview-precedence"; Path=$localPreview
    Valid=$true; Count=1; CoverHash=$expectedCoverHash }

$missingLocalCover = Join-Path $OutputDir "missing-local-cover"
$missingLocalModel = Join-Path $missingLocalCover "model"
New-Item -ItemType Directory -Force -Path $missingLocalCover | Out-Null
Copy-Item -LiteralPath $source -Destination $missingLocalModel -Recurse
Remove-Item -LiteralPath (Join-Path $missingLocalModel "resources\cover.png")
$fallbackResources = Join-Path $missingLocalCover "resources"
New-Item -ItemType Directory -Force -Path $fallbackResources | Out-Null
$fallbackCover = Join-Path $root "resources\assets\models\keyboard\resources\cover.png"
Copy-Item $fallbackCover (Join-Path $fallbackResources "cover.png")
$fallbackCoverHash = (Get-FileHash $fallbackCover -Algorithm SHA256).Hash
$fixtures += [pscustomobject]@{ Name="missing-local-cover"; Path=$missingLocalCover
    Valid=$true; Count=1; CoverHash=$fallbackCoverHash }

$mixedPreview = Join-Path $OutputDir "mixed-preview-layout"
$mixedModel = Join-Path $mixedPreview "model"
New-Item -ItemType Directory -Force -Path $mixedPreview | Out-Null
Copy-Item -LiteralPath $source -Destination $mixedModel -Recurse
Remove-Item -LiteralPath (Join-Path $mixedModel "resources\cover.png")
$mixedResources = Join-Path $mixedPreview "resources"
New-Item -ItemType Directory -Force -Path $mixedResources | Out-Null
Set-Content -LiteralPath (Join-Path $mixedResources "metadata.txt") -Value "preview"
Copy-Item $fallbackCover (Join-Path $mixedPreview "cat.png")
$fixtures += [pscustomobject]@{ Name="mixed-preview-layout"; Path=$mixedPreview
    Valid=$true; Count=1; CoverHash=$fallbackCoverHash }

$results = foreach ($fixture in $fixtures) {
    $data = Join-Path $OutputDir ("data-" + $fixture.Name)
    $process = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru `
        -WindowStyle Hidden -ArgumentList @("--ci-smoke", "--ci-exit-ms=1200",
            "--data-root=$data", "--ci-import=$($fixture.Path)")
    $process.WaitForExit()
    $models = @(Get-ChildItem (Join-Path $data "custom-models") -Directory `
        -ErrorAction SilentlyContinue)
    $installed = $models.Count
    $covers = @($models | Where-Object {
        Test-Path (Join-Path $_.FullName "resources\cover.png")
    }).Count
    $actualCoverHash = if ($models.Count -eq 1 -and $covers -eq 1) {
        (Get-FileHash (Join-Path $models[0].FullName "resources\cover.png") `
            -Algorithm SHA256).Hash
    } else { "" }
    $coverMatches = -not $fixture.CoverHash -or $actualCoverHash -eq $fixture.CoverHash
    $legacyKeys = 0
    $legacyMatches = $true
    if ($fixture.Legacy) {
        $expected = @{
            standard = "resources\left-keys\KeyA.png", "resources\left-keys\KeyB.png", `
                "resources\left-keys\ShiftLeft.png", "resources\left-keys\ShiftRight.png"
            keyboard = "resources\left-keys\KeyA.png", "resources\left-keys\KeyB.png", `
                "resources\left-keys\ShiftLeft.png", "resources\right-keys\LeftArrow.png", `
                "resources\right-keys\ShiftRight.png"
            gamepad = "resources\left-keys\DPadLeft.png", "resources\left-keys\DPadRight.png", "resources\right-keys\South.png"
        }
        $direct = @{
            standard = "resources\left-keys\KeyA.png", "resources\left-keys\KeyB.png"
            keyboard = "resources\left-keys\KeyA.png"
            gamepad = "resources\left-keys\DPadLeft.png"
        }
        foreach ($model in $models) {
            $modePath = Join-Path $model.FullName ".bongo-cat-neo-mode"
            $mode = if (Test-Path $modePath) { (Get-Content $modePath -Raw).Trim() } else { "" }
            $backgroundPath = Join-Path $model.FullName "resources\background.png"
            $coverPath = Join-Path $model.FullName "resources\cover.png"
            $legacyMatches = $legacyMatches -and (Test-Path $backgroundPath) -and
                (Get-FileHash $backgroundPath -Algorithm SHA256).Hash -eq
                    $fixture.BackgroundHashes[$mode]
            $legacyMatches = $legacyMatches -and (Test-Path $coverPath) -and
                (Get-FileHash $coverPath -Algorithm SHA256).Hash -eq $fixture.CoverHashes[$mode]
            if ($mode -eq "keyboard") {
                $legacyMatches = $legacyMatches -and
                    -not (Test-Path (Join-Path $model.FullName "resources\left-keys\ShiftRight.png")) -and
                    -not (Test-Path (Join-Path $model.FullName "resources\right-keys\ShiftLeft.png"))
            }
            foreach ($relative in @($expected[$mode])) {
                $keyPath = Join-Path $model.FullName $relative
                if (Test-Path $keyPath) { $legacyKeys++ } else { $legacyMatches = $false }
                if ($mode -eq "standard" -and (Test-Path $keyPath)) {
                    $expectedHash = if ($relative -like "*ShiftLeft.png") {
                        $fixture.ShiftLeftHash
                    } elseif ($relative -like "*ShiftRight.png") {
                        $fixture.ShiftRightHash
                    } else { $fixture.StandardKeyHash }
                    $legacyMatches = $legacyMatches -and (Get-FileHash $keyPath `
                        -Algorithm SHA256).Hash -eq $expectedHash
                }
                if ($relative -in @($direct[$mode]) -and (Test-Path $keyPath)) {
                    $expectedHash = if ($mode -eq "keyboard") { $fixture.KeyboardDirectHash } `
                        elseif ($mode -eq "gamepad") { $fixture.GamepadDirectHash } `
                        else { $fixture.StandardKeyHash }
                    $legacyMatches = $legacyMatches -and
                        (Get-FileHash $keyPath -Algorithm SHA256).Hash -eq $expectedHash
                }
                if ($mode -eq "keyboard" -and $relative -like "*KeyB.png" -and
                    (Test-Path $keyPath)) {
                    $bitmap = [Drawing.Bitmap]::new($keyPath)
                    $pixel = $bitmap.GetPixel(0, 0)
                    $bitmap.Dispose()
                    $legacyMatches = $legacyMatches -and $pixel.R -eq 170 -and
                        $pixel.G -eq 0 -and $pixel.B -eq 85 -and $pixel.A -eq 192
                }
                if ($mode -eq "keyboard" -and $relative -like "*Shift*.png" -and
                    (Test-Path $keyPath)) {
                    $hash = if ($relative -like "*ShiftLeft.png") {
                        $fixture.KeyboardShiftLeftHash
                    } else { $fixture.KeyboardShiftRightHash }
                    $legacyMatches = $legacyMatches -and
                        (Get-FileHash $keyPath -Algorithm SHA256).Hash -eq $hash
                }
            }
        }
    }
    $passed = if ($fixture.Valid) {
        $process.ExitCode -eq 0 -and $installed -eq $fixture.Count -and
            ($fixture.Name -ne "nested-package" -or $covers -eq 3) -and
            $coverMatches -and
            (-not $fixture.Legacy -or ($legacyMatches -and $legacyKeys -eq 12))
    } else {
        $process.ExitCode -ne 0 -and $installed -eq 0
    }
    [pscustomobject]@{ Case=$fixture.Name; ExpectedValid=$fixture.Valid
        ExitCode=$process.ExitCode; Installed=$installed; Covers=$covers
        CoverMatches=$coverMatches; Passed=$passed }
}
$results | Export-Csv (Join-Path $OutputDir "audit.csv") -NoTypeInformation -Encoding UTF8
$results | Format-Table -AutoSize
if (@($results | Where-Object { -not $_.Passed }).Count) { exit 1 }
