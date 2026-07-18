param(
    [string]$Exe = "",
    [string]$OutputDir = "",
    [ValidateSet("visible", "hidden", "preferences")]
    [string]$Mode = "visible",
    [ValidateSet("en-US", "zh-CN", "zh-TW", "pt-BR", "vi-VN")]
    [string]$Language = "zh-CN",
    [int]$DurationSeconds = 60,
    [int]$IntervalSeconds = 2,
    [int]$WarmupSeconds = 5,
    [double]$MaximumWorkingMiB = 100,
    [double]$MaximumGrowthMiB = 8,
    [int]$MaximumHandleGrowth = 8
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Exe) { $Exe = Join-Path $root "build\l2dcat.exe" }
if (-not $OutputDir) { $OutputDir = Join-Path $root "build\soak-audit" }
$Exe = [IO.Path]::GetFullPath($Exe)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if ($DurationSeconds -lt 5 -or $IntervalSeconds -lt 1 -or $WarmupSeconds -lt 1 -or
    $IntervalSeconds -ge $DurationSeconds) { throw "Invalid soak duration or interval" }
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$existing = @(Get-Process l2dcat -ErrorAction SilentlyContinue)
if ($existing.Count) {
    $existing | Stop-Process -Force
    $existing | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 350
}

$data = Join-Path $OutputDir ("data-" + [DateTime]::UtcNow.Ticks)
$exitMilliseconds = ($WarmupSeconds + $DurationSeconds + 3) * 1000
$arguments = @("--ci-smoke", "--ci-ignore-global-input",
    "--ci-exit-ms=$exitMilliseconds", "--data-root=$data")
if ($Mode -eq "hidden") { $arguments += "--ci-shortcuts" }
if ($Mode -eq "preferences") {
    $arguments += @("--ci-preferences", "--ci-language=$Language", "--ci-theme=light")
}

$process = Start-Process -FilePath $Exe -ArgumentList $arguments `
    -WorkingDirectory (Split-Path $Exe) -WindowStyle Hidden -PassThru
$samples = [Collections.Generic.List[object]]::new()
try {
    Start-Sleep -Seconds $WarmupSeconds
    if ($process.HasExited) {
        throw "l2dcat exited during warmup with code $($process.ExitCode)"
    }
    $process.Refresh()
    $started = [DateTime]::UtcNow
    $previousCpu = $process.TotalProcessorTime
    $previousAt = $started
    while (([DateTime]::UtcNow - $started).TotalSeconds -lt $DurationSeconds) {
        if ($process.HasExited) {
            throw "l2dcat exited before the soak duration with code $($process.ExitCode)"
        }
        $process.Refresh()
        $now = [DateTime]::UtcNow
        $cpu = $process.TotalProcessorTime
        $elapsed = ($now - $previousAt).TotalSeconds
        $cpuPercent = if ($elapsed -gt 0) {
            100.0 * ($cpu - $previousCpu).TotalSeconds /
                ($elapsed * [Environment]::ProcessorCount)
        } else { 0.0 }
        $samples.Add([pscustomobject]@{
            TimestampUtc=$now.ToString("o")
            ElapsedSeconds=[Math]::Round(($now - $started).TotalSeconds, 2)
            WorkingMiB=[Math]::Round($process.WorkingSet64 / 1MB, 3)
            PrivateMiB=[Math]::Round($process.PrivateMemorySize64 / 1MB, 3)
            Handles=$process.HandleCount
            CpuPercent=[Math]::Round($cpuPercent, 4)
        })
        $previousCpu = $cpu
        $previousAt = $now
        Start-Sleep -Seconds $IntervalSeconds
    }
} finally {
    if (-not $process.HasExited) { $process.WaitForExit(5000) | Out-Null }
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}

$report = Join-Path $OutputDir "soak-$Mode-$Language.csv"
$samples | Export-Csv -NoTypeInformation -Encoding UTF8 -Path $report
if (-not $samples.Count) { throw "No soak samples were captured" }
$first = $samples[0]
$last = $samples[$samples.Count - 1]
$maximumWorking = ($samples | Measure-Object WorkingMiB -Maximum).Maximum
$workingGrowth = $last.WorkingMiB - $first.WorkingMiB
$handleGrowth = $last.Handles - $first.Handles
$averageCpu = ($samples | Measure-Object CpuPercent -Average).Average
$passed = $process.ExitCode -eq 0 -and $maximumWorking -le $MaximumWorkingMiB -and
    $workingGrowth -le $MaximumGrowthMiB -and $handleGrowth -le $MaximumHandleGrowth
[pscustomobject]@{
    Mode=$Mode; Language=$Language; Samples=$samples.Count; ExitCode=$process.ExitCode
    MaximumWorkingMiB=[Math]::Round($maximumWorking, 3)
    WorkingGrowthMiB=[Math]::Round($workingGrowth, 3)
    HandleGrowth=$handleGrowth; AverageCpuPercent=[Math]::Round($averageCpu, 4)
    Passed=$passed; Report=$report
} | Format-List
if (-not $passed) { exit 1 }
