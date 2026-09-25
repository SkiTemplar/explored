# Rutas compartidas por los scripts de herramientas.
$ErrorActionPreference = 'Stop'
$script:ProjectRoot = Split-Path -Parent $PSScriptRoot
$script:UProject = Join-Path $ProjectRoot 'Explored.uproject'
$script:EngineRoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.6' }
$script:BuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$script:EditorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$script:Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$script:RunUAT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$script:Blender = if ($env:BLENDER_EXE) { $env:BLENDER_EXE } else { 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' }

function Assert-ExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "$Step falló con código $LASTEXITCODE" }
}
