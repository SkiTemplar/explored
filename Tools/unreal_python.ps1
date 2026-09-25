# Ejecuta un script de Python del editor de Unreal en modo commandlet (sin interfaz).
param([Parameter(Mandatory = $true)] [string]$Script)
. "$PSScriptRoot\common.ps1"

$path = (Resolve-Path (Join-Path $ProjectRoot $Script)).Path
$cmdArgs = @($UProject, '-run=pythonscript', "-script=$path", '-unattended', '-nosplash', '-nopause', '-stdout', '-FullStdOutLogOutput')
$code = Invoke-Native $EditorCmd $cmdArgs 'LogPython|LogExplored|Error:'
if ($code -ne 0) { throw "Script $Script falló con código $code" }
