# Empaqueta Baliza Siete para Windows (Win64) en Saved/Package/Windows.
# Pasos: audio local presente (no se versiona), datos (DataCheck --strict), compilación del editor
# (lo usan GatherText y el cocinado) y del juego con Build.bat -WaitMutex, .locres compilados
# (GatherText), y BuildCookRun -skipbuild (con -build, UAT choca con el mutex de UBT si
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

if (-not $SkipChecks) {
    Push-Location (Join-Path $ProjectRoot 'Tools\DataCheck')
    try { uv run datacheck --strict; Assert-ExitCode 'DataCheck' } finally { Pop-Location }
    & "$PSScriptRoot\build.ps1"
    & "$PSScriptRoot\build.ps1" -Target Game -Configuration $Configuration
}

# Los .locres que carga el juego (Ajustes → Idioma) los compila el editor desde los .archive. Va
# después de compilar el editor: el commandlet necesita los módulos del proyecto (clon limpio).
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

$root = Join-Path $archive 'Windows'
# El archivado de UAT no vacía la carpeta: sin esto quedan restos del paquete anterior (el .pdb de
# otra configuración, manifiestos viejos) que inflan el tamaño y se cuelan en lo que se entrega.
if (Test-Path $root) { Remove-Item $root -Recurse -Force }

$cmdArgs = @('BuildCookRun', "-project=$UProject", '-noP4', '-platform=Win64', "-clientconfig=$Configuration",
    '-skipbuild', '-nocompileeditor', '-cook', '-map=/Game/Maps/Map_IslaSal', '-stage', '-pak', '-iostore',
    '-package', '-prereqs', '-archive', "-archivedirectory=$archive", '-unattended', '-utf8output',
    # UnrealMCPython (solo editor) se conecta al cocinado sin interfaz y llena el log de errores.
    '-AdditionalCookerOptions=-DisablePlugins=UnrealMCPython')
if ($Configuration -eq 'Shipping') { $cmdArgs += '-nodebuginfo' }
$cookStart = Get-Date
$code = Invoke-Native $RunUAT $cmdArgs '(Error|error C|BUILD SUCCESSFUL|BUILD FAILED|AutomationTool exiting)'
if ($code -ne 0) { throw "BuildCookRun falló con código $code" }

# Avisos de terceros (EULA de Unreal, CC BY, MIT, OFL) a la vista, en la raíz del paquete.
$licenses = Join-Path $ProjectRoot 'Content\Licenses\THIRD_PARTY_LICENSES.txt'
if (Test-Path $licenses) { Copy-Item $licenses $root -Force }
$suffix = if ($Configuration -eq 'Shipping') { '-Win64-Shipping' } else { '' }
$expected = @(
    (Join-Path $root 'Explored.exe'),
    (Join-Path $root 'THIRD_PARTY_LICENSES.txt'),
    (Join-Path $root 'Explored\Content\Licenses\THIRD_PARTY_LICENSES.txt'),
    (Join-Path $root "Explored\Binaries\Win64\Explored$suffix.exe"),
    (Join-Path $root 'Explored\Content\Data\island.json'),
    (Join-Path $root 'Explored\Content\Data\items.json'),
    (Join-Path $root 'Explored\Content\Data\soundtrack.json'),
    # Césped continuo (GrassConfigLoader) y campo del suelo: sin ellos no hay césped.
    (Join-Path $root 'Explored\Content\Data\grass.json'),
    (Join-Path $root 'Explored\Content\Data\ground_field.bin')
)
$missing = @($expected | Where-Object { -not (Test-Path $_) })
$paks = @(Get-ChildItem -Path (Join-Path $root 'Explored\Content\Paks') -Include '*.pak', '*.utoc' -Recurse -ErrorAction SilentlyContinue)
if ($paks.Count -eq 0) { $missing += 'Explored\Content\Paks\*.pak|*.utoc' }
# Lo cocinado es lo que entra en el .ucas: el audio de muestra tiene que haber pasado por el cocinado.
$cooked = Join-Path $ProjectRoot 'Saved\Cooked\Windows\Explored\Content'
$missing += @($audioProbes | Where-Object { -not (Test-Path (Join-Path $cooked $_)) } | ForEach-Object { "audio cocinado $_" })
# Mallas del césped (/Game/Art/K6_Cesped, cargadas por ruta desde grass.json) y suelo de la isla.
$meshProbes = @('Art\K6_Cesped\SM_GrassPatch_A_01.uasset', 'Art\K6_Cesped\SM_Clover_01.uasset', 'Art\Island\SM_IslaSal_Terrain.uasset')
$missing += @($meshProbes | Where-Object { -not (Test-Path (Join-Path $cooked $_)) } | ForEach-Object { "malla cocinada $_" })
# Los .locres, las fuentes y la hoja de iconos de Slate van dentro del .pak: el manifiesto de UAT
# tiene que listarlos.
$ufsManifest = Join-Path $root 'Manifest_UFSFiles_Win64.txt'
$ufsProbes = @('Explored/Content/Localization/Game/en/Game.locres', 'Explored/Content/Slate/Fonts/Lora-Regular.ttf',
    'Explored/Content/Slate/T_IconosUI.png')
if (Test-Path $ufsManifest) {
    $missing += @($ufsProbes | Where-Object { -not (Select-String -Path $ufsManifest -Pattern $_ -SimpleMatch -Quiet) } |
        ForEach-Object { "$_ en el .pak" })
} else { $missing += 'Manifest_UFSFiles_Win64.txt' }
# Un material que no compila se cocina como el de por defecto y el cocinado sigue dando «Success».
$cookLog = Get-ChildItem -Path (Join-Path $env:APPDATA 'Unreal Engine\AutomationTool\Logs') -Filter 'Cook-*.txt' -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $cookStart } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $cookLog) { $missing += 'registro del cocinado (Cook-*.txt)' }
else {
    $badShaders = @(Select-String -Path $cookLog.FullName -Pattern 'Failed to compile Material (\S+)' |
        ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
    if ($badShaders.Count -gt 0) { throw "Materiales sin compilar en el cocinado ($($cookLog.FullName)): $($badShaders -join ', ')" }
}
if ($missing.Count -gt 0) { throw "Paquete incompleto; falta: $($missing -join ', ')" }

$sizeMB = (Get-ChildItem -Path $root -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Output ("Paquete {0}: {1} ({2:N0} MB)" -f $Configuration, $root, $sizeMB)
