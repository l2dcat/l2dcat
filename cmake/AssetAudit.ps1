param([string]$ModelRoot = "", [string]$OutputDir = "")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $ModelRoot) { $ModelRoot = Join-Path $root "resources\assets\models" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build\asset-audit" }
$ModelRoot = [IO.Path]::GetFullPath($ModelRoot)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Add-Type -AssemblyName System.Drawing

$rows = foreach ($file in Get-ChildItem $ModelRoot -Recurse -Filter "*.png" -File) {
    $bitmap = [Drawing.Bitmap]::new($file.FullName)
    try {
        $transparent = 0
        $visible = 0
        for ($y = 0; $y -lt $bitmap.Height; $y += 8) {
            for ($x = 0; $x -lt $bitmap.Width; $x += 8) {
                if ($bitmap.GetPixel($x, $y).A -eq 0) { $transparent++ }
                else { $visible++ }
            }
        }
        $relative = $file.FullName.Substring($ModelRoot.Length + 1)
        $kind = if ($relative -match "keys\\") { "overlay" }
            elseif ($file.Name -eq "cover.png") { "cover" }
            elseif ($file.Name -eq "background.png") { "background" }
            else { "texture" }
        [pscustomobject]@{ File=$relative; Kind=$kind; Width=$bitmap.Width
            Height=$bitmap.Height; TransparentSamples=$transparent; VisibleSamples=$visible }
    } finally { $bitmap.Dispose() }
}
$models = @(Get-ChildItem $ModelRoot -Directory)
$overlays = @($rows | Where-Object Kind -eq "overlay")
$bad = @($overlays | Where-Object { $_.Width -ne 612 -or $_.Height -ne 354 -or
    $_.TransparentSamples -eq 0 -or $_.VisibleSamples -eq 0 })
$missing = @($models | Where-Object {
    -not (Test-Path (Join-Path $_.FullName "resources\cover.png")) -or
    -not (Test-Path (Join-Path $_.FullName "resources\background.png")) -or
    -not @(Get-ChildItem $_.FullName -Filter "*.model3.json" -File).Count
})
$rows | Export-Csv (Join-Path $OutputDir "assets.csv") -NoTypeInformation -Encoding UTF8
$passed = $models.Count -eq 3 -and $overlays.Count -eq 126 -and
    $bad.Count -eq 0 -and $missing.Count -eq 0
[pscustomobject]@{ Models=$models.Count; AllPng=$rows.Count; Overlays=$overlays.Count
    BadOverlays=$bad.Count; MissingRequired=$missing.Count; Passed=$passed
    Report=(Join-Path $OutputDir "assets.csv") } | Format-List
if (-not $passed) { $bad + $missing | Format-Table -AutoSize; exit 1 }
