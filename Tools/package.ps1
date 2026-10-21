# Empaqueta Baliza Siete para Windows (Win64) en Saved/Package/Windows.
# Pasos: datos (DataCheck --strict), compilación del editor (lo usa el cocinado) y del juego con
# Build.bat -WaitMutex, y BuildCookRun -skipbuild (con -build, UAT choca con el mutex de UBT si
# otro proceso compila). Al final comprueba que el paquete trae el ejecutable, los .pak/.utoc y
# los JSON de Content/Data sueltos (DirectoriesToAlwaysStageAsNonUFS, Config/DefaultGame.ini).
# Uso: Tools\package.ps1 [-Configuration Shipping|Development] [-SkipChecks]
#   Development deja dentro el subsistema de capturas: sirve para Tools\bench.ps1 -Exe.
param(
    [ValidateSet('Development', 'Shipping')] [string]$Configuration = 'Shipping',
    # Omite DataCheck y la compilación previa (si ya se acaban de hacer).
    [switch]$SkipChecks
)
. "$PSScriptRoot\common.ps1"

$archive = Join-Path $ProjectRoot 'Saved\Package'

if (-not $SkipChecks) {
    Push-Location (Join-Path $ProjectRoot 'Tools\DataCheck')
    try { uv run datacheck --strict; Assert-ExitCode 'DataCheck' } finally { Pop-Location }
    & "$PSScriptRoot\build.ps1"
    & "$PSScriptRoot\build.ps1" -Target Game -Configuration $Configuration
}

$cmdArgs = @('BuildCookRun', "-project=$UProject", '-noP4', '-platform=Win64', "-clientconfig=$Configuration",
    '-skipbuild', '-nocompileeditor', '-cook', '-map=/Game/Maps/Map_IslaSal', '-stage', '-pak', '-iostore',
    '-package', '-prereqs', '-archive', "-archivedirectory=$archive", '-unattended', '-utf8output',
    # UnrealMCPython (solo editor) se conecta al cocinado sin interfaz y llena el log de errores.
    '-AdditionalCookerOptions=-DisablePlugins=UnrealMCPython')
if ($Configuration -eq 'Shipping') { $cmdArgs += '-nodebuginfo' }
$code = Invoke-Native $RunUAT $cmdArgs '(Error|error C|BUILD SUCCESSFUL|BUILD FAILED|AutomationTool exiting)'
if ($code -ne 0) { throw "BuildCookRun falló con código $code" }

$root = Join-Path $archive 'Windows'
$suffix = if ($Configuration -eq 'Shipping') { '-Win64-Shipping' } else { '' }
$expected = @(
    (Join-Path $root 'Explored.exe'),
    (Join-Path $root "Explored\Binaries\Win64\Explored$suffix.exe"),
    (Join-Path $root 'Explored\Content\Data\island.json'),
    (Join-Path $root 'Explored\Content\Data\items.json'),
    (Join-Path $root 'Explored\Content\Data\soundtrack.json')
)
$missing = @($expected | Where-Object { -not (Test-Path $_) })
$paks = @(Get-ChildItem -Path (Join-Path $root 'Explored\Content\Paks') -Include '*.pak', '*.utoc' -Recurse -ErrorAction SilentlyContinue)
if ($paks.Count -eq 0) { $missing += 'Explored\Content\Paks\*.pak|*.utoc' }
if ($missing.Count -gt 0) { throw "Paquete incompleto; falta: $($missing -join ', ')" }

$sizeMB = (Get-ChildItem -Path $root -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Output ("Paquete {0}: {1} ({2:N0} MB)" -f $Configuration, $root, $sizeMB)
