param(
    [string]$Exe = "",
    [string]$ModelDirectory = "",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build\l2dcat.exe" }
if (-not $ModelDirectory) {
    $ModelDirectory = Join-Path $root "resources\assets\models\standard"
}
if (-not $OutputDir) { $OutputDir = Join-Path $root "build\drop-import-audit" }
$Exe = [IO.Path]::GetFullPath($Exe)
$ModelDirectory = [IO.Path]::GetFullPath($ModelDirectory)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class L2DCatDropNative {
    public delegate bool EnumProc(IntPtr handle, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr handle, out uint process);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr handle);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr handle, out Rect rect);
    [DllImport("shell32.dll")] public static extern void DragAcceptFiles(IntPtr handle, bool accept);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr handle, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("kernel32.dll")] public static extern IntPtr GlobalAlloc(uint flags, UIntPtr bytes);
    [DllImport("kernel32.dll")] public static extern IntPtr GlobalLock(IntPtr memory);
    [DllImport("kernel32.dll")] public static extern bool GlobalUnlock(IntPtr memory);
    public static bool Drop(IntPtr window, string path) {
        byte[] text = Encoding.Unicode.GetBytes(path + "\0\0");
        IntPtr memory = GlobalAlloc(0x42, (UIntPtr)(20 + text.Length));
        if (memory == IntPtr.Zero) return false;
        IntPtr data = GlobalLock(memory);
        Marshal.WriteInt32(data, 0, 20);
        Marshal.WriteInt32(data, 16, 1);
        Marshal.Copy(text, 0, IntPtr.Add(data, 20), text.Length);
        GlobalUnlock(memory);
        DragAcceptFiles(window, true);
        return PostMessageW(window, 0x233, memory, IntPtr.Zero);
    }
}
'@

Get-Process l2dcat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 350
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$dataRoot = Join-Path $OutputDir ("data-" + [DateTime]::UtcNow.Ticks)
$arguments = @("--ci-smoke", "--ci-preferences", "--ci-preference-page=2",
    "--ci-exit-ms=6000", "--data-root=$dataRoot")
$process = Start-Process -FilePath $Exe -ArgumentList $arguments `
    -WorkingDirectory (Split-Path $Exe) -WindowStyle Normal -PassThru
Start-Sleep -Milliseconds 1400
$windows = [Collections.Generic.List[object]]::new()
[L2DCatDropNative]::EnumWindows({
    param($handle, $data)
    [uint32]$owner = 0
    [void][L2DCatDropNative]::GetWindowThreadProcessId($handle, [ref]$owner)
    if ($owner -eq $process.Id -and [L2DCatDropNative]::IsWindowVisible($handle)) {
        $rect = [L2DCatDropNative+Rect]::new()
        if ([L2DCatDropNative]::GetWindowRect($handle, [ref]$rect)) {
            $windows.Add([pscustomobject]@{
                Handle=$handle; Area=($rect.R-$rect.L)*($rect.B-$rect.T)
            })
        }
    }
    return $true
}, [IntPtr]::Zero) | Out-Null
$target = ($windows | Sort-Object Area -Descending | Select-Object -First 1).Handle
$posted = $target -and [L2DCatDropNative]::Drop($target, $ModelDirectory)
$process.WaitForExit()
$models = @(Get-ChildItem (Join-Path $dataRoot "custom-models") `
    -Directory -ErrorAction SilentlyContinue)
$settings = @($models | ForEach-Object {
    Get-ChildItem $_.FullName -Filter "*.model3.json" -File -ErrorAction SilentlyContinue
})
$passed = $posted -and $process.ExitCode -eq 0 -and
    $models.Count -eq 1 -and $settings.Count -eq 1
[pscustomobject]@{
    Posted=$posted; Target=$target; ImportedCount=$models.Count
    ImportedModel3=$settings.Count; ExitCode=$process.ExitCode
    Passed=$passed; DataRoot=$dataRoot
} | Format-List
if (-not $passed) { exit 1 }
