# Regenera el contenido del juego que se deriva de código: audio e importación a Unreal.
# El arte v1 (texturas, mallas, props y su horneado) se borró en el reset v2 (docs/v3/plan.md);
# build_materials.py e import_props.py vuelven al encadenado cuando existan M_Toon y el kit
# nuevo (F2 y F4). El mapa Content/Maps/Map_IslaSal es de autor y se versiona; para
# recrearlo desde cero: Tools\unreal_python.ps1 -Script Tools/Unreal/create_map_islasal.py
# Uso: Tools\build_content.ps1 [-Skip audio,import]
param([string[]]$Skip = @())
. "$PSScriptRoot\common.ps1"
# Con `powershell -File` la lista llega como una sola cadena "a,b": se separa aquí.
$Skip = @($Skip | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })

function Step([string]$Name, [scriptblock]$Body) {
    if ($Skip -contains $Name) { Write-Host "== $Name (omitido)"; return }
    Write-Host "== $Name"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & $Body
    Write-Host ("   {0:N1} s" -f $sw.Elapsed.TotalSeconds)
}

Push-Location $ProjectRoot
try {
    Step 'audio' {
        Push-Location (Join-Path $ProjectRoot 'Tools\Audio')
        try { uv run explored-audio build; Assert-ExitCode 'Audio' } finally { Pop-Location }
    }
    Step 'import' {
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/import_audio.py
    }
}
finally {
    Pop-Location
}
