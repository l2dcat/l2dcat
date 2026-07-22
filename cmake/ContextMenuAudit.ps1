param(
    [string]$Exe = "",
    [string]$OutputDir = "",
    [string]$Language = "zh-CN"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build\l2dcat.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build\context-menu-audit" }
$Exe = [IO.Path]::GetFullPath($Exe)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class L2DCatMenuNative {
    public delegate bool EnumProc(IntPtr handle, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr handle, out uint process);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr handle);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr handle, out Rect rect);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr handle, uint command);
    [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr handle, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll")] public static extern bool GetMenuItemRect(
        IntPtr window, IntPtr menu, uint item, out Rect rect);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetMenuStringW(
        IntPtr menu, uint item, StringBuilder text, int max, uint flags);
    [DllImport("user32.dll")] public static extern int GetClassNameW(IntPtr handle, StringBuilder text, int size);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr handle);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string className, string title);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowExW(
        IntPtr parent, IntPtr childAfter, string className, string title);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr handle, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern void keybd_event(
        byte key, byte scan, uint flags, UIntPtr extra);
}
'@
Add-Type -AssemblyName System.Drawing

function Get-Windows([int]$ProcessId) {
    $rows = [Collections.Generic.List[object]]::new()
    [L2DCatMenuNative]::EnumWindows({
        param($handle, $data)
        [uint32]$owner = 0
        [void][L2DCatMenuNative]::GetWindowThreadProcessId($handle, [ref]$owner)
        if ($owner -eq $ProcessId -and [L2DCatMenuNative]::IsWindowVisible($handle)) {
            $rect = [L2DCatMenuNative+Rect]::new()
            $class = [Text.StringBuilder]::new(64)
            [void][L2DCatMenuNative]::GetClassNameW($handle, $class, 64)
            if ([L2DCatMenuNative]::GetWindowRect($handle, [ref]$rect)) {
                $rows.Add([pscustomobject]@{Handle=$handle; Class=$class.ToString();
                    Rect=$rect; Area=($rect.R-$rect.L)*($rect.B-$rect.T)})
            }
        }
        return $true
    }, [IntPtr]::Zero) | Out-Null
    return $rows
}

function Get-MenuWindow([string]$ExpectedFirstLabel) {
    $after = [IntPtr]::Zero
    while ($true) {
        $handle = [L2DCatMenuNative]::FindWindowExW(
            [IntPtr]::Zero, $after, "#32768", $null)
        if ($handle -eq [IntPtr]::Zero) { break }
        $after = $handle
        if (-not [L2DCatMenuNative]::IsWindowVisible($handle)) { continue }
        $menu = [L2DCatMenuNative]::SendMessageW($handle, 0x01E1,
            [IntPtr]::Zero, [IntPtr]::Zero)
        if ($menu -eq [IntPtr]::Zero -or
            [L2DCatMenuNative]::GetMenuItemCount($menu) -lt 8) { continue }
        $text = [Text.StringBuilder]::new(128)
        [void][L2DCatMenuNative]::GetMenuStringW($menu, 0, $text, 128, 0x400)
        if ($text.ToString() -eq $ExpectedFirstLabel) {
            return $handle
        }
    }
    return [IntPtr]::Zero
}

Get-Process l2dcat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 350
$localePath = Join-Path $root "resources\assets\locales\$Language.json"
if (-not (Test-Path $localePath)) { $localePath = Join-Path $root "resources\assets\locales\en-US.json" }
$locale = Get-Content -Raw -Encoding utf8 $localePath | ConvertFrom-Json
$menuLabels = $locale.composables.useAppMenu.labels
$expected = @(
    if ($menuLabels.preference) { $menuLabels.preference } else { "Preferences..." }
    if ($menuLabels.hideCat) { $menuLabels.hideCat } else { "Hide Cat" }
    if ($menuLabels.passThrough) { $menuLabels.passThrough } else { "Pass Through" }
    if ($menuLabels.alwaysOnTop) { $menuLabels.alwaysOnTop } else { "Always on Top" }
    if ($menuLabels.windowSize) { $menuLabels.windowSize } else { "Window Size" }
    if ($menuLabels.opacity) { $menuLabels.opacity } else { "Opacity" }
    if ($menuLabels.model) { $menuLabels.model } else { "Model" }
    if ($menuLabels.quitApp) { $menuLabels.quitApp } else { "Quit App" }
)
$data = Join-Path $OutputDir ("data-" + [DateTime]::UtcNow.Ticks)
$process = Start-Process -FilePath $Exe -ArgumentList @("--ci-smoke", "--ci-context-menu",
    "--ci-language=$Language", "--ci-exit-ms=15000", "--data-root=$data") -WorkingDirectory (Split-Path $Exe) `
    -WindowStyle Normal -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds(2)
do {
    Start-Sleep -Milliseconds 50
    $menuHandle = Get-MenuWindow $expected[0]
} while ($menuHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
if ($menuHandle -ne [IntPtr]::Zero) {
    $menuRect = [L2DCatMenuNative+Rect]::new()
    [void][L2DCatMenuNative]::GetWindowRect($menuHandle, [ref]$menuRect)
    $bitmap = [Drawing.Bitmap]::new($menuRect.R - $menuRect.L, $menuRect.B - $menuRect.T)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($menuRect.L, $menuRect.T, 0, 0, $bitmap.Size)
    $bitmap.Save((Join-Path $OutputDir "context-menu.png"), [Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose(); $bitmap.Dispose()
    $nativeMenu = [L2DCatMenuNative]::SendMessageW($menuHandle, 0x01E1,
        [IntPtr]::Zero, [IntPtr]::Zero)
    $labels = [Collections.Generic.List[string]]::new()
    $count = [L2DCatMenuNative]::GetMenuItemCount($nativeMenu)
    for ($index = 0; $index -lt $count; $index++) {
        $text = [Text.StringBuilder]::new(128)
        [void][L2DCatMenuNative]::GetMenuStringW($nativeMenu, $index, $text, 128, 0x400)
        if ($text.Length) { $labels.Add($text.ToString()) }
    }
    $owner = [L2DCatMenuNative]::GetWindow($menuHandle, 4)
    $itemRect = [L2DCatMenuNative+Rect]::new()
    $itemKnown = [L2DCatMenuNative]::GetMenuItemRect(
        $owner, $nativeMenu, 0, [ref]$itemRect)
    if (-not $itemKnown -or $itemRect.L -lt $menuRect.L -or
        $itemRect.R -gt $menuRect.R -or $itemRect.T -lt $menuRect.T -or
        $itemRect.B -gt $menuRect.B) {
        $itemRect = [L2DCatMenuNative+Rect]::new()
        $itemRect.L = $menuRect.L; $itemRect.R = $menuRect.R
        $itemRect.T = $menuRect.T + 8; $itemRect.B = $menuRect.T + 30
    }
    [void][L2DCatMenuNative]::SetCursorPos(
        [int](($itemRect.L + $itemRect.R) / 2),
        [int](($itemRect.T + $itemRect.B) / 2))
    [L2DCatMenuNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    [L2DCatMenuNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
$windowDeadline = [DateTime]::UtcNow.AddMilliseconds(2500)
do {
    Start-Sleep -Milliseconds 100
    $visible = @(Get-Windows $process.Id | Where-Object Class -ne "#32768")
} while ($visible.Count -lt 2 -and [DateTime]::UtcNow -lt $windowDeadline)
$preferencesOpened = $visible.Count -ge 2
$exited = $process.WaitForExit(20000)
if (-not $exited) { Stop-Process -Id $process.Id -Force }
$exitCode = if ($exited) { $process.ExitCode } else { -1 }
$labelsMatch = ($labels -join "|") -eq ($expected -join "|")
$passed = $menuHandle -ne [IntPtr]::Zero -and $preferencesOpened -and $exitCode -eq 0 -and $labelsMatch
$result = [pscustomobject]@{MenuFound=($menuHandle-ne[IntPtr]::Zero); Labels=$labels
    LabelsMatch=$labelsMatch; PreferencesOpened=$preferencesOpened; VisibleWindows=$visible.Count
    ExitCode=$exitCode; Passed=$passed}
$result | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $OutputDir "result.json")
$result | Format-List
if (-not $passed) { exit 1 }
