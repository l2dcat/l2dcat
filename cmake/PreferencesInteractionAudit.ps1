param(
    [string]$Exe = "",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build-cubism\Release\l2dcat.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build-cubism\preferences-interaction" }
$Exe = [IO.Path]::GetFullPath($Exe)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$dataRoot = Join-Path $OutputDir "data"
$configPath = Join-Path $dataRoot "config.json"
New-Item -ItemType Directory -Force -Path $dataRoot | Out-Null
Remove-Item -LiteralPath $configPath -Force -ErrorAction SilentlyContinue

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class L2DCatPreferencesNative {
    public delegate bool EnumProc(IntPtr handle, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc, IntPtr data);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr handle);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr handle, out uint process);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr handle, out Rect rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr handle, out Rect rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr handle, ref Point point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr handle);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr handle, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, int data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr handle, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
'@
[void][L2DCatPreferencesNative]::SetProcessDPIAware()

function Get-AppWindows([int]$ProcessId) {
    $windows = [Collections.Generic.List[object]]::new()
    [L2DCatPreferencesNative]::EnumWindows({
        param($handle, $unused)
        [uint32]$owner = 0
        [void][L2DCatPreferencesNative]::GetWindowThreadProcessId($handle, [ref]$owner)
        if ($owner -eq $ProcessId -and [L2DCatPreferencesNative]::IsWindowVisible($handle)) {
            $rect = [L2DCatPreferencesNative+Rect]::new()
            if ([L2DCatPreferencesNative]::GetWindowRect($handle, [ref]$rect)) {
                $area = ($rect.R - $rect.L) * ($rect.B - $rect.T)
                if ($area -gt 400) { $windows.Add([pscustomobject]@{ Handle=$handle; Area=$area }) }
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
        if ($windows.Count -ge 2) {
            return ($windows | Sort-Object Area -Descending | Select-Object -First 1).Handle
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Preferences window was not created"
}

function Get-ClientPoint([IntPtr]$Window, [double]$X, [double]$Y) {
    $client = [L2DCatPreferencesNative+Rect]::new()
    [void][L2DCatPreferencesNative]::GetClientRect($Window, [ref]$client)
    $point = [L2DCatPreferencesNative+Point]::new()
    $point.X = [int][Math]::Round($X * ($client.R - $client.L) / 900.0)
    $point.Y = [int][Math]::Round($Y * ($client.B - $client.T) / 640.0)
    [void][L2DCatPreferencesNative]::ClientToScreen($Window, [ref]$point)
    return $point
}

function Invoke-Click([IntPtr]$Window, [double]$X, [double]$Y) {
    $point = Get-ClientPoint $Window $X $Y
    $client = [L2DCatPreferencesNative+Rect]::new()
    [void][L2DCatPreferencesNative]::GetClientRect($Window, [ref]$client)
    $clientX = [int][Math]::Round($X * ($client.R - $client.L) / 900.0)
    $clientY = [int][Math]::Round($Y * ($client.B - $client.T) / 640.0)
    $position = [IntPtr]([long](($clientY -band 0xFFFF) -shl 16) -bor
        [long]($clientX -band 0xFFFF))
    [void][L2DCatPreferencesNative]::SetForegroundWindow($Window)
    [void][L2DCatPreferencesNative]::SetCursorPos($point.X, $point.Y)
    Start-Sleep -Milliseconds 50
    [void][L2DCatPreferencesNative]::PostMessageW(
        $Window, 0x0200, [IntPtr]::Zero, $position)
    [void][L2DCatPreferencesNative]::PostMessageW(
        $Window, 0x0201, [IntPtr]1, $position)
    Start-Sleep -Milliseconds 180
    [void][L2DCatPreferencesNative]::PostMessageW(
        $Window, 0x0202, [IntPtr]::Zero, $position)
    Start-Sleep -Milliseconds 250
}

function Invoke-PhysicalClick([IntPtr]$Window, [double]$X, [double]$Y) {
    $point = Get-ClientPoint $Window $X $Y
    [void][L2DCatPreferencesNative]::SetForegroundWindow($Window)
    [void][L2DCatPreferencesNative]::SetCursorPos($point.X, $point.Y)
    Start-Sleep -Milliseconds 80
    [L2DCatPreferencesNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [L2DCatPreferencesNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}

function Invoke-Wheel([IntPtr]$Window, [double]$X, [double]$Y) {
    $point = Get-ClientPoint $Window $X $Y
    [void][L2DCatPreferencesNative]::SetForegroundWindow($Window)
    [void][L2DCatPreferencesNative]::SetCursorPos($point.X, $point.Y)
    Start-Sleep -Milliseconds 50
    $position = [IntPtr]([long](($point.Y -band 0xFFFF) -shl 16) -bor
        [long]($point.X -band 0xFFFF))
    $wheel = [IntPtr]([long][uint32]4287102976)
    [void][L2DCatPreferencesNative]::PostMessageW($Window, 0x020A, $wheel, $position)
    Start-Sleep -Milliseconds 80
    [void][L2DCatPreferencesNative]::PostMessageW($Window, 0x020A, $wheel, $position)
    Start-Sleep -Milliseconds 300
}

function Invoke-Text([IntPtr]$Window, [string]$Text) {
    [void][L2DCatPreferencesNative]::SetForegroundWindow($Window)
    [void][L2DCatPreferencesNative]::PostMessageW(
        $Window, 0x0007, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    foreach ($character in $Text.ToCharArray()) {
        [void][L2DCatPreferencesNative]::PostMessageW(
            $Window, 0x0109, [IntPtr][int]$character, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 40
    }
}

function Save-Window([IntPtr]$Window, [string]$Name) {
    [void][L2DCatPreferencesNative]::SetForegroundWindow($Window)
    Start-Sleep -Milliseconds 120
    $rect = [L2DCatPreferencesNative+Rect]::new()
    [void][L2DCatPreferencesNative]::GetWindowRect($Window, [ref]$rect)
    $bitmap = [Drawing.Bitmap]::new($rect.R - $rect.L, $rect.B - $rect.T)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $printed = $false
    $dc = $graphics.GetHdc()
    try { $printed = [L2DCatPreferencesNative]::PrintWindow($Window, $dc, 2) }
    finally { $graphics.ReleaseHdc($dc) }
    if (-not $printed) { $graphics.CopyFromScreen($rect.L, $rect.T, 0, 0, $bitmap.Size) }
    $path = Join-Path $OutputDir $Name
    $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose(); $bitmap.Dispose()
    return $path
}

function Measure-Difference([string]$First, [string]$Second) {
    $a = [Drawing.Bitmap]::new($First); $b = [Drawing.Bitmap]::new($Second)
    try {
        $changed = 0; $samples = 0
        for ($y = 0; $y -lt $a.Height; $y += 6) {
            for ($x = 0; $x -lt $a.Width; $x += 6) {
                if ($a.GetPixel($x, $y).ToArgb() -ne $b.GetPixel($x, $y).ToArgb()) { $changed++ }
                $samples++
            }
        }
        return $changed / [double]$samples
    } finally { $a.Dispose(); $b.Dispose() }
}

Get-Process l2dcat -ErrorAction SilentlyContinue | Stop-Process -Force
$arguments = @("--ci-preferences", "--ci-preference-page=0", "--ci-language=zh-CN",
    "--ci-theme=light", "--config=$configPath", "--data-root=$dataRoot")
$process = Start-Process -FilePath $Exe -ArgumentList $arguments `
    -WorkingDirectory (Split-Path $Exe) -PassThru
try {
    $window = Wait-Preferences $process.Id
    Invoke-Click $window 100 500
    $baseline = Save-Window $window "01-baseline.png"
    Invoke-Click $window 310 135
    $toggled = Save-Window $window "02-toggle-card.png"
    Invoke-Wheel $window 520 540
    [L2DCatPreferencesNative]::keybd_event(0x22, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [L2DCatPreferencesNative]::keybd_event(0x22, 0, 2, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
    $scrolled = Save-Window $window "03-scrolled.png"

    Invoke-Click $window 90 90
    $general = Save-Window $window "04-general.png"
    Invoke-Click $window 838 383
    $combo = Save-Window $window "05-combo-open.png"
    [L2DCatPreferencesNative]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)
    [L2DCatPreferencesNative]::keybd_event(0x1B, 0, 2, [UIntPtr]::Zero)

    Invoke-Click $window 90 194
    $shortcuts = Save-Window $window "06-shortcuts.png"
    Invoke-Click $window 745 135
    Invoke-Text $window "TEST"
    Start-Sleep -Milliseconds 300
    $edited = Save-Window $window "07-shortcut-edited.png"

    Invoke-Click $window 90 246
    $about = Save-Window $window "08-about.png"
    [Windows.Forms.Clipboard]::SetText("clipboard-audit-sentinel")
    Invoke-PhysicalClick $window 800 263
    $clipboard = [Windows.Forms.Clipboard]::GetText()
    [void][L2DCatPreferencesNative]::PostMessageW($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    for ($i = 0; $i -lt 50 -and -not (Test-Path $configPath); $i++) {
        Start-Sleep -Milliseconds 50
    }
    $config = Get-Content -Raw -Encoding utf8 $configPath | ConvertFrom-Json
    $result = [ordered]@{
        ToggleDifference = Measure-Difference $baseline $toggled
        ScrollDifference = Measure-Difference $toggled $scrolled
        PageDifference = Measure-Difference $scrolled $general
        ComboDifference = Measure-Difference $general $combo
        EditDifference = Measure-Difference $shortcuts $edited
        TogglePersisted = $config.model.mirror -eq $true
        ShortcutPersisted = $config.shortcuts.visibleCat -eq "TEST"
        CopyWorked = $clipboard -like "l2dcat*"
    }
    $result | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $OutputDir "result.json")
    [pscustomobject]$result | Format-List
    $passed = $result.ToggleDifference -gt 0.0001 -and $result.ScrollDifference -gt 0.02 -and
        $result.PageDifference -gt 0.02 -and $result.ComboDifference -gt 0.002 -and
        $result.EditDifference -gt 0.0001 -and $result.TogglePersisted -and
        $result.ShortcutPersisted -and $result.CopyWorked
    if (-not $passed) { exit 1 }
} finally {
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
