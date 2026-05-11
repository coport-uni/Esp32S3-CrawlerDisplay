# After a Write/Edit under claude_test/, remind to update the index README.
# Skips when the write itself targets claude_test/README.md.
# Emits a reminder to stderr and exits 2 so the model sees the feedback.

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

if ($normalized -match '(^|/)claude_test/' -and $basename -ne 'README.md') {
    [Console]::Error.WriteLine(
        "REMINDER: '$basename' was added/edited under claude_test/.`n" +
        "Per CLAUDE.md section 3, update claude_test/README.md with a row describing the file's purpose and any lessons learned.")
    exit 2
}

exit 0
