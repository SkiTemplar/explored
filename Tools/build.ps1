# Compila el editor (por defecto) o el juego.
param(
    [ValidateSet('Editor', 'Game')] [string]$Target = 'Editor',
    [ValidateSet('Development', 'DebugGame', 'Shipping')] [string]$Configuration = 'Development'
)
. "$PSScriptRoot\common.ps1"

$targetName = if ($Target -eq 'Editor') { 'ExploredEditor' } else { 'Explored' }
& $BuildBat $targetName Win64 $Configuration "-Project=$UProject" -WaitMutex -NoHotReloadFromIDE
Assert-ExitCode "Build $targetName"
