# Playtest automático completo: lanza el set de capturas «playtest» (a pie, por isla y
# punto de interés, con el auditor de defectos UExploredPlaytestAuditor activo), espera a
# que termine, recoge las capturas y el informe, y monta una hoja de contacto.
# Uso: Tools\playtest.ps1 [-Width 1600] [-Height 900] [-TimeoutMinutes 45]
param(
    [int]$Width = 1600,
    [int]$Height = 900,
    [int]$TimeoutMinutes = 45
)
. "$PSScriptRoot\common.ps1"

$outDir = Join-Path $ProjectRoot 'Saved\Shots'
New-Item -ItemType Directory -Force $outDir | Out-Null
Get-ChildItem $outDir -Filter '*.png' -ErrorAction SilentlyContinue | Remove-Item -Force
Remove-Item (Join-Path $outDir 'playtest_report.json') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $outDir 'playtest_report.txt') -ErrorAction SilentlyContinue

$cmdArgs = @($UProject, '-game', '-ExploredShots=playtest', "-ShotsDir=$outDir", '-windowed', "-ResX=$Width", "-ResY=$Height",
    '-NoSound', '-nosplash', '-unattended', '-log', "-ABSLOG=$ProjectRoot\Saved\Logs\Playtest.log")
$proc = Start-Process -FilePath $Editor -ArgumentList $cmdArgs -PassThru
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $proc.Kill()
    throw "El playtest no terminó en $TimeoutMinutes minutos (revisa Saved\Logs\Playtest.log)"
}

$shots = Get-ChildItem $outDir -Filter '*.png'
Write-Output "Capturas: $($shots.Count) -> $outDir"

$reportJson = Join-Path $outDir 'playtest_report.json'
$reportTxt = Join-Path $outDir 'playtest_report.txt'
if (Test-Path $reportTxt) {
    Write-Output '--- Resumen del informe ---'
    Get-Content $reportTxt | ForEach-Object { Write-Output $_ }
    Write-Output "Informe completo: $reportJson"
}
else {
    Write-Warning "No se generó $reportTxt; revisa Saved\Logs\Playtest.log (¿arrancó UExploredPlaytestAuditor?)"
}

# Hoja de contacto (rejilla de miniaturas con su nombre): Python vía uv, nunca pip ni python directo.
$contactSheet = Join-Path $outDir 'playtest_contact.png'
uv run --with pillow python (Join-Path $PSScriptRoot 'Playtest\contact_sheet.py') --shots-dir $outDir --out $contactSheet
Assert-ExitCode 'Hoja de contacto'
Write-Output "Hoja de contacto: $contactSheet"
