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
$fixtures += [pscustomobject]@{ Name="valid"; Path=(New-Fixture "valid"); Valid=$true }
$malformed = New-Fixture "malformed-json"
Set-Content -LiteralPath (Join-Path $malformed "cat.model3.json") -Value "{" -NoNewline
$fixtures += [pscustomobject]@{ Name="malformed-json"; Path=$malformed; Valid=$false }
$missingCover = New-Fixture "missing-cover"
Remove-Item -LiteralPath (Join-Path $missingCover "resources\cover.png")
$fixtures += [pscustomobject]@{ Name="missing-cover"; Path=$missingCover; Valid=$true }
$missingBackground = New-Fixture "missing-background"
Remove-Item -LiteralPath (Join-Path $missingBackground "resources\background.png")
$fixtures += [pscustomobject]@{ Name="missing-background"; Path=$missingBackground; Valid=$true }
$missingTexture = New-Fixture "missing-texture"
Remove-Item -LiteralPath (Join-Path $missingTexture "demomodel.1024\texture_00.png")
$fixtures += [pscustomobject]@{ Name="missing-texture"; Path=$missingTexture; Valid=$false }
$traversal = New-Fixture "path-traversal"
$manifest = Get-Content (Join-Path $traversal "cat.model3.json") -Raw |
    ConvertFrom-Json
$manifest.FileReferences.Moc = "../outside.moc3"
$manifest | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $traversal "cat.model3.json")
$fixtures += [pscustomobject]@{ Name="path-traversal"; Path=$traversal; Valid=$false }

$results = foreach ($fixture in $fixtures) {
    $data = Join-Path $OutputDir ("data-" + $fixture.Name)
    $process = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru `
        -WindowStyle Hidden -ArgumentList @("--ci-smoke", "--ci-exit-ms=1200",
            "--data-root=$data", "--ci-import=$($fixture.Path)")
    $process.WaitForExit()
    $installed = @(Get-ChildItem (Join-Path $data "custom-models") -Directory `
        -ErrorAction SilentlyContinue).Count
    $passed = if ($fixture.Valid) {
        $process.ExitCode -eq 0 -and $installed -eq 1
    } else {
        $process.ExitCode -ne 0 -and $installed -eq 0
    }
    [pscustomobject]@{ Case=$fixture.Name; ExpectedValid=$fixture.Valid
        ExitCode=$process.ExitCode; Installed=$installed; Passed=$passed }
}
$results | Export-Csv (Join-Path $OutputDir "audit.csv") -NoTypeInformation -Encoding UTF8
$results | Format-Table -AutoSize
if (@($results | Where-Object { -not $_.Passed }).Count) { exit 1 }
