# Regenera el contenido del juego que se deriva de código: audio, materiales, kits de arte
# (Art/Export/K*, Tools/Style) y su importación a Unreal, y el postproceso del mapa. El mapa
# Content/Maps/Map_IslaSal es de autor y se versiona; para recrearlo desde cero:
# Tools\unreal_python.ps1 -Script Tools/Unreal/create_map_islasal.py
# Uso: Tools\build_content.ps1 [-Skip audio,import,materials,props,post]
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
    Step 'materials' {
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/build_materials.py
    }
    Step 'props' {
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/import_props.py
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/count_textures.py
    }
    Step 'post' {
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/map_post.py
    }
}
finally {
    Pop-Location
}
