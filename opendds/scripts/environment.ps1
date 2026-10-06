function Set-OpenDDSEnvironment {
    param([string]$DdsRoot)
    $root = (Resolve-Path -LiteralPath $DdsRoot -ErrorAction Stop).Path
    foreach ($file in @('VERSION.txt', 'cmake\OpenDDSConfig.cmake', 'cmake\config.cmake',
                         'bin\opendds_idl.exe', 'ACE_wrappers\bin\tao_idl.exe')) {
        if (!(Test-Path -LiteralPath (Join-Path $root $file))) { throw "Falta $file en $root. Consulta opendds/README.md." }
    }
    if ((Get-Content (Join-Path $root 'VERSION.txt') -Raw) -notmatch '3\.34\.0') {
        throw 'Este demo requiere OpenDDS 3.34.0.'
    }
    $env:DDS_ROOT = $root
    $env:ACE_ROOT = Join-Path $root 'ACE_wrappers'
    $env:TAO_ROOT = Join-Path $env:ACE_ROOT 'TAO'
    $env:PATH = "$root\bin;$root\lib;$env:ACE_ROOT\bin;$env:ACE_ROOT\lib;$env:PATH"
    # Do not source a relocated setenv.cmd: it can contain obsolete paths.
    return $root
}

function Find-CMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) {
            $candidate = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $candidate) { return $candidate }
        }
    }
    throw 'No se encontro CMake. Instala CMake o el componente CMake de Visual Studio.'
}

function Import-MSVC {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path $vswhere)) { throw 'Instala Visual Studio 2022 / Build Tools con Desktop development with C++.' }
    $vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'No se encontro MSVC de Visual Studio 2022.' }
    $devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
    $lines = & $env:ComSpec /c "call `"$devcmd`" -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'VsDevCmd fallo.' }
    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
}
