# Genera la vista previa o hornea el archipiélago con el commandlet ExploredWorldGen.
param(
    [ValidateSet('preview', 'bake')] [string]$Mode = 'preview',
    [uint32]$Seed = 20260926,
    [string]$Extra = ''
)
. "$PSScriptRoot\common.ps1"

$cmdArgs = @($UProject, '-run=ExploredWorldGen', "-mode=$Mode", "-seed=$Seed",
    '-unattended', '-nosplash', '-nopause', '-stdout', '-FullStdOutLogOutput')
if ($Extra) { $cmdArgs += $Extra.Split(' ') }

$code = Invoke-Native $EditorCmd $cmdArgs 'LogExplored|Error:'
if ($code -ne 0) { throw "WorldGen $Mode falló con código $code" }
