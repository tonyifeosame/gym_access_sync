$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $repoRoot 'build'
$msysBin = 'C:\msys64\ucrt64\bin'

if (-not (Test-Path $msysBin)) {
    throw "MSYS2 UCRT runtime not found at $msysBin."
}

$compiler = Join-Path $msysBin 'g++.exe'
if (-not (Test-Path $compiler)) {
    $compilerCmd = Get-Command 'g++.exe' -ErrorAction SilentlyContinue
    if ($compilerCmd) {
        $compiler = $compilerCmd.Source
    } else {
        throw 'Unable to find g++.exe. Install the MSYS2 UCRT toolchain.'
    }
}

$env:PATH = $msysBin + ';' + $env:PATH

if (Test-Path $buildDir) {
    try {
        Remove-Item $buildDir -Recurse -Force -ErrorAction Stop
    } catch {
        $timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
        $buildDir = Join-Path $repoRoot ("build_" + $timestamp)
    }
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$requiredDlls = @('libsqlite3-0.dll','libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')
foreach ($dll in $requiredDlls) {
    $sourceDll = Join-Path $msysBin $dll
    if (-not (Test-Path $sourceDll)) {
        throw "Required runtime DLL not found: $sourceDll"
    }
    Copy-Item $sourceDll $buildDir -Force
}

$unitSources = @('unit_tests.cpp','database.cpp','mock_esp32_device.cpp','api_client.cpp','fingerprint_service.cpp','mock_fingerprint_scanner.cpp','door_controller.cpp','mock_relay.cpp','validation.cpp')
$integrationSources = @('integration_test.cpp','database.cpp','api_client.cpp','crow_server.cpp')
$mainSources = @('main.cpp','database.cpp','api_client.cpp','crow_server.cpp','sync.cpp','validation.cpp')

$unitExe = Join-Path $buildDir 'unit_tests.exe'
$integrationExe = Join-Path $buildDir 'checkin_test.exe'
$mainExe = Join-Path $buildDir 'gym_access_terminal.exe'

Write-Host "Building $unitExe"
& $compiler -std=c++17 -I. -o $unitExe @unitSources -lsqlite3 -lws2_32
if ($LASTEXITCODE -ne 0) { throw 'Unit test build failed.' }

Write-Host "Building $integrationExe"
& $compiler -std=c++17 -I. -o $integrationExe @integrationSources -lsqlite3 -lws2_32
if ($LASTEXITCODE -ne 0) { throw 'Integration test build failed.' }

Write-Host "Building $mainExe"
& $compiler -std=c++17 -I. -o $mainExe @mainSources -lsqlite3 -lws2_32
if ($LASTEXITCODE -ne 0) { throw 'Main app build failed.' }

Write-Host "Build complete."
Write-Host ""
Write-Host "Executables created:"
Write-Host "  $unitExe"
Write-Host "  $integrationExe"
Write-Host "  $mainExe"
Write-Host ""
Write-Host "Run them manually:"
Write-Host "  .\build\unit_tests.exe"
Write-Host "  .\build\checkin_test.exe"