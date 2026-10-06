param(
    [string]$DdsRoot = 'C:\OpenDDS-3.34.0\OpenDDS-3.34.0',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$BuildDir = '',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'environment.ps1')
Import-MSVC
$root = Set-OpenDDSEnvironment $DdsRoot
$source = Split-Path $PSScriptRoot -Parent
if (!$BuildDir) { $BuildDir = Join-Path $source 'build-windows' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$suffix = if ($Configuration -eq 'Debug') { 'd' } else { '' }
$library = Join-Path $root "lib\OpenDDS_Dcps$suffix.dll"
if (!(Test-Path $library)) { throw "Falta $library. Selecciona la configuracion compilada o compila OpenDDS para $Configuration." }
$bytes = [IO.File]::ReadAllBytes($library)
$offset = [BitConverter]::ToInt32($bytes, 0x3c)
if ([BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0x8664) { throw 'El demo usa x64; la biblioteca OpenDDS no es x64.' }
Write-Host "OpenDDS: $root; MSVC 2022; x64; $Configuration"
foreach ($name in @("OpenDDS_Rtps$suffix.dll", "OpenDDS_Rtps_Udp$suffix.dll")) {
    if (!(Test-Path (Join-Path $root "lib\$name"))) { throw "Falta dependencia $name" }
}

# Preserve the installation. Adapt a private CMake package copy if it was moved.
$package = Join-Path $BuildDir 'opendds-package'
New-Item -ItemType Directory -Force -Path $package | Out-Null
Copy-Item -Path (Join-Path $root 'cmake\*') -Destination $package -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'VERSION.txt') -Destination $BuildDir -Force
$config = Get-Content (Join-Path $package 'config.cmake') -Raw
if ($config -notmatch 'set\(OPENDDS_SOURCE_DIR "([^"]+)"\)') { throw 'config.cmake no corresponde a una instalacion configure/MPC soportada.' }
$oldRoot = $matches[1]
$cmakeRoot = $root.Replace('\','/')
$config = $config.Replace($oldRoot, $cmakeRoot)
$config += "`nset(DDS_ROOT `"$cmakeRoot`")`nset(OPENDDS_INCLUDE_DIRS `"$cmakeRoot`")`nset(OPENDDS_BIN_DIR `"$cmakeRoot/bin`")`nset(OPENDDS_LIB_DIR `"$cmakeRoot/lib`")`n"
[IO.File]::WriteAllText((Join-Path $package 'config.cmake'), $config)
$cmake = Find-CMake
& $cmake -S $source -B $BuildDir -G 'Visual Studio 17 2022' -A x64 "-DOpenDDS_DIR=$package" -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configure fallo.' }
& $cmake --build $BuildDir --config $Configuration --parallel 4 -- /verbosity:minimal /clp:ErrorsOnly
if ($LASTEXITCODE -ne 0) { throw 'La compilacion fallo.' }
if ($Test) {
    $ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
    & $ctest --test-dir $BuildDir -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'CTest fallo. Si aparece 0xc0e90002, consulta el bloqueo de DLL por CodeIntegrity documentado en opendds/README.md.' }
}
