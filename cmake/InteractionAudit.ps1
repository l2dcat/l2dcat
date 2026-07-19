param(
    [string]$Exe = "",
    [string]$DataRoot = "$env:APPDATA\l2dcat\l2dcat",
    [string]$Model = "model-32027d288-standard-1",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build-cubism\Release\l2dcat.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build-cubism\interaction-audit" }
$Exe = [IO.Path]::GetFullPath($Exe)
$DataRoot = [IO.Path]::GetFullPath($DataRoot)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$frame = Join-Path $DataRoot "frame.bmp"
$inputAudit = Join-Path $DataRoot "input-audit.txt"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class L2DCatInteractionNative {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect r);
    [DllImport("user32.dll", EntryPoint="GetWindowLongPtrW")]
    public static extern IntPtr GetWindowLongPtr(IntPtr h, int n);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void keybd_event(
        byte key, byte scan, uint flags, UIntPtr extra);
}
'@

function Wait-Window([Diagnostics.Process]$Process) {
    for ($index = 0; $index -lt 100; $index++) {
        $Process.Refresh()
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return $Process.MainWindowHandle
        }
        Start-Sleep -Milliseconds 40
    }
    throw "l2dcat window was not created"
}

function Wait-Frame {
    for ($index = 0; $index -lt 100 -and -not (Test-Path $frame); $index++) {
        Start-Sleep -Milliseconds 30
    }
    if (-not (Test-Path $frame)) { throw "l2dcat frame audit was not created" }
}

function Wait-NewFrame([DateTime]$Previous) {
    for ($index = 0; $index -lt 100; $index++) {
        if ((Get-Item $frame).LastWriteTimeUtc -gt $Previous) { return }
        Start-Sleep -Milliseconds 2
    }
    throw "l2dcat did not render a new frame"
}

function Copy-Frame([string]$Destination) {
    for ($index = 0; $index -lt 40; $index++) {
        try { Copy-Item $frame $Destination -Force; return }
        catch { Start-Sleep -Milliseconds 25 }
    }
    throw "Cannot copy the current frame"
}

function Send-Alt1 {
    [L2DCatInteractionNative]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
    [L2DCatInteractionNative]::keybd_event(0x31, 0, 0, [UIntPtr]::Zero)
    [L2DCatInteractionNative]::keybd_event(0x31, 0, 2, [UIntPtr]::Zero)
    [L2DCatInteractionNative]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
}

function Measure-FrameDifference([string]$First, [string]$Second) {
    $left = [Drawing.Bitmap]::new($First)
    $right = [Drawing.Bitmap]::new($Second)
    try {
        $changed = 0; $samples = 0
        for ($y = 0; $y -lt $left.Height; $y += 3) {
            for ($x = 0; $x -lt $left.Width; $x += 3) {
                $a = $left.GetPixel($x, $y); $b = $right.GetPixel($x, $y)
                $delta = [Math]::Abs($a.R - $b.R) + [Math]::Abs($a.G - $b.G) +
                    [Math]::Abs($a.B - $b.B) + [Math]::Abs($a.A - $b.A)
                if ($delta -gt 12) { $changed++ }
                $samples++
            }
        }
        return $changed / [double]$samples
    } finally { $left.Dispose(); $right.Dispose() }
}

function Find-AlphaPoints([string]$Path) {
    $bitmap = [Drawing.Bitmap]::new($Path)
    try {
        $transparent = $null; $opaque = $null
        for ($y = 4; $y -lt $bitmap.Height - 4; $y += 4) {
            for ($x = 4; $x -lt $bitmap.Width - 4; $x += 4) {
                $pixel = $bitmap.GetPixel($x, $y)
                $interior = $x -gt $bitmap.Width * 0.05 -and
                    $x -lt $bitmap.Width * 0.95 -and $y -gt $bitmap.Height * 0.05 -and
                    $y -lt $bitmap.Height * 0.95
                if ($null -eq $transparent -and $interior -and $pixel.A -lt 8) {
                    $transparent = @($x, $y)
                }
                $central = $x -gt $bitmap.Width * 0.25 -and $x -lt $bitmap.Width * 0.75 -and
                    $y -gt $bitmap.Height * 0.35 -and $y -lt $bitmap.Height * 0.85
                if ($null -eq $opaque -and $central -and $pixel.A -gt 240 -and
                    ($pixel.R + $pixel.G + $pixel.B) -gt 500) { $opaque = @($x, $y) }
                if ($null -ne $transparent -and $null -ne $opaque) {
                    return @($transparent, $opaque, @($bitmap.Width, $bitmap.Height))
                }
            }
        }
        throw "Frame does not contain transparent and opaque pixels"
    } finally { $bitmap.Dispose() }
}

function Measure-WatermarkInk([string]$Path) {
    $bitmap = [Drawing.Bitmap]::new($Path)
    try {
        $white = 0; $samples = 0
        for ($y = 15; $y -lt [Math]::Min(300, $bitmap.Height); $y += 3) {
            for ($x = 90; $x -lt [Math]::Min(500, $bitmap.Width); $x += 3) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ($pixel.A -gt 220 -and $pixel.R -gt 220 -and
                    $pixel.G -gt 220 -and $pixel.B -gt 220) { $white++ }
                $samples++
            }
        }
        return $white / [double]$samples
    } finally { $bitmap.Dispose() }
}

function Test-ClickThrough([IntPtr]$Window, [int]$X, [int]$Y) {
    [void][L2DCatInteractionNative]::SetCursorPos($X, $Y)
    Start-Sleep -Milliseconds 350
    return ([L2DCatInteractionNative]::GetWindowLongPtr($Window, -20).ToInt64() -band 0x20) -ne 0
}

Get-Process l2dcat -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $frame -Force -ErrorAction SilentlyContinue
Remove-Item $inputAudit -Force -ErrorAction SilentlyContinue
$arguments = @("--ci-smoke", "--ci-input-audit", "--ci-exit-ms=14000",
    "--ci-model=$Model", "--data-root=$DataRoot")
$process = Start-Process $Exe -ArgumentList $arguments -WorkingDirectory `
    (Split-Path $Exe) -PassThru
try {
    $window = Wait-Window $process
    $rect = [L2DCatInteractionNative+Rect]::new()
    [void][L2DCatInteractionNative]::GetWindowRect($window, [ref]$rect)
    Wait-Frame; Start-Sleep -Milliseconds 250
    $baseline = Join-Path $OutputDir "toggle-0.bmp"; Copy-Frame $baseline
    Send-Alt1; Start-Sleep -Milliseconds 500
    $hidden = Join-Path $OutputDir "toggle-1.bmp"; Copy-Frame $hidden
    $previousFrame = (Get-Item $frame).LastWriteTimeUtc
    Send-Alt1; Wait-NewFrame $previousFrame
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $restored = @{}
    foreach ($delay in 15, 60, 150, 500) {
        while ($watch.ElapsedMilliseconds -lt $delay) { Start-Sleep -Milliseconds 2 }
        $restored[$delay] = Join-Path $OutputDir ("toggle-2-{0:D3}.bmp" -f $delay)
        Copy-Frame $restored[$delay]
    }
    Send-Alt1; Start-Sleep -Milliseconds 500
    $hiddenAgain = Join-Path $OutputDir "toggle-3.bmp"; Copy-Frame $hiddenAgain

    $points = Find-AlphaPoints $hiddenAgain
    $width = $rect.R - $rect.L; $height = $rect.B - $rect.T; $size = $points[2]
    $transparentX = $rect.L + [int](($points[0][0] + 0.5) * $width / $size[0])
    $transparentY = $rect.T + [int](($points[0][1] + 0.5) * $height / $size[1])
    $opaqueX = $rect.L + [int](($points[1][0] + 0.5) * $width / $size[0])
    $opaqueY = $rect.T + [int](($points[1][1] + 0.5) * $height / $size[1])
    $transparentPasses = Test-ClickThrough $window $transparentX $transparentY
    $opaquePasses = Test-ClickThrough $window $opaqueX $opaqueY
    $process.Refresh()
    $result = [ordered]@{
        BaselineVsHidden = Measure-FrameDifference $baseline $hidden
        Restore015VsHidden = Measure-FrameDifference $restored[15] $hidden
        Restore060VsHidden = Measure-FrameDifference $restored[60] $hidden
        Restore150VsHidden = Measure-FrameDifference $restored[150] $hidden
        Restore500VsHidden = Measure-FrameDifference $restored[500] $hidden
        ThirdVsHidden = Measure-FrameDifference $hiddenAgain $hidden
        BaselineWatermarkInk = Measure-WatermarkInk $baseline
        HiddenWatermarkInk = Measure-WatermarkInk $hidden
        Restore015WatermarkInk = Measure-WatermarkInk $restored[15]
        Restore060WatermarkInk = Measure-WatermarkInk $restored[60]
        Restore150WatermarkInk = Measure-WatermarkInk $restored[150]
        Restore500WatermarkInk = Measure-WatermarkInk $restored[500]
        ThirdWatermarkInk = Measure-WatermarkInk $hiddenAgain
        TransparentPixelPassThrough = $transparentPasses
        OpaquePixelPassThrough = $opaquePasses
        TransparentPoint = "$transparentX,$transparentY"
        OpaquePoint = "$opaqueX,$opaqueY"
        WindowRect = "$($rect.L),$($rect.T),$($rect.R),$($rect.B)"
        WorkingSetMiB = [Math]::Round($process.WorkingSet64 / 1MB, 2)
        MouseAuditLines = if (Test-Path $inputAudit) {
            @(Get-Content $inputAudit | Where-Object { $_ -like "mouse*" }).Count
        } else { 0 }
    }
    $result | ConvertTo-Json | Set-Content (Join-Path $OutputDir "result.json")
    [pscustomobject]$result | Format-List
    $visibleInk = $result.HiddenWatermarkInk + 0.005
    $passed = $result.BaselineVsHidden -gt 0.05 -and
        $result.BaselineWatermarkInk -gt $visibleInk -and
        $result.Restore015WatermarkInk -gt $visibleInk -and
        $result.Restore060WatermarkInk -gt $visibleInk -and
        $result.Restore150WatermarkInk -gt $visibleInk -and
        $result.Restore500WatermarkInk -gt $visibleInk -and
        [Math]::Abs($result.ThirdWatermarkInk - $result.HiddenWatermarkInk) -lt 0.005 -and
        $transparentPasses -and -not $opaquePasses
    if (-not $passed) { exit 1 }
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
