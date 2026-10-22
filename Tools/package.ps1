# Empaqueta Baliza Siete para Windows (Win64) en Saved/Package/Windows.
# Pasos: audio local presente (no se versiona), .locres compilados (GatherText), datos (DataCheck
# --strict), compilación del editor (lo usa el cocinado) y del juego con
# Build.bat -WaitMutex, y BuildCookRun -skipbuild (con -build, UAT choca con el mutex de UBT si
# otro proceso compila). Al final comprueba que el paquete trae el ejecutable, los .pak/.utoc y
# los JSON de Content/Data sueltos (DirectoriesToAlwaysStageAsNonUFS, Config/DefaultGame.ini), el
# audio de muestra cocinado y el .locres inglés preparado para el .pak.
# Uso: Tools\package.ps1 [-Configuration Shipping|Development] [-SkipChecks]
#   Development deja dentro el subsistema de capturas: sirve para Tools\bench.ps1 -Exe.
param(
    [ValidateSet('Development', 'Shipping')] [string]$Configuration = 'Shipping',
    # Omite DataCheck y la compilación previa (si ya se acaban de hacer).
    [switch]$SkipChecks
)
. "$PSScriptRoot\common.ps1"

$archive = Join-Path $ProjectRoot 'Saved\Package'

# El audio no se versiona (README, «Requisitos»): sin él el paquete sale mudo y nada más lo detecta.
# Una muestra por carpeta que carga el código: clases de sonido, música grabada, efectos y ambiente.
$audioProbes = @(
    'Audio\Classes\SC_Master.uasset',
    'Audio\Musica\mus_rec_macleod_canon_in_d.uasset',
    'Generated\Audio\Efectos\sfx_ship_horn.uasset',
    'Generated\Audio\Ambiente\amb_ocean_calm.uasset'
)
$missingAudio = @($audioProbes | Where-Object { -not (Test-Path (Join-Path $ProjectRoot "Content\$_")) })
if ($missingAudio.Count -gt 0) {
    throw "Falta el audio local ($($missingAudio -join ', ')): genéralo e impórtalo como dice README.md («Requisitos»)"
}

# Los .locres que carga el juego (Ajustes → Idioma) los compila el editor desde los .archive.
$locres = @('es', 'en' | ForEach-Object { Join-Path $ProjectRoot "Content\Localization\Game\$_\Game.locres" })
# El commandlet reescribe el .ini que recibe (le quita los comentarios): se le pasa una copia.
$compileIni = Join-Path $ProjectRoot 'Saved\Temp\Game_Compile.ini'
New-Item -ItemType Directory -Force -Path (Split-Path $compileIni) | Out-Null
Copy-Item (Join-Path $ProjectRoot 'Config\Localization\Game_Compile.ini') $compileIni -Force
$code = Invoke-Native $EditorCmd @($UProject, '-run=GatherText', "-config=$compileIni",
    '-unattended', '-nopause', '-NoSound', '-DisablePlugins=UnrealMCPython') '(Error|GenerateTextLocalizationResource.*(Completed|failed))'
if ($code -ne 0) { throw "La compilación de la localización falló con código $code" }
$missingLocres = @($locres | Where-Object { -not (Test-Path $_) })
if ($missingLocres.Count -gt 0) { throw "No se generaron los .locres: $($missingLocres -join ', ')" }

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
# Lo cocinado es lo que entra en el .ucas: el audio de muestra tiene que haber pasado por el cocinado.
$cooked = Join-Path $ProjectRoot 'Saved\Cooked\Windows\Explored\Content'
$missing += @($audioProbes | Where-Object { -not (Test-Path (Join-Path $cooked $_)) } | ForEach-Object { "audio cocinado $_" })
# Los .locres van dentro del .pak: el registro de UAT tiene que haberlos preparado.
$stageLog = Get-ChildItem -Path (Join-Path $ProjectRoot 'Saved\StagedBuilds\Windows') -Filter 'Manifest_UFSFiles_Win64.txt' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($stageLog -and -not (Select-String -Path $stageLog.FullName -Pattern 'Localization/Game/en/Game.locres' -SimpleMatch -Quiet)) {
    $missing += 'Localization/Game/en/Game.locres en el paquete'
}
if ($missing.Count -gt 0) { throw "Paquete incompleto; falta: $($missing -join ', ')" }

$sizeMB = (Get-ChildItem -Path $root -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Output ("Paquete {0}: {1} ({2:N0} MB)" -f $Configuration, $root, $sizeMB)
