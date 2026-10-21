# Flujo de menús de Baliza Siete en una partida real (F5): menú principal sin que corra la partida,
# nueva partida, cambios, guardar, volver al menú y «Continuar» con el mismo estado.
# Lanza Map_IslaSal en -game con UExploredFrontendFlowTest («-ExploredFrontendFlow») y lee el log.
# Uso: Tools\frontend_flow.ps1 [-TimeoutMinutes 6] [-Window]
param(
    [int]$TimeoutMinutes = 6,
    [switch]$Window
)
. "$PSScriptRoot\common.ps1"

$logFile = Join-Path $ProjectRoot 'Saved\Logs\FrontendFlow.log'
Remove-Item $logFile -ErrorAction SilentlyContinue
$cmdArgs = @($UProject, '/Game/Maps/Map_IslaSal', '-game', '-ExploredFrontendFlow', '-windowed', '-ResX=1280', '-ResY=720',
    '-NoSound', '-nosplash', '-unattended', '-log', "-ABSLOG=$logFile")
if (-not $Window) { $cmdArgs += '-RenderOffscreen' }
$proc = Start-Process -FilePath $Editor -ArgumentList $cmdArgs -PassThru
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $proc.Kill()
    throw "La prueba del flujo de menús no terminó en $TimeoutMinutes minutos (revisa $logFile)"
}
if (-not (Test-Path $logFile)) { throw "No se generó $logFile" }
Select-String -Path $logFile -Pattern '\[FrontendFlow\]' | ForEach-Object { $_.Line -replace '^.*\[FrontendFlow\] ', '' }
if (-not (Select-String -Path $logFile -Pattern '\[FrontendFlow\] RESULTADO: OK' -Quiet)) {
    Write-Warning 'El flujo de menús falla (ver el log)'
    exit 1
}
