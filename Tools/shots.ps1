# Lanza el juego, toma capturas de verificación visual y lo cierra.
# Uso: Tools\shots.ps1 [-Set all|islands|day|spawn|aerial] [-Width 1600] [-Height 900]
param(
    [string]$Set = 'all',
    [int]$Width = 1600,
    [int]$Height = 900,
    [int]$TimeoutMinutes = 20
)
. "$PSScriptRoot\common.ps1"

$outDir = Join-Path $ProjectRoot 'Saved\Shots'
New-Item -ItemType Directory -Force $outDir | Out-Null
Get-ChildItem $outDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force

$cmdArgs = @($UProject, '-game', "-ExploredShots=$Set", "-ShotsDir=$outDir", '-windowed', "-ResX=$Width", "-ResY=$Height",
    '-NoSound', '-nosplash', '-unattended', '-log', "-ABSLOG=$ProjectRoot\Saved\Logs\Shots.log")
$proc = Start-Process -FilePath $Editor -ArgumentList $cmdArgs -PassThru
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $proc.Kill()
    throw "Las capturas no terminaron en $TimeoutMinutes minutos"
}
Get-ChildItem $outDir -Filter *.png | ForEach-Object { Write-Output $_.FullName }
