<#
.SYNOPSIS
  Lanza en sesiones en la nube de Claude Code las tareas pendientes de Tools/Nube/cola/*.md.
  Estas sesiones consumen el crédito de sesiones en la nube, no el plan.

.DESCRIPTION
  Cada fichero .md de la cola es el encargo completo de una sesión; comun.md se antepone
  a todos. Tras lanzar una tarea, su fichero se mueve a lanzadas/ con la fecha como prefijo
  y la salida del CLI (que incluye el id de sesión) se anota en lanzadas/registro.log.
  Las rutinas programadas NO consumen el crédito: por eso existe este lanzador.

.PARAMETER Max
  Número máximo de tareas que se lanzan en esta ejecución (3 por defecto).

.PARAMETER DryRun
  Muestra qué se lanzaría sin lanzar nada.
#>
param(
    [int]$Max = 3,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Cola = Join-Path $Root 'cola'
$Lanzadas = Join-Path $Root 'lanzadas'
$Registro = Join-Path $Lanzadas 'registro.log'
$Comun = Join-Path $Root 'comun.md'
$Claude = Join-Path $env:USERPROFILE '.local\bin\claude.exe'

if (-not (Test-Path $Claude)) { throw "No se encuentra el CLI de Claude en $Claude" }
New-Item -ItemType Directory -Force $Lanzadas | Out-Null

$Tareas = @(Get-ChildItem -Path $Cola -Filter '*.md' | Sort-Object Name | Select-Object -First $Max)
if ($Tareas.Count -eq 0) { Write-Host 'Cola vacía.'; exit 0 }

$Preambulo = if (Test-Path $Comun) { Get-Content -Raw -Encoding UTF8 $Comun } else { '' }

foreach ($Tarea in $Tareas) {
    $Encargo = $Preambulo + "`n`n" + (Get-Content -Raw -Encoding UTF8 $Tarea.FullName)
    if ($DryRun) {
        Write-Host "[dry-run] $($Tarea.Name): $($Encargo.Length) caracteres"
        continue
    }

    Write-Host "Lanzando $($Tarea.Name)..."
    # --cloud exige una terminal interactiva (TTY): no se puede capturar ni redirigir la
    # salida, así que el script se ejecuta a mano desde una terminal de Windows Terminal o
    # PowerShell, nunca desde un proceso sin consola ni desde otra sesión de Claude.
    # El repo se deduce del remoto git del directorio actual.
    Push-Location (Split-Path (Split-Path $Root))
    try { & $Claude --cloud $Encargo } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Falló $($Tarea.Name) (código $LASTEXITCODE); se queda en la cola."
        continue
    }

    $Sello = Get-Date -Format 'yyyy-MM-dd_HHmm'
    Move-Item $Tarea.FullName (Join-Path $Lanzadas "$($Sello)_$($Tarea.Name)")
    Add-Content -Encoding UTF8 $Registro "[$Sello] $($Tarea.Name) lanzada"
}
