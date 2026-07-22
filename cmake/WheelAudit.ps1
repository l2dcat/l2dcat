param([string]$Exe = "", [string]$OutputDir = "", [int]$ScaleDelta = 120,
    [int]$BurstCount = 1, [int]$BurstDelayMs = 0, [switch]$AtEdge, [switch]$GlobalControl,
    [switch]$SystemWheel, [switch]$ControlOpacity, [string]$DataRoot = "",
    [ValidateRange(10, 100)][double]$InitialOpacity = 50)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build-cubism\Release\l2dcat.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build-cubism\wheel-audit" }
$Exe = [IO.Path]::GetFullPath($Exe)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Add-Type -AssemblyName System.Drawing

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
    [DllImport("user32.dll")] public static extern bool SetWindowPos(
        IntPtr h, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern bool SystemParametersInfoW(
        uint action, uint parameter, out Rect value, uint update);
    [DllImport("user32.dll")] public static extern IntPtr GetShellWindow();
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern void keybd_event(
        byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(
        uint flags, uint x, uint y, int data, UIntPtr extraInfo);
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

function Get-VisiblePixels([string]$Path) {
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (Test-Path -LiteralPath $Path) {
            try {
                $bitmap = [Drawing.Bitmap]::new($Path)
                $visible = 0
                for ($y = 0; $y -lt $bitmap.Height; $y += 4) {
                    for ($x = 0; $x -lt $bitmap.Width; $x += 4) {
                        $color = $bitmap.GetPixel($x, $y)
                        if ($color.R + $color.G + $color.B -gt 60) { $visible++ }
                    }
                }
                $bitmap.Dispose()
                return $visible
            } catch { Start-Sleep -Milliseconds 25 }
        } else { Start-Sleep -Milliseconds 25 }
    }
    return 0
}

Get-Process l2dcat -ErrorAction SilentlyContinue | Stop-Process -Force
$data = if ($DataRoot) { [IO.Path]::GetFullPath($DataRoot) } else {
    Join-Path $OutputDir ("data-" + [DateTime]::UtcNow.Ticks)
}
$arguments = @("--ci-smoke", "--ci-frame-series", "--ci-exit-ms=9000",
    "--data-root=$data")
if ($ControlOpacity) {
    $config = Join-Path $OutputDir "opacity-settings.json"
    $json = @{ schemaVersion=2; window=@{ opacity=$InitialOpacity } } |
        ConvertTo-Json -Compress
    [IO.File]::WriteAllText($config, $json, [Text.UTF8Encoding]::new($false))
    $arguments += "--config=$config"
}
$frameSeries = Join-Path $data "frame-series.csv"
for ($attempt = 0; $attempt -lt 20; $attempt++) {
    try {
        if ([IO.File]::Exists($frameSeries)) { [IO.File]::Delete($frameSeries) }
        break
    } catch { Start-Sleep -Milliseconds 10 }
}
$process = Start-Process $Exe -ArgumentList $arguments `
    -WorkingDirectory (Split-Path $Exe) -PassThru
$globalControlDown = $false
try {
    $window = Wait-Window $process
    Start-Sleep -Milliseconds 400
    $initial = Get-Rect $window
    $workArea = [L2DCatWheelNative+Rect]::new()
    $workAreaAvailable = [L2DCatWheelNative]::SystemParametersInfoW(
        0x0030, 0, [ref]$workArea, 0)
    if ($AtEdge -and $workAreaAvailable) {
        [void][L2DCatWheelNative]::SetWindowPos($window, [IntPtr]::Zero,
            $workArea.R - ($initial.R - $initial.L),
            $workArea.B - ($initial.B - $initial.T), 0, 0, 0x0015)
        Start-Sleep -Milliseconds 100
        $initial = Get-Rect $window
    }
    $position = Position-LParam $initial
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

    $foregroundSeparated = $false
    if ($GlobalControl) {
        $shell = [L2DCatWheelNative]::GetShellWindow()
        if ($shell -ne [IntPtr]::Zero) {
            [void][L2DCatWheelNative]::SetForegroundWindow($shell)
            Start-Sleep -Milliseconds 120
        }
        $foregroundSeparated = [L2DCatWheelNative]::GetForegroundWindow() -ne $window
    }
    if ($ControlOpacity) {
        [L2DCatWheelNative]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
        $globalControlDown = $true; Start-Sleep -Milliseconds 80
    } elseif (-not $GlobalControl) {
        [void][L2DCatWheelNative]::PostMessageW(
            $window, 0x0101, [IntPtr]0x11, [IntPtr]::Zero)
    }
    $process.Refresh()
    $initialWorkingSet = $process.WorkingSet64
    $initialCpu = $process.TotalProcessorTime.TotalMilliseconds
    $peakWorkingSet = $initialWorkingSet
    $initialFrames = if (Test-Path -LiteralPath $frameSeries) {
        @(Import-Csv -LiteralPath $frameSeries)
    } else { @() }
    $frameStartCount = $initialFrames.Count
    $initialRecordedOpacity = if ($initialFrames.Count -and
        $null -ne $initialFrames[-1].opacity_percent) {
        [double]$initialFrames[-1].opacity_percent
    } else { [double]$InitialOpacity }
    $initialRenderWidth = if ($initialFrames.Count) {
        [double]$initialFrames[-1].width
    } else { 0.0 }
    $initialRenderHeight = if ($initialFrames.Count) {
        [double]$initialFrames[-1].height
    } else { 0.0 }
    $wheelKeys = if ($ControlOpacity) { 8 } else { 0 }
    if ($SystemWheel) {
        [void][L2DCatWheelNative]::SetCursorPos(
            [int](($initial.L + $initial.R) / 2), [int](($initial.T + $initial.B) / 2))
        Start-Sleep -Milliseconds 80
    }
    for ($index = 0; $index -lt [Math]::Max(1, $BurstCount); $index++) {
        if ($SystemWheel) {
            [L2DCatWheelNative]::mouse_event(
                0x0800, 0, 0, $ScaleDelta, [UIntPtr]::Zero)
        } else {
            [void][L2DCatWheelNative]::PostMessageW($window, 0x020A,
                (Wheel-WParam $ScaleDelta $wheelKeys), $position)
        }
        if ($BurstDelayMs -gt 0) { Start-Sleep -Milliseconds $BurstDelayMs }
    }
    $samples = [Collections.Generic.List[object]]::new()
    $sampleClock = [Diagnostics.Stopwatch]::StartNew()
    for ($index = 0; $index -lt 30; $index++) {
        $rect = Get-Rect $window
        $process.Refresh()
        if ($process.WorkingSet64 -gt $peakWorkingSet) {
            $peakWorkingSet = $process.WorkingSet64
        }
        $samples.Add([pscustomobject]@{ElapsedMs=$sampleClock.Elapsed.TotalMilliseconds
            Width=$rect.R-$rect.L; Height=$rect.B-$rect.T
            CenterX=($rect.L+$rect.R)/2.0; CenterY=($rect.T+$rect.B)/2.0})
        Start-Sleep -Milliseconds 16
    }
    if ($ControlOpacity) {
        [L2DCatWheelNative]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
        $globalControlDown = $false
    } elseif (-not $GlobalControl) {
        [void][L2DCatWheelNative]::PostMessageW(
            $window, 0x0101, [IntPtr]0x11, [IntPtr]::Zero)
    }
    Start-Sleep -Milliseconds 80
    $visiblePixels = Get-VisiblePixels (Join-Path $data "frame.bmp")
    $frameRows = if (Test-Path -LiteralPath $frameSeries) {
        @(@(Import-Csv -LiteralPath $frameSeries) | Select-Object -Skip $frameStartCount)
    } else { @() }
    $minimumFramePixels = if ($frameRows.Count) {
        ($frameRows | Measure-Object -Property visible_pixels -Minimum).Minimum
    } else { 0 }
    $uniqueRenderWidths = @($frameRows.width | Select-Object -Unique).Count
    $finalRenderWidth = if ($frameRows.Count) {
        [double]$frameRows[-1].width
    } else { $initialRenderWidth }
    $finalRenderHeight = if ($frameRows.Count) {
        [double]$frameRows[-1].height
    } else { $initialRenderHeight }
    $opacityRows = @($frameRows | Where-Object { $null -ne $_.opacity_percent })
    $uniqueRenderOpacities = @($opacityRows.opacity_percent | Select-Object -Unique).Count
    $finalRecordedOpacity = if ($opacityRows.Count) {
        [double]$opacityRows[-1].opacity_percent
    } else { $initialRecordedOpacity }
    $finalWindowOpacity = if ($opacityRows.Count -and
        $null -ne $opacityRows[-1].window_opacity) {
        100.0 * [double]$opacityRows[-1].window_opacity
    } else { $initialRecordedOpacity }
    $maxNormalizedRenderWidthStep = 0.0
    $maxRenderWidthStep = 0.0
    $oppositeRenderSteps = 0
    $normalizedRenderSteps = [Collections.Generic.List[double]]::new()
    $renderIntervals = [Collections.Generic.List[double]]::new()
    for ($index = 1; $index -lt $frameRows.Count; $index++) {
        $signedStep = [double]$frameRows[$index].width -
            [double]$frameRows[$index - 1].width
        $step = [Math]::Abs($signedStep)
        $elapsed = ([double]$frameRows[$index].ticks_ns -
            [double]$frameRows[$index - 1].ticks_ns) / 1000000.0
        if ($elapsed -gt 0) {
            $renderIntervals.Add($elapsed)
            $normalized = $step * 16.6667 / $elapsed
            if ($step -gt 0) { $normalizedRenderSteps.Add($normalized) }
            if ($normalized -gt $maxNormalizedRenderWidthStep) {
                $maxNormalizedRenderWidthStep = $normalized
            }
        }
        if ($step -gt $maxRenderWidthStep) { $maxRenderWidthStep = $step }
        if (($ScaleDelta -gt 0 -and $signedStep -lt 0) -or
            ($ScaleDelta -lt 0 -and $signedStep -gt 0)) { $oppositeRenderSteps++ }
    }
    $sortedSteps = @($normalizedRenderSteps | Sort-Object)
    $sortedIntervals = @($renderIntervals | Sort-Object)
    $stepP95 = if ($sortedSteps.Count) { $sortedSteps[
        [Math]::Ceiling(($sortedSteps.Count - 1) * 0.95)] } else { 0.0 }
    $intervalP95 = if ($sortedIntervals.Count) { $sortedIntervals[
        [Math]::Ceiling(($sortedIntervals.Count - 1) * 0.95)] } else { 0.0 }
    $maximumInterval = if ($sortedIntervals.Count) { $sortedIntervals[-1] } else { 0.0 }
    $firstRenderWidth = if ($frameRows.Count) { [double]$frameRows[0].width } else { 0.0 }
    $maxRenderWidthStepPercent = if ($firstRenderWidth -gt 0) {
        100.0 * $maxRenderWidthStep / $firstRenderWidth
    } else { 0.0 }
    if (Test-Path -LiteralPath $frameSeries) {
        Copy-Item -LiteralPath $frameSeries -Destination `
            (Join-Path $OutputDir "frame-series.csv") -Force
    }
    $process.Refresh()
    $final = $samples[$samples.Count - 1]
    $opacitySamples | Export-Csv (Join-Path $OutputDir "opacity-samples.csv") -NoTypeInformation
    $samples | Export-Csv (Join-Path $OutputDir "scale-samples.csv") -NoTypeInformation
    $uniqueWidths = @($samples.Width | Select-Object -Unique).Count
    $maxWidthStep = 0
    $maxNormalizedWidthStep = 0.0
    $settledAtMs = 0.0
    for ($index = 1; $index -lt $samples.Count; $index++) {
        $step = [Math]::Abs($samples[$index].Width - $samples[$index - 1].Width)
        if ($step -gt 0) { $settledAtMs = $samples[$index].ElapsedMs }
        if ($step -gt $maxWidthStep) { $maxWidthStep = $step }
        $elapsed = $samples[$index].ElapsedMs - $samples[$index - 1].ElapsedMs
        if ($elapsed -gt 0) {
            $normalized = $step * 16.6667 / $elapsed
            if ($normalized -gt $maxNormalizedWidthStep) {
                $maxNormalizedWidthStep = $normalized
            }
        }
    }
    $centerX = ($initial.L + $initial.R) / 2.0
    $centerY = ($initial.T + $initial.B) / 2.0
    $maximumCenterDriftX = ($samples | ForEach-Object {
        [Math]::Abs($_.CenterX - $centerX) } | Measure-Object -Maximum).Maximum
    $maximumCenterDriftY = ($samples | ForEach-Object {
        [Math]::Abs($_.CenterY - $centerY) } | Measure-Object -Maximum).Maximum
    $initialArea = ($initial.R - $initial.L) * ($initial.B - $initial.T)
    $finalArea = $final.Width * $final.Height
    $frameAreaRatio = if ($initialArea -gt 0) { $finalArea / [double]$initialArea } else { 1.0 }
    $allowedWorkingSetGrowthMB = 64.0 * [Math]::Max(1.0, $frameAreaRatio)
    $result = [ordered]@{
        OpacityAvailable=$opacityAvailable; OpacityAlpha=$alpha
        InitialOpacityPercent=$initialRecordedOpacity
        FinalOpacityPercent=$finalRecordedOpacity
        FinalWindowOpacityPercent=$finalWindowOpacity
        UniqueAnimatedOpacities=$uniqueRenderOpacities
        InitialRenderWidth=$initialRenderWidth; FinalRenderWidth=$finalRenderWidth
        InitialRenderHeight=$initialRenderHeight; FinalRenderHeight=$finalRenderHeight
        InitialWidth=$initial.R-$initial.L; FinalWidth=$final.Width
        InitialHeight=$initial.B-$initial.T; FinalHeight=$final.Height
        UniqueAnimatedWidths=$uniqueWidths
        MaxAnimatedWidthStep=$maxWidthStep
        MaxNormalizedWidthStep=$maxNormalizedWidthStep
        CenterDriftX=[Math]::Abs($final.CenterX-$centerX)
        CenterDriftY=[Math]::Abs($final.CenterY-$centerY)
        MaximumCenterDriftX=$maximumCenterDriftX
        MaximumCenterDriftY=$maximumCenterDriftY
        SettledAtMs=$settledAtMs
        BurstCount=$BurstCount
        BurstDelayMs=$BurstDelayMs
        GlobalControl=$GlobalControl.IsPresent
        SystemWheel=$SystemWheel.IsPresent
        ForegroundSeparated=(-not $GlobalControl -or $foregroundSeparated)
        PeakWorkingSetMB=$peakWorkingSet / 1MB
        WorkingSetGrowthMB=($peakWorkingSet-$initialWorkingSet) / 1MB
        FrameAreaRatio=$frameAreaRatio
        AllowedWorkingSetGrowthMB=$allowedWorkingSetGrowthMB
        ScaleCpuMs=$process.TotalProcessorTime.TotalMilliseconds-$initialCpu
        VisibleFramePixels=$visiblePixels
        CapturedRenderFrames=$frameRows.Count
        UniqueRenderWidths=$uniqueRenderWidths
        MaxNormalizedRenderWidthStep=$maxNormalizedRenderWidthStep
        NormalizedRenderWidthStepP95=$stepP95
        MaxRenderWidthStep=$maxRenderWidthStep
        MaxRenderWidthStepPercent=$maxRenderWidthStepPercent
        OppositeRenderSteps=$oppositeRenderSteps
        ChangedRenderFrames=$normalizedRenderSteps.Count
        RenderIntervalP95Ms=$intervalP95
        MaximumRenderIntervalMs=$maximumInterval
        MinimumFramePixels=[int]$minimumFramePixels
        WithinWorkArea=(-not $AtEdge -or -not $workAreaAvailable -or
            ($final.CenterX-$final.Width/2 -ge $workArea.L -and
            $final.CenterY-$final.Height/2 -ge $workArea.T -and
            $final.CenterX+$final.Width/2 -le $workArea.R -and
            $final.CenterY+$final.Height/2 -le $workArea.B))
    }
    $shortBurst = $BurstCount -le 3
    $expectedOpacity = if ($ControlOpacity) {
        [Math]::Max(10.0, [Math]::Min(100.0,
            $initialRecordedOpacity + [Math]::Sign($ScaleDelta) * 5.0 *
            [Math]::Max(1, $BurstCount)))
    } else { $initialRecordedOpacity }
    $result.ExpectedOpacityPercent = $expectedOpacity
    $renderDirectionPassed = $ControlOpacity -or
        ($ScaleDelta -gt 0 -and $finalRenderWidth -gt $initialRenderWidth -and
        $finalRenderHeight -gt $initialRenderHeight) -or
        ($ScaleDelta -lt 0 -and $finalRenderWidth -lt $initialRenderWidth -and
        $finalRenderHeight -lt $initialRenderHeight)
    $result.RenderDirectionPassed = $renderDirectionPassed
    $directionPassed = if ($ControlOpacity) {
        $final.Width -eq $result.InitialWidth -and
            $final.Height -eq $result.InitialHeight -and
            [Math]::Abs($finalRecordedOpacity - $expectedOpacity) -le 0.1 -and
            [Math]::Abs($finalWindowOpacity - $expectedOpacity) -le 1.0
    } elseif ($ScaleDelta -gt 0) {
        $final.Width -gt $result.InitialWidth -and $final.Height -gt $result.InitialHeight -and
            (-not $shortBurst -or $final.Width -le $result.InitialWidth * 1.2) -and
            $renderDirectionPassed
    } else {
        $final.Width -lt $result.InitialWidth -and $final.Height -lt $result.InitialHeight -and
            (-not $shortBurst -or $final.Width -ge $result.InitialWidth * 0.8) -and
            $renderDirectionPassed
    }
    $centerPassed = $AtEdge -or
        ($result.MaximumCenterDriftX -le 2 -and $result.MaximumCenterDriftY -le 2)
    $animationPassed = if ($ControlOpacity) {
        $uniqueRenderOpacities -ge 2
    } else { $uniqueWidths -ge 4 -or $uniqueRenderWidths -ge 4 }
    $smoothnessPassed = if ($ControlOpacity) {
        $uniqueRenderWidths -eq 1 -and $result.MaximumCenterDriftX -le 0.5 -and
            $result.MaximumCenterDriftY -le 0.5
    } elseif ($frameRows.Count -ge 2) {
        $maxNormalizedRenderWidthStep -le 20 -and $stepP95 -le 16 -and
            $maxRenderWidthStepPercent -le 3.0 -and $oppositeRenderSteps -eq 0
    } else { $maxNormalizedWidthStep -le 8 }
    $latencyPassed = $ControlOpacity -or -not $shortBurst -or $settledAtMs -le 350
    $result.Passed = $directionPassed -and
        $animationPassed -and $smoothnessPassed -and $latencyPassed -and $centerPassed -and
        $result.WithinWorkArea -and
        $result.WorkingSetGrowthMB -le $result.AllowedWorkingSetGrowthMB -and
        $result.ScaleCpuMs -le 1000 -and $result.ForegroundSeparated -and
        $result.VisibleFramePixels -ge 100 -and $result.CapturedRenderFrames -ge 2 -and
        $result.MinimumFramePixels -ge 100
    $result | ConvertTo-Json | Set-Content (Join-Path $OutputDir "result.json")
    [pscustomobject]$result | Format-List
    if (-not $result.Passed) { exit 1 }
} finally {
    if ($globalControlDown) {
        [L2DCatWheelNative]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
    }
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
