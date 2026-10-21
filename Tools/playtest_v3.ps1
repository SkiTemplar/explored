# Partida completa de Baliza Siete por bot (F3e): lanza Map_IslaSal en -game con
# UExploredPlaytestBot («-BalizaRun») y UExploredPlaytestAuditor, espera a que termine y deja
# el informe (duración estimada a 1×, desmayos, etapas, guardar/cargar, errores de log y
# softlocks) en Saved\Shots\playtest_report.json y playtest_report.txt.
# Uso: Tools\playtest_v3.ps1 [-Route competente] [-Clock 6] [-TimeoutMinutes 50] [-Window]
# Sin -Window se renderiza fuera de pantalla (-RenderOffscreen): nadie cierra la ventana a mitad.
# Con -Exe <Saved\Package\Windows\Explored.exe> juega sobre un paquete Development (criterio 2
# de F7); en Shipping el bot no existe.
param(
    [string]$Route = 'competente',
    [ValidateRange(1, 12)] [int]$Clock = 6,
    [int]$Width = 1280,
    [int]$Height = 720,
    [int]$TimeoutMinutes = 50,
    [switch]$Window,
    [string]$Exe = ''
)
. "$PSScriptRoot\common.ps1"

$outDir = Join-Path $ProjectRoot 'Saved\Shots'
New-Item -ItemType Directory -Force $outDir | Out-Null
$reportJson = Join-Path $outDir 'playtest_report.json'
$reportTxt = Join-Path $outDir 'playtest_report.txt'
Remove-Item $reportJson, $reportTxt -ErrorAction SilentlyContinue
$logFile = Join-Path $ProjectRoot 'Saved\Logs\PlaytestV3.log'

$cmdArgs = @('/Game/Maps/Map_IslaSal', '-SkipMenu', '-ExploredPlaytestAudit', "-BalizaRun=$Route",
    "-BalizaClock=$Clock", "-ShotsDir=$outDir", '-windowed', "-ResX=$Width", "-ResY=$Height", '-nosplash', '-unattended',
    '-log', "-ABSLOG=$logFile")
if (-not $Window) { $cmdArgs += '-RenderOffscreen' }
$start = Get-Date
if ($Exe) {
    if (-not (Test-Path $Exe)) { throw "No existe el ejecutable $Exe" }
    $proc = Start-Process -FilePath $Exe -ArgumentList $cmdArgs -PassThru
}
else {
    $proc = Start-Process -FilePath $Editor -ArgumentList (@($UProject, '-game') + $cmdArgs) -PassThru
}
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $proc.Kill()
    throw "El bot no terminó en $TimeoutMinutes minutos (revisa $logFile)"
}
$minutes = ((Get-Date) - $start).TotalMinutes
Write-Output ('Prueba terminada en {0:N1} min reales' -f $minutes)

if (-not (Test-Path $reportTxt)) {
    throw "No se generó $reportTxt; revisa $logFile"
}
$text = Get-Content $reportTxt -Encoding UTF8
$section = $false
foreach ($line in $text) {
    if ($line -like '== Partida completa del bot*') { $section = $true }
    if ($section) { Write-Output $line }
}
Write-Output "Informe completo: $reportJson"
$passed = Select-String -Path $reportTxt -Pattern 'RESULTADO: VERDE' -Quiet
if (-not $passed) {
    Write-Warning 'El bot no cumple el criterio de salida de F3 (ver el informe)'
    exit 1
}
