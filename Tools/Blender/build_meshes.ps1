# Genera, valida y previsualiza el kit de vegetación y rocas de Explored.
#
# Encadena, con Blender 5.2 en modo headless:
#   1. run_all.py       -> genera y exporta todas las mallas + manifest.json
#   2. validate.py      -> reimporta cada FBX y comprueba presupuesto de
#                           triángulos, geometría degenerada, dimensiones
#                           plausibles, color de vértice y colisión watertight
#                           en las rocas
#   3. render_preview.py -> lámina de contacto en Art/Export/Meshes/preview.png
#
# Uso:
#   pwsh Tools/Blender/build_meshes.ps1            # genera + valida + preview
#   pwsh Tools/Blender/build_meshes.ps1 -SkipPreview
#
# Idempotente: las semillas de cada malla son fijas, así que repetir la
# ejecución sobrescribe los mismos ficheros sin acumular basura.

param(
    [switch]$SkipPreview
)

. (Join-Path $PSScriptRoot '..\common.ps1')

$RunAll = Join-Path $PSScriptRoot 'run_all.py'
$Validate = Join-Path $PSScriptRoot 'validate.py'
$RenderPreview = Join-Path $PSScriptRoot 'render_preview.py'

Write-Host '== Generando mallas (run_all.py) =='
$code = Invoke-Native $Blender @('-b', '--factory-startup', '--python', $RunAll) '\[run_all\]|Error|Traceback'
Assert-ExitCode 'run_all.py'

Write-Host ''
Write-Host '== Validando el kit (validate.py) =='
$code = Invoke-Native $Blender @('-b', '--factory-startup', '--python', $Validate) 'OK|FALLO|\[validate\]'
if ($code -ne 0) { throw 'validate.py encontró fallos: revisa el detalle de arriba.' }

if (-not $SkipPreview) {
    Write-Host ''
    Write-Host '== Renderizando lámina de contacto (render_preview.py) =='
    Invoke-Native $Blender @('-b', '--factory-startup', '--python', $RenderPreview) '\[render_preview\]|Error|Traceback'
    Assert-ExitCode 'render_preview.py'
    Write-Host "Lámina escrita en Art\Export\Meshes\preview.png"
}

Write-Host ''
Write-Host 'Kit generado y validado correctamente.'
