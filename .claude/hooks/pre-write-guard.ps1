# Block writes that violate project-layout rules:
#   - Debug/scratch-named files outside claude_test/
#   - Any edits to managed_components/ (regenerated from dependencies.lock)
#   - Any edits to build/ (build artifacts)
# Reads JSON tool_input on stdin and exits 2 (with stderr) to block.

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

# Rule 1: managed_components/ is regenerated; never edit by hand.
if ($normalized -match '(^|/)managed_components/') {
    [Console]::Error.WriteLine(
        "BLOCKED: managed_components/ is regenerated from dependencies.lock.`n" +
        "Change the version in main/idf_component.yml and run 'idf.py reconfigure' instead.`n" +
        "Path: $filePath")
    exit 2
}

# Rule 2: build/ is build output; never edit by hand.
if ($normalized -match '(^|/)build/') {
    [Console]::Error.WriteLine(
        "BLOCKED: build/ is generated output. Run 'idf.py build' rather than editing files there.`n" +
        "Path: $filePath")
    exit 2
}

# Rule 3: debug-/scratch-named source files must live under claude_test/.
$debugPrefixes = @('debug_', 'scratch_', 'tmp_', 'probe_', 'experiment_', 'test_debug_')
$looksDebug = $false
foreach ($p in $debugPrefixes) {
    if ($basename.ToLower().StartsWith($p)) { $looksDebug = $true; break }
}

if ($looksDebug -and ($normalized -notmatch '(^|/)claude_test/')) {
    [Console]::Error.WriteLine(
        "BLOCKED: '$basename' looks like a debug/scratch file but is not under claude_test/.`n" +
        "Per CLAUDE.md section 3, debug or exploratory files must live in claude_test/.`n" +
        "Move it to claude_test/$basename and update claude_test/README.md.")
    exit 2
}

exit 0
