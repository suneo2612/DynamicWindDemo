param(
    [ValidateSet('vs2026', 'vs2022')]
    [string]$Preset = 'vs2026',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
} else {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw 'CMake not found. Install CMake 3.25+ or Visual Studio C++ CMake tools.'
    }
    $installations = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
    $cmake = $installations | ForEach-Object {
        Join-Path $_ 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    } | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $cmake) {
        throw 'Visual Studio C++ CMake tools are missing.'
    }
}

Push-Location $root
try {
    & $cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    $buildPreset = "$Preset-$($Configuration.ToLowerInvariant())"
    & $cmake --build --preset $buildPreset --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
} finally {
    Pop-Location
}
