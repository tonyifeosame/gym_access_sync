$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $repoRoot 'build'
$msysBin = 'C:\msys64\ucrt64\bin'

if (-not (Test-Path $msysBin)) {
    throw "MSYS2 UCRT runtime not found at $msysBin."
}

$compiler = Join-Path $msysBin 'g++.exe'
if (-not (Test-Path $compiler)) {
    throw 'Unable to find g++.exe. Install the MSYS2 UCRT toolchain.'
}

$env:PATH = $msysBin + ';' + $env:PATH
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$requiredDlls = @('libsqlite3-0.dll','libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')
foreach ($dll in $requiredDlls) {
    $sourceDll = Join-Path $msysBin $dll
    if (-not (Test-Path $sourceDll)) {
        throw "Required runtime DLL not found: $sourceDll"
    }
    Copy-Item $sourceDll $buildDir -Force
}

$exePath = Join-Path $buildDir 'integration_tests.exe'
Write-Host "Building $exePath"
& $compiler -std=c++17 -I. -o $exePath integration_test.cpp database.cpp api_client.cpp crow_server.cpp -lsqlite3 -lws2_32
if ($LASTEXITCODE -ne 0) {
    throw 'Integration test build failed.'
}

Push-Location $buildDir
try {
    & .\integration_tests.exe
    if ($LASTEXITCODE -ne 0) {
        throw 'Integration tests failed.'
    }
}
finally {
    Pop-Location
}
