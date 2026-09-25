# Regenera todo el contenido del juego a partir del código: texturas, mallas, audio,
# importación a Unreal, materiales y horneado del archipiélago.
# Uso: Tools\build_content.ps1 [-Skip textures,meshes,audio,import,materials,world]
param([string[]]$Skip = @())
. "$PSScriptRoot\common.ps1"

function Step([string]$Name, [scriptblock]$Body) {
    if ($Skip -contains $Name) { Write-Host "== $Name (omitido)"; return }
    Write-Host "== $Name"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & $Body
    Write-Host ("   {0:N1} s" -f $sw.Elapsed.TotalSeconds)
}

Push-Location $ProjectRoot
try {
    Step 'textures' {
        uv run --with numpy --with pillow python Tools/Textures/gen_textures.py
        Assert-ExitCode 'Texturas'
    }
    Step 'meshes' {
        $code = Invoke-Native $Blender @('-b', '--factory-startup', '--python', (Join-Path $ProjectRoot 'Tools\Blender\run_all.py')) '(ERROR|Error|\[)'
        if ($code -ne 0) { throw "Blender falló con código $code" }
    }
    Step 'audio' {
        Push-Location (Join-Path $ProjectRoot 'Tools\Audio')
        try { uv run explored-audio build; Assert-ExitCode 'Audio' } finally { Pop-Location }
    }
    Step 'import' {
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/import_textures.py
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/import_meshes.py
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/import_audio.py
    }
    Step 'materials' {
        & "$PSScriptRoot\unreal_python.ps1" -Script Tools/Unreal/build_materials.py
    }
    Step 'world' {
        & "$PSScriptRoot\worldgen.ps1" -Mode bake
    }
}
finally {
    Pop-Location
}
