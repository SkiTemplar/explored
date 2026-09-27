# Lanza el juego con -ExploredBench, recorre spawn/aerea/orilla, vuelca stat unit/gpu/rhi/
# streaming, r.Nanite.ShowStats y memreport -full a Saved/Logs y lo cierra.
# Uso: Tools\bench.ps1 [-Width 1920] [-Height 1080]
# Lee después: Saved\Logs\Bench.log (líneas «[Bench] ...») y Saved\Profiling\MemReports\*.memreport.
param(
    [int]$Width = 1920,
    [int]$Height = 1080,
    [int]$TimeoutMinutes = 15
)
. "$PSScriptRoot\common.ps1"

$logFile = Join-Path $ProjectRoot 'Saved\Logs\Bench.log'

$cmdArgs = @($UProject, '-game', '-ExploredBench', '-windowed', "-ResX=$Width", "-ResY=$Height",
    '-NoSound', '-nosplash', '-unattended', '-log', "-ABSLOG=$logFile")
$proc = Start-Process -FilePath $Editor -ArgumentList $cmdArgs -PassThru
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $proc.Kill()
    throw "El benchmark no terminó en $TimeoutMinutes minutos"
}
Write-Output "Log: $logFile"
Select-String -Path $logFile -Pattern '\[Bench\]' | ForEach-Object { $_.Line }
