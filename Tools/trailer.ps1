# Graba el tráiler de Baliza Siete: un PNG por fotograma a tiempo fijo (UExploredTrailerSubsystem, -ExploredTrailer)
# según Tools/Trailer/trailer.json, fuera de pantalla y con prioridad BelowNormal. El final del juego («finale»)
# se graba en su propio proceso, después del resto. Al acabar comprueba que no falta ni se repite ningún
# fotograma, que los tiempos del JSON cuadran y deja hojas de contacto (un fotograma cada 0,5 s) por plano.
# Uso: Tools\trailer.ps1 [-Preview] [-Shot 03a-palmar,07-final] [-Width 1920] [-Height 1080] [-SkipBuild]
#   -Preview: 640x360 y uno de cada 3 fotogramas (10 fps equivalentes) en Saved\Trailer\preview.
#   Salida: Saved\Trailer\frames\f_00000.png..., Saved\Trailer\contact\*.png y Saved\Trailer\report.txt.
param(
    [switch]$Preview,
    [string[]]$Shot = @(),
    [int]$Width = 0,
    [int]$Height = 0,
    [int]$TimeoutMinutes = 90,
    [switch]$SkipBuild,
    [switch]$NoContactSheets
)
. "$PSScriptRoot\common.ps1"

# Con -File, «-Shot a,b» llega como una sola cadena.
$Shot = @($Shot | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$json = Join-Path $ProjectRoot 'Tools\Trailer\trailer.json'
$spec = Get-Content $json -Raw -Encoding UTF8 | ConvertFrom-Json
$fps = [int]$spec.fps
$stride = if ($Preview) { 3 } else { 1 }
if ($Width -le 0) { $Width = if ($Preview) { 640 } else { [int]$spec.width } }
if ($Height -le 0) { $Height = if ($Preview) { 360 } else { [int]$spec.height } }
$root = Join-Path $ProjectRoot 'Saved\Trailer'
$outDir = if ($Preview) { Join-Path $root 'preview\frames' } else { Join-Path $root 'frames' }
$contactDir = Join-Path $root 'contact'
New-Item -ItemType Directory -Force $outDir, $contactDir | Out-Null

# Línea de tiempo: fotogramas por plano y comprobación de los tiempos escritos en el JSON.
$timeline = @()
$frame = 0
$problems = @()
foreach ($s in $spec.shots) {
    $count = [int][math]::Round([double]$s.duration * $fps)
    $entry = [pscustomobject]@{ Name = $s.name; Kind = $(if ($s.kind) { $s.kind } else { 'normal' }); Start = $frame; Frames = $count }
    if ($null -ne $s.startFrame -and ([int]$s.startFrame -ne $frame -or [int]$s.frames -ne $count)) {
        $problems += "$($s.name): el JSON dice $($s.startFrame)+$($s.frames) y la duración da $frame+$count"
    }
    if ($null -ne $s.start -and ([math]::Abs([double]$s.start - $frame / $fps) -gt 0.001 -or [math]::Abs([double]$s.end - ($frame + $count) / $fps) -gt 0.001)) {
        $problems += "$($s.name): start/end no cuadran con los fotogramas"
    }
    $timeline += $entry
    $frame += $count
}
$totalFrames = $frame
foreach ($name in $Shot) {
    if (-not ($timeline | Where-Object Name -eq $name)) { throw "No hay ningún plano «$name» en $json" }
}
$selected = @($timeline | Where-Object { $Shot.Count -eq 0 -or $Shot -contains $_.Name })

# Nunca dos procesos de Unreal de este proyecto a la vez.
$others = @(Get-CimInstance Win32_Process -Filter "Name like 'UnrealEditor%' or Name like 'Explored%'" |
    Where-Object { $_.CommandLine -match 'Explored\.uproject|ExploredTrailer|Explored-Win64' })
if ($others.Count -gt 0) { throw "Hay un proceso de Unreal de Explored abierto (PID $($others.ProcessId -join ', ')); ciérralo antes" }

if (-not $SkipBuild) {
    $dll = Join-Path $ProjectRoot 'Binaries\Win64\UnrealEditor-Explored.dll'
    $newest = Get-ChildItem (Join-Path $ProjectRoot 'Source') -Recurse -Include *.cpp, *.h, *.cs | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not (Test-Path $dll) -or $newest.LastWriteTime -gt (Get-Item $dll).LastWriteTime) {
        & "$PSScriptRoot\build.ps1"
    }
}

# Fotogramas viejos de los planos que se van a grabar.
foreach ($s in $selected) {
    for ($f = $s.Start; $f -lt $s.Start + $s.Frames; $f++) {
        $file = Join-Path $outDir ('f_{0:D5}.png' -f $f)
        if (Test-Path $file) { Remove-Item $file -Force }
    }
}
Get-ChildItem $outDir -Filter 'report_*.json' -ErrorAction SilentlyContinue | Remove-Item -Force

# Grupos de proceso: todo lo normal junto (el haz al final) y cada «finale» solo.
$groups = @()
$normal = @($selected | Where-Object Kind -ne 'finale' | ForEach-Object Name)
if ($normal.Count -gt 0) { $groups += , $normal }
foreach ($s in ($selected | Where-Object Kind -eq 'finale')) { $groups += , @($s.Name) }

$index = 0
foreach ($group in $groups) {
    $index++
    $log = Join-Path $ProjectRoot "Saved\Logs\Trailer_$index.log"
    $gameArgs = @($UProject, '-game', "-ExploredTrailer=`"$json`"", "-TrailerDir=`"$outDir`"", "-TrailerShots=$($group -join ',')",
        "-TrailerStride=$stride", '-windowed', "-ResX=$Width", "-ResY=$Height", '-ForceRes', '-RenderOffscreen',
        '-NoSound', '-nosplash', '-unattended', '-log', "-ABSLOG=`"$log`"")
    Write-Output "[$index/$($groups.Count)] $($group -join ', ')"
    $proc = Start-Process -FilePath $Editor -ArgumentList $gameArgs -PassThru
    Start-Sleep -Milliseconds 500
    try { $proc.PriorityClass = 'BelowNormal' } catch { Write-Warning "No se pudo bajar la prioridad: $_" }
    if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
        # Solo el proceso propio, por PID (nunca por nombre de imagen: puede haber otros editores abiertos).
        & taskkill /PID $proc.Id /T /F | Out-Null
        throw "El grupo $index no terminó en $TimeoutMinutes minutos (log: $log)"
    }
    $done = Select-String -Path $log -Pattern '\[Trailer\] Terminado' -Encoding UTF8 | Select-Object -Last 1
    if (-not $done) { throw "El grupo $index no llegó al final (log: $log)" }
    Write-Output "  $($done.Line -replace '^.*\[Trailer\] ', '')"
    Select-String -Path $log -Pattern '\[Trailer\] (movimiento|final:|fotograma .* sin entregar)' -Encoding UTF8 | ForEach-Object { Write-Output "  $($_.Line -replace '^.*\[Trailer\] ', '')" }
}

# Verificación en disco: cada fotograma esperado existe una vez y dos seguidos no son idénticos.
$missing = @()
$identical = @()
$expected = 0
$lines = @()
foreach ($s in $selected) {
    $previousHash = $null
    for ($f = $s.Start; $f -lt $s.Start + $s.Frames; $f++) {
        if ($f % $stride -ne 0) { continue }
        $expected++
        $file = Join-Path $outDir ('f_{0:D5}.png' -f $f)
        if (-not (Test-Path $file)) { $missing += $f; $previousHash = $null; continue }
        $hash = (Get-FileHash $file -Algorithm MD5).Hash
        if ($hash -eq $previousHash) { $identical += $f }
        $previousHash = $hash
    }
    $lines += '{0,-22} {1,6:F2}-{2,6:F2} s  fotogramas {3,4}-{4,4} ({5})' -f $s.Name, ($s.Start / $fps), (($s.Start + $s.Frames) / $fps), $s.Start, ($s.Start + $s.Frames - 1), $s.Frames
}
$extra = @(Get-ChildItem $outDir -Filter 'f_*.png' | Where-Object {
        $n = [int]($_.BaseName.Substring(2)); $n -ge $totalFrames -or $n % $stride -ne 0 })

if (-not $NoContactSheets) {
    foreach ($s in $selected) {
        $list = Join-Path $env:TEMP "trailer_contact_$($s.Name).txt"
        $step = [int]($fps / 2)
        $files = @()
        for ($f = $s.Start; $f -lt $s.Start + $s.Frames; $f += $step) {
            $pick = $f - ($f % $stride)
            $file = Join-Path $outDir ('f_{0:D5}.png' -f $pick)
            if (Test-Path $file) { $files += "file '$($file -replace '\\', '/')'" }
        }
        if ($files.Count -eq 0) { continue }
        [IO.File]::WriteAllLines($list, $files)
        $cols = [math]::Min(4, $files.Count)
        $rows = [math]::Ceiling($files.Count / $cols)
        $sheet = Join-Path $contactDir ($(if ($Preview) { "preview_$($s.Name).png" } else { "$($s.Name).png" }))
        & ffmpeg -hide_banner -loglevel error -y -f concat -safe 0 -i $list -vf "scale=480:-2,tile=${cols}x${rows}:padding=4" -frames:v 1 $sheet
        Remove-Item $list -Force
    }
}

$report = @(
    "Tráiler: $(if ($Preview) { 'ensayo' } else { 'grabación' }) ${Width}x$Height, $fps fps, uno de cada $stride, $totalFrames fotogramas en total"
    $lines
    "Esperados $expected, faltan $($missing.Count), idénticos al anterior $($identical.Count), sobrantes $($extra.Count)"
)
if ($problems.Count -gt 0) { $report += 'Tiempos del JSON:'; $report += $problems }
if ($missing.Count -gt 0) { $report += "Faltan: $($missing -join ', ')" }
if ($identical.Count -gt 0) { $report += "Idénticos: $($identical -join ', ')" }
$reportFile = Join-Path $root $(if ($Preview) { 'report_preview.txt' } else { 'report.txt' })
[IO.File]::WriteAllLines($reportFile, $report)
$report | ForEach-Object { Write-Output $_ }
if ($missing.Count -gt 0 -or $problems.Count -gt 0) { throw 'El tráiler no está completo (ver el informe)' }
