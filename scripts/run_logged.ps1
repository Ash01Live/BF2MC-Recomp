<#
  run_logged.ps1 <name> <command> [args...]

  Runs a command, saves its output to logs/<name>.log, and on failure
  turns the important lines into GitHub Actions error annotations, so the
  cause is visible on the run page (and through the API) without opening
  the full log.
#>
$Name = $args[0]
$Cmd  = @($args | Select-Object -Skip 1)   # skip <name>
New-Item -ItemType Directory -Force logs | Out-Null
$Log = "logs/$Name.log"

$global:LASTEXITCODE = 0
$ok = $true
try {
    & $Cmd[0] @($Cmd | Select-Object -Skip 1) 2>&1 | Tee-Object -FilePath $Log
    if ($LASTEXITCODE) { $ok = $false }
} catch {
    $_ | Out-String | Tee-Object -FilePath $Log -Append
    $ok = $false
}
if ($ok) { exit 0 }

function Escape($s) { $s -replace '%', '%25' -replace "`r", '' -replace "`n", '%0A' }
$lines = @(Get-Content $Log -ErrorAction SilentlyContinue)

# Up to 8 lines that look like real errors, one annotation each.
$errs = $lines | Where-Object { $_ -match '(?i)(error|fatal|exception|traceback|cannot|could not|not found|undefined|unresolved)' } |
    Select-Object -First 8
foreach ($e in $errs) { Write-Host "::error title=$Name::$(Escape $e.Trim())" }

# The tail of the output as one multi-line annotation.
$tail = ($lines | Select-Object -Last 25) -join "`n"
Write-Host "::error title=$Name (last 25 lines)::$(Escape $tail)"
exit 1
