param([string]$Exe = "", [string]$OutputDir = "")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build\l2dcat.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build\theme-import-audit" }
$Exe = [IO.Path]::GetFullPath($Exe)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$source = Join-Path $root "resources\assets\models\standard"
Remove-Item -LiteralPath $OutputDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

function New-Fixture([string]$Name) {
    $path = Join-Path $OutputDir $Name
    Copy-Item -LiteralPath $source -Destination $path -Recurse
    return $path
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
$missingTexture = New-Fixture "missing-texture"
Remove-Item -LiteralPath (Join-Path $missingTexture "demomodel.1024\texture_00.png")
$fixtures += [pscustomobject]@{ Name="missing-texture"; Path=$missingTexture; Valid=$false; Count=0 }
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
    $passed = if ($fixture.Valid) {
        $process.ExitCode -eq 0 -and $installed -eq $fixture.Count -and
            ($fixture.Name -ne "nested-package" -or $covers -eq 3)
    } else {
        $process.ExitCode -ne 0 -and $installed -eq 0
    }
    [pscustomobject]@{ Case=$fixture.Name; ExpectedValid=$fixture.Valid
        ExitCode=$process.ExitCode; Installed=$installed; Covers=$covers; Passed=$passed }
}
$results | Export-Csv (Join-Path $OutputDir "audit.csv") -NoTypeInformation -Encoding UTF8
$results | Format-Table -AutoSize
if (@($results | Where-Object { -not $_.Passed }).Count) { exit 1 }
