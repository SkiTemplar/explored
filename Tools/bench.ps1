# Recorrido de rendimiento de F7 por Map_IslaSal (-ExploredBench, UExploredShotSubsystem):
# Cala Varada, Palmar, base del faro, galería del faro y ciclón (D2 20:30). En cada vista mide
# 12 s con un barrido de cámara y escribe media/mínimo/1 % de fps frente al presupuesto
# (≥ 60 fps de media y ≥ 45 de mínimo a 1080p, FBenchBudgetModel), stat unit/gpu/rhi/streaming,
# r.Nanite.ShowStats y memreport -full; al final, «[Bench] Resumen ... OK|FALLA».
# Uso: Tools\bench.ps1 [-Width 1920] [-Height 1080] [-Window] [-Exe <ruta a Explored.exe>]
#   Sin -Exe lanza el editor en modo juego (UnrealEditor.exe -game, Development Editor compilado).
#   Con -Exe usa un paquete Development (Tools\package.ps1 -Configuration Development); en
#   Shipping el subsistema de capturas no existe y el bench no hace nada.
# Lee después: Saved\Logs\Bench.log (líneas «[Bench] ...») y Saved\Profiling\MemReports\*.memreport.
# Sale con error si el resumen no llega o dice FALLA.
param(
    [int]$Width = 1920,
    [int]$Height = 1080,
    [int]$TimeoutMinutes = 15,
    # Sin -Window se renderiza fuera de pantalla (-RenderOffscreen) y no aparece ninguna ventana.
    [switch]$Window,
    [string]$Exe = ''
)
. "$PSScriptRoot\common.ps1"

$logFile = Join-Path $ProjectRoot 'Saved\Logs\Bench.log'
if (Test-Path $logFile) { Remove-Item $logFile -Force }

$gameArgs = @('-ExploredBench', '-windowed', "-ResX=$Width", "-ResY=$Height",
    '-NoSound', '-nosplash', '-unattended', '-log', "-ABSLOG=$logFile")
if (-not $Window) { $gameArgs += '-RenderOffscreen' }
if ($Exe) {
    if (-not (Test-Path $Exe)) { throw "No existe el ejecutable $Exe" }
    $proc = Start-Process -FilePath $Exe -ArgumentList $gameArgs -PassThru
}
else {
    $proc = Start-Process -FilePath $Editor -ArgumentList (@($UProject, '-game') + $gameArgs) -PassThru
}
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    # Solo el proceso propio, por PID (nunca por nombre de imagen: hay otros editores abiertos).
    $proc.Kill()
    throw "El benchmark no terminó en $TimeoutMinutes minutos"
}
if (-not (Test-Path $logFile)) { throw "El juego no escribió $logFile" }

Write-Output "Log: $logFile"
$lines = @(Select-String -Path $logFile -Pattern '\[Bench\]' -Encoding UTF8 | ForEach-Object { $_.Line })
$lines | ForEach-Object { Write-Output $_ }
$summary = $lines | Where-Object { $_ -match '\[Bench\] Resumen' } | Select-Object -Last 1
if (-not $summary) { throw 'El recorrido no llegó al resumen (¿mapa sin cargar o cuelgue?)' }
if ($summary -match ': FALLA') { throw "Fuera de presupuesto: $summary" }
