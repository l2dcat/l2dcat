param(
    [string]$Exe = "",
    [string]$OutputDir = ""
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
    [DllImport("user32.dll")] public static extern int GetClassNameW(IntPtr handle, StringBuilder text, int size);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr handle);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string className, string title);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr handle, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
}
'@

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

Get-Process l2dcat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 350
$data = Join-Path $OutputDir ("data-" + [DateTime]::UtcNow.Ticks)
$process = Start-Process -FilePath $Exe -ArgumentList @("--ci-smoke", "--ci-context-menu",
    "--ci-exit-ms=6500", "--data-root=$data") -WorkingDirectory (Split-Path $Exe) `
    -WindowStyle Normal -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds(2)
do {
    Start-Sleep -Milliseconds 50
    $menuHandle = [L2DCatMenuNative]::FindWindowW("#32768", $null)
} while ($menuHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
if ($menuHandle -ne [IntPtr]::Zero) {
    $menuRect = [L2DCatMenuNative+Rect]::new()
    [void][L2DCatMenuNative]::GetWindowRect($menuHandle, [ref]$menuRect)
    [void][L2DCatMenuNative]::SetCursorPos($menuRect.L + 50, $menuRect.T + 12)
    [L2DCatMenuNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    [L2DCatMenuNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
$windowDeadline = [DateTime]::UtcNow.AddMilliseconds(2500)
do {
    Start-Sleep -Milliseconds 100
    $visible = @(Get-Windows $process.Id | Where-Object Class -ne "#32768")
} while ($visible.Count -lt 2 -and [DateTime]::UtcNow -lt $windowDeadline)
$preferencesOpened = $visible.Count -ge 2
$exited = $process.WaitForExit(8000)
if (-not $exited) { Stop-Process -Id $process.Id -Force }
$exitCode = if ($exited) { $process.ExitCode } else { -1 }
$passed = $menuHandle -ne [IntPtr]::Zero -and $preferencesOpened -and $exitCode -eq 0
[pscustomobject]@{MenuFound=($menuHandle-ne[IntPtr]::Zero); PreferencesOpened=$preferencesOpened
    VisibleWindows=$visible.Count; ExitCode=$exitCode; Passed=$passed} | Format-List
if (-not $passed) { exit 1 }
