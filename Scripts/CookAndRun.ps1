$Root = Resolve-Path "$PSScriptRoot/.."
$BuildDir = "$Root/Build"
$Cooker = "$BuildDir/bin/Debug/AssetCooker.exe"
$GameDir = "$BuildDir/Game/Sandbox/Debug"
$CookedDir = "$BuildDir/Game/Sandbox/Cooked/Assets"
$RawAssets = "$Root/Game/Sandbox/Assets"
$EngineSource = "$Root/Dev/Engine/Source"

# Ensure directories exist
New-Item -ItemType Directory -Force -Path "$CookedDir/Models" | Out-Null
New-Item -ItemType Directory -Force -Path "$CookedDir/Scenes" | Out-Null
New-Item -ItemType Directory -Force -Path "$CookedDir/Shaders" | Out-Null

# Compile Shaders
Write-Host "Compiling Shaders..."
$Shaders = @("Basic.vert", "Basic.frag", "Mesh.vert", "Mesh.frag", "MeshInstanced.vert", "MeshInstanced.frag", "Line.vert", "Line.frag")
foreach ($Shader in $Shaders) {
    $ShaderInput = "$EngineSource/Shaders/$Shader"
    $Output = "$EngineSource/Shaders/$Shader.spv"
    if (Test-Path $ShaderInput) {
        glslc $ShaderInput -o $Output
        Copy-Item $Output "$CookedDir/Shaders/" -Force
    }
}

# Cook Assets
Write-Host "Cooking Assets..."
if (Test-Path $Cooker) {
    & $Cooker COOK TEXTURE "$RawAssets/test.png" "$CookedDir/test.oaktex"
    & $Cooker COOK MESH "$RawAssets/Models/test.fbx" "$CookedDir/Models/test.oakmesh"
    & $Cooker COOK SKELETON "$RawAssets/Models/test.fbx" "$CookedDir/Models/test.oakskel"
    # Cook animation from test_anim.fbx as test.oakanim since test.fbx has no animation
    & $Cooker COOK ANIMATION "$RawAssets/Models/test_anim.fbx" "$CookedDir/Models/test.oakanim"
    & $Cooker COOK MESH "$RawAssets/Models/cube.obj" "$CookedDir/Models/cube.oakmesh"

    # Cook Joli
    & $Cooker COOK MESH "$RawAssets/Models/Joli.fbx" "$CookedDir/Models/Joli.oakmesh"
    & $Cooker COOK SKELETON "$RawAssets/Models/Joli.fbx" "$CookedDir/Models/Joli.oakskel"
    & $Cooker COOK ANIMATION "$RawAssets/Models/Joli.fbx" "$CookedDir/Models/Joli.oakanim"
} else {
    Write-Error "AssetCooker not found at $Cooker"
}

# Run Game
Write-Host "Running Game..."
if (Test-Path "$GameDir/Sandbox.exe") {
    Set-Location "$GameDir"
    & "./Sandbox.exe"
} else {
    Write-Error "Sandbox.exe not found at $GameDir"
}
