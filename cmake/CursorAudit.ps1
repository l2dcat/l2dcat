param(
    [string]$Exe = "",
    [string]$OutputDir = "",
    [string]$ImportPath = "",
    [string]$Language = "zh-CN",
    [ValidateSet("light", "dark")]
    [string]$Theme = "light"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build-cubism\Release\l2dcat.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build-cubism\cursor-audit" }
if (-not $ImportPath) { $ImportPath = Join-Path $root "build-cubism\import-fixture-plain" }
$Exe = [IO.Path]::GetFullPath($Exe)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$ImportPath = [IO.Path]::GetFullPath($ImportPath)
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class L2DCatCursorNative {
    public delegate bool EnumProc(IntPtr handle, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    [StructLayout(LayoutKind.Sequential)] public struct CursorInfo {
        public int Size, Flags; public IntPtr Cursor; public Point Screen;
    }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc, IntPtr data);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr handle);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr handle, out uint process);
    [DllImport("user32.dll", EntryPoint="GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr handle, int index);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr handle, out Rect rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr handle, out Rect rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr handle, ref Point point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr handle);
    [DllImport("user32.dll")] public static extern bool GetCursorInfo(ref CursorInfo info);
    [DllImport("user32.dll")] public static extern IntPtr LoadCursor(IntPtr instance, IntPtr name);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, int data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr handle, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr handle, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
'@
[void][L2DCatCursorNative]::SetProcessDPIAware()

function Get-AppWindows([int]$ProcessId) {
    $windows = [Collections.Generic.List[object]]::new()
    [L2DCatCursorNative]::EnumWindows({
        param($handle, $unused)
        [uint32]$owner = 0
        [void][L2DCatCursorNative]::GetWindowThreadProcessId($handle, [ref]$owner)
        if ($owner -eq $ProcessId -and [L2DCatCursorNative]::IsWindowVisible($handle)) {
            $rect = [L2DCatCursorNative+Rect]::new()
            if ([L2DCatCursorNative]::GetWindowRect($handle, [ref]$rect)) {
                $area = ($rect.R - $rect.L) * ($rect.B - $rect.T)
                $width = $rect.R - $rect.L; $height = $rect.B - $rect.T
                if ($area -gt 400 -and $width -ge 700 -and $height -ge 550) {
                    $windows.Add([pscustomobject]@{
                        Handle=$handle; Area=$area; Width=$width; Height=$height })
                }
            }
        }
        return $true
    }, [IntPtr]::Zero) | Out-Null
    return $windows
}

function Wait-Preferences([int]$ProcessId) {
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        $windows = @(Get-AppWindows $ProcessId)
        if ($windows.Count -ge 1) {
            return ($windows | Sort-Object Area -Descending | Select-Object -First 1).Handle
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Preferences window was not created"
}

function Get-ClientPoint([IntPtr]$Window, [double]$X, [double]$Y) {
    $client = [L2DCatCursorNative+Rect]::new()
    [void][L2DCatCursorNative]::GetClientRect($Window, [ref]$client)
    $point = [L2DCatCursorNative+Point]::new()
    $point.X = [int][Math]::Round($X * ($client.R - $client.L) / 900.0)
    $point.Y = [int][Math]::Round($Y * ($client.B - $client.T) / 680.0)
    [void][L2DCatCursorNative]::ClientToScreen($Window, [ref]$point)
    return $point
}

function Invoke-Click([IntPtr]$Window, [double]$X, [double]$Y) {
    $point = Get-ClientPoint $Window $X $Y
    [void][L2DCatCursorNative]::SetForegroundWindow($Window)
    [void][L2DCatCursorNative]::SetCursorPos($point.X, $point.Y)
    Start-Sleep -Milliseconds 250
    [L2DCatCursorNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [L2DCatCursorNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}

function Invoke-Wheel([IntPtr]$Window, [double]$X, [double]$Y, [int]$Delta) {
    $point = Get-ClientPoint $Window $X $Y
    [void][L2DCatCursorNative]::SetForegroundWindow($Window)
    [void][L2DCatCursorNative]::SetCursorPos($point.X, $point.Y)
    Start-Sleep -Milliseconds 120
    [L2DCatCursorNative]::mouse_event(0x0800, 0, 0, $Delta, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 450
}

function Test-Cursor([IntPtr]$Window, [string]$Name, [double]$X,
    [double]$Y, [int]$SystemId) {
    $point = Get-ClientPoint $Window $X $Y
    [void][L2DCatCursorNative]::SetForegroundWindow($Window)
    [void][L2DCatCursorNative]::SetCursorPos($point.X, $point.Y)
    Start-Sleep -Milliseconds 250
    $info = [L2DCatCursorNative+CursorInfo]::new()
    $info.Size = [Runtime.InteropServices.Marshal]::SizeOf($info)
    [void][L2DCatCursorNative]::GetCursorInfo([ref]$info)
    $expected = [L2DCatCursorNative]::LoadCursor([IntPtr]::Zero, [IntPtr]$SystemId)
    $arrow = [L2DCatCursorNative]::LoadCursor([IntPtr]::Zero, [IntPtr]32512)
    $text = [L2DCatCursorNative]::LoadCursor([IntPtr]::Zero, [IntPtr]32513)
    $hand = [L2DCatCursorNative]::LoadCursor([IntPtr]::Zero, [IntPtr]32649)
    $actual = if ($info.Cursor -eq $hand) { "hand" } elseif ($info.Cursor -eq $text) {
        "text" } elseif ($info.Cursor -eq $arrow) { "arrow" } else { "other" }
    return [pscustomobject]@{ Name=$Name; Actual=$actual; Passed=$info.Cursor -eq $expected }
}

function Save-Window([IntPtr]$Window, [string]$Name) {
    [void][L2DCatCursorNative]::SetForegroundWindow($Window)
    Start-Sleep -Milliseconds 300
    $rect = [L2DCatCursorNative+Rect]::new()
    [void][L2DCatCursorNative]::GetWindowRect($Window, [ref]$rect)
    $bitmap = [Drawing.Bitmap]::new($rect.R - $rect.L, $rect.B - $rect.T)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($rect.L, $rect.T, 0, 0, $bitmap.Size)
    $path = Join-Path $OutputDir $Name
    $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose(); $bitmap.Dispose()
}

function Start-AuditPage([int]$Page, [bool]$Import) {
    # Move away from transient selection/translation overlays before opening the
    # next window; otherwise a topmost helper can intercept the first hover.
    [void][L2DCatCursorNative]::SetCursorPos(2, 2)
    Start-Sleep -Milliseconds 400
    $dataRoot = Join-Path $OutputDir "data-page$Page"
    Remove-Item -LiteralPath $dataRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $dataRoot | Out-Null
    $arguments = @("--ci-preferences", "--ci-preference-page=$Page",
        "--ci-language=$Language", "--ci-theme=$Theme", "--data-root=$dataRoot")
    if ($Import) {
        $target = Join-Path $dataRoot "custom-models\cursor-fixture"
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        Copy-Item -Path (Join-Path $ImportPath "*") -Destination $target -Recurse -Force
    }
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments `
        -WorkingDirectory (Split-Path $Exe) -PassThru
    $window = Wait-Preferences $process.Id
    [void][L2DCatCursorNative]::SetForegroundWindow($window)
    Start-Sleep -Milliseconds 800
    return [pscustomobject]@{ Process=$process; Window=$window }
}

Get-Process l2dcat -ErrorAction SilentlyContinue | Stop-Process -Force
$results = [Collections.Generic.List[object]]::new()
$hand = 32649; $text = 32513; $arrow = 32512

$page = Start-AuditPage 0 $false
try {
    $results.Add((Test-Cursor $page.Window "tab" 276 114 $hand))
    $results.Add((Test-Cursor $page.Window "close" 851 44 $hand))
    $results.Add((Test-Cursor $page.Window "toggle" 818 248 $hand))
    $results.Add((Test-Cursor $page.Window "number-property" 818 545 $hand))
    $results.Add((Test-Cursor $page.Window "empty-space" 500 640 $arrow))
} finally { $page.Process.Kill(); $page.Process.WaitForExit() }

$page = Start-AuditPage 1 $false
try {
    $results.Add((Test-Cursor $page.Window "combo" 710 248 $hand))
} finally { $page.Process.Kill(); $page.Process.WaitForExit() }

$page = Start-AuditPage 3 $false
try {
    $results.Add((Test-Cursor $page.Window "text-field" 700 248 $text))
} finally { $page.Process.Kill(); $page.Process.WaitForExit() }

$page = Start-AuditPage 2 $true
try {
    $results.Add((Test-Cursor $page.Window "import-card" 180 270 $hand))
    $results.Add((Test-Cursor $page.Window "model-card" 450 270 $hand))
    $results.Add((Test-Cursor $page.Window "model-close" 560 417 $hand))
    Save-Window $page.Window "model-page.png"
    Invoke-Click $page.Window 560 417
    Save-Window $page.Window "remove-dialog.png"
    $results.Add((Test-Cursor $page.Window "cancel-button" 523 376 $hand))
    Invoke-Click $page.Window 523 376
    Save-Window $page.Window "after-cancel.png"
} finally { $page.Process.Kill(); $page.Process.WaitForExit() }

$results | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $OutputDir "result.json")
$results | Format-Table
if (@($results | Where-Object { -not $_.Passed }).Count) { exit 1 }
