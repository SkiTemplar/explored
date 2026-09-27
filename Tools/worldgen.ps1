# Genera la vista previa, hornea el archipiélago o construye sus HLOD.
#
# -Mode hlod ejecuta el WorldPartitionHLODsBuilder de Epic (setup + build + finalize) sobre
# /Game/Maps/Archipelago: genera los actores HLOD (silueta de terreno fusionada/simplificada e
# instancing de vegetación) que definen las capas HLOD_Terrain/HLOD_Vegetation horneadas por
# ExploredWorldGen -mode=map (ver Source/ExploredEditor/WorldGenCommandlet.cpp). Hace falta
# volver a lanzarlo cada vez que se rehornea el mapa (map/bake), porque el mapa se recrea desde
# cero y los HLOD viejos no sobreviven a esa recreación.
param(
    [ValidateSet('preview', 'bake', 'terrain', 'map', 'hlod')] [string]$Mode = 'preview',
    [uint32]$Seed = 20260926,
    [string]$Extra = ''
)
. "$PSScriptRoot\common.ps1"

if ($Mode -eq 'hlod') {
    $cmdArgs = @($UProject, '/Game/Maps/Archipelago', '-run=WorldPartitionBuilderCommandlet',
        '-Builder=WorldPartitionHLODsBuilder', '-SetupHLODs', '-BuildHLODs', '-FinalizeHLODs',
        '-AllowCommandletRendering', '-unattended', '-nosplash', '-nopause', '-stdout', '-FullStdOutLogOutput')
    if ($Extra) { $cmdArgs += $Extra.Split(' ') }

    $code = Invoke-Native $EditorCmd $cmdArgs 'LogWorldPartitionHLODsBuilder|LogWorldPartitionBuilderCommandlet|Error:'
    if ($code -ne 0) { throw "WorldPartitionHLODsBuilder falló con código $code" }
    exit 0
}

$cmdArgs = @($UProject, '-run=ExploredWorldGen', "-mode=$Mode", "-seed=$Seed",
    '-unattended', '-nosplash', '-nopause', '-stdout', '-FullStdOutLogOutput')
if ($Extra) { $cmdArgs += $Extra.Split(' ') }

$code = Invoke-Native $EditorCmd $cmdArgs 'LogExplored|Error:'
if ($code -ne 0) { throw "WorldGen $Mode falló con código $code" }
