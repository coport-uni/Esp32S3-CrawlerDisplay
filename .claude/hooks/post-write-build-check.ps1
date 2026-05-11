# After a Write/Edit to project C/H sources, run 'idf.py build' to catch
# compile errors and warnings immediately. Only triggers for files under
# main/, components/, or the top-level CMakeLists.txt / sdkconfig.defaults.
# Skips silently if idf.py is not on PATH (export script not sourced).

$ErrorActionPreference = 'Stop'

$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }

try {
    $payload = $raw | ConvertFrom-Json
} catch {
    exit 0
}

$filePath = $payload.tool_input.file_path
if ([string]::IsNullOrWhiteSpace($filePath)) { exit 0 }

$normalized = $filePath -replace '\\', '/'
$basename   = Split-Path -Leaf $filePath

# Only react to source files that can affect the firmware binary.
$shouldBuild = $false
if ($normalized -match '(^|/)(main|components)/.*\.(c|h|cc|cpp|hpp)$') { $shouldBuild = $true }
if ($basename -eq 'CMakeLists.txt') { $shouldBuild = $true }
if ($basename -eq 'sdkconfig.defaults' -or $basename -eq 'idf_component.yml') { $shouldBuild = $true }
if (-not $shouldBuild) { exit 0 }

# Skip if idf.py is not available (avoid spamming when the user just opened the project).
$idf = Get-Command idf.py -ErrorAction SilentlyContinue
if ($null -eq $idf) { exit 0 }

# Run incremental build, capture combined output.
$projectDir = if ($env:CLAUDE_PROJECT_DIR) { $env:CLAUDE_PROJECT_DIR } else { (Get-Location).Path }
$logFile = Join-Path $projectDir '.claude/last-build.log'
New-Item -ItemType Directory -Force -Path (Split-Path $logFile) | Out-Null

$proc = Start-Process -FilePath 'idf.py' -ArgumentList @('build') `
    -WorkingDirectory $projectDir `
    -RedirectStandardOutput $logFile `
    -RedirectStandardError "$logFile.err" `
    -NoNewWindow -PassThru -Wait

$stdout = if (Test-Path $logFile)        { Get-Content -Raw $logFile }        else { '' }
$stderr = if (Test-Path "$logFile.err")  { Get-Content -Raw "$logFile.err" }  else { '' }
$combined = "$stdout`n$stderr"

# Surface errors first; if none, surface warnings; otherwise silent.
$errorLines   = ($combined -split "`n") | Where-Object { $_ -match ': error:' }
$warningLines = ($combined -split "`n") | Where-Object { $_ -match ': warning:' }

if ($proc.ExitCode -ne 0 -or $errorLines.Count -gt 0) {
    [Console]::Error.WriteLine("BUILD FAILED after edit to '$basename'. Errors:")
    $errorLines | Select-Object -First 30 | ForEach-Object { [Console]::Error.WriteLine($_) }
    if ($errorLines.Count -gt 30) {
        [Console]::Error.WriteLine("...({0} more error lines in .claude/last-build.log)" -f ($errorLines.Count - 30))
    }
    exit 2
}

if ($warningLines.Count -gt 0) {
    [Console]::Error.WriteLine("BUILD OK but with $($warningLines.Count) warning line(s) — CLAUDE.md section 6 requires zero warnings:")
    $warningLines | Select-Object -First 20 | ForEach-Object { [Console]::Error.WriteLine($_) }
    exit 2
}

exit 0
