param([string]$Exe = "", [string]$OutputDir = "")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build-cubism\Release\l2dcat.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build-cubism\wheel-audit" }
$Exe = [IO.Path]::GetFullPath($Exe)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class L2DCatWheelNative {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect r);
    [DllImport("user32.dll")] public static extern bool PostMessageW(
        IntPtr h, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern bool GetLayeredWindowAttributes(
        IntPtr h, out uint color, out byte alpha, out uint flags);
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

function Get-Rect([IntPtr]$Window) {
    $rect = [L2DCatWheelNative+Rect]::new()
    if (-not [L2DCatWheelNative]::GetWindowRect($Window, [ref]$rect)) {
        throw "Cannot read l2dcat window bounds"
    }
    return $rect
}

function Wheel-WParam([int]$Delta, [int]$Keys = 0) {
    return [IntPtr]([int64](($Delta -band 0xffff) -shl 16) -bor $Keys)
}

function Position-LParam($Rect) {
    $x = [int](($Rect.L + $Rect.R) / 2) -band 0xffff
    $y = [int](($Rect.T + $Rect.B) / 2) -band 0xffff
    return [IntPtr]([int64](($y -shl 16) -bor $x))
}

Get-Process l2dcat -ErrorAction SilentlyContinue | Stop-Process -Force
$data = Join-Path $OutputDir ("data-" + [DateTime]::UtcNow.Ticks)
$process = Start-Process $Exe -ArgumentList @("--ci-smoke", "--ci-exit-ms=9000",
    "--data-root=$data") -WorkingDirectory (Split-Path $Exe) -PassThru
try {
    $window = Wait-Window $process
    Start-Sleep -Milliseconds 400
    $initial = Get-Rect $window
    $position = Position-LParam $initial
    [void][L2DCatWheelNative]::PostMessageW($window, 0x020A,
        (Wheel-WParam -120), $position)
    $opacitySamples = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt 30; $index++) {
        [uint32]$sampleColor = 0; [byte]$sampleAlpha = 255; [uint32]$sampleFlags = 0
        [void][L2DCatWheelNative]::GetLayeredWindowAttributes(
            $window, [ref]$sampleColor, [ref]$sampleAlpha, [ref]$sampleFlags)
        $opacitySamples.Add([pscustomobject]@{Index=$index; Alpha=$sampleAlpha})
        Start-Sleep -Milliseconds 16
    }
    [uint32]$color = 0; [byte]$alpha = 255; [uint32]$flags = 0
    $opacityAvailable = [L2DCatWheelNative]::GetLayeredWindowAttributes(
        $window, [ref]$color, [ref]$alpha, [ref]$flags)

    [void][L2DCatWheelNative]::PostMessageW($window, 0x0100, [IntPtr]0x11, [IntPtr]::Zero)
    [void][L2DCatWheelNative]::PostMessageW($window, 0x020A,
        (Wheel-WParam 120 8), $position)
    $samples = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt 30; $index++) {
        $rect = Get-Rect $window
        $samples.Add([pscustomobject]@{Width=$rect.R-$rect.L; Height=$rect.B-$rect.T
            CenterX=($rect.L+$rect.R)/2.0; CenterY=($rect.T+$rect.B)/2.0})
        Start-Sleep -Milliseconds 16
    }
    [void][L2DCatWheelNative]::PostMessageW($window, 0x0101, [IntPtr]0x11, [IntPtr]::Zero)
    $final = $samples[$samples.Count - 1]
    $opacitySamples | Export-Csv (Join-Path $OutputDir "opacity-samples.csv") -NoTypeInformation
    $samples | Export-Csv (Join-Path $OutputDir "scale-samples.csv") -NoTypeInformation
    $uniqueWidths = @($samples.Width | Select-Object -Unique).Count
    $centerX = ($initial.L + $initial.R) / 2.0
    $centerY = ($initial.T + $initial.B) / 2.0
    $result = [ordered]@{
        OpacityAvailable=$opacityAvailable; OpacityAlpha=$alpha
        InitialWidth=$initial.R-$initial.L; FinalWidth=$final.Width
        InitialHeight=$initial.B-$initial.T; FinalHeight=$final.Height
        UniqueAnimatedWidths=$uniqueWidths
        CenterDriftX=[Math]::Abs($final.CenterX-$centerX)
        CenterDriftY=[Math]::Abs($final.CenterY-$centerY)
    }
    $result.Passed = $opacityAvailable -and $alpha -lt 255 -and
        $final.Width -gt $result.InitialWidth -and $final.Height -gt $result.InitialHeight -and
        $uniqueWidths -ge 10 -and $result.CenterDriftX -le 2 -and $result.CenterDriftY -le 2
    $result | ConvertTo-Json | Set-Content (Join-Path $OutputDir "result.json")
    [pscustomobject]$result | Format-List
    if (-not $result.Passed) { exit 1 }
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
