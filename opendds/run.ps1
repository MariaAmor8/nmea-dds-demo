# Arguments are forwarded verbatim so --local / --peer match the Linux CLI.
$ErrorActionPreference = 'Stop'
if ($args.Count -lt 1 -or $args[0] -notin @('publisher','subscriber')) {
    Write-Error 'Uso: .\opendds\run.ps1 publisher|subscriber --local IP --peer IP [opciones]'
    exit 2
}
. (Join-Path $PSScriptRoot 'scripts\environment.ps1')
$root = if ($env:MARINE_DDS_ROOT) { $env:MARINE_DDS_ROOT } else { 'C:\OpenDDS-3.34.0\OpenDDS-3.34.0' }
$root = Set-OpenDDSEnvironment $root
$configuration = if ($env:MARINE_DDS_CONFIG) { $env:MARINE_DDS_CONFIG } else { 'Debug' }
if ($configuration -notin @('Debug','Release')) { throw 'MARINE_DDS_CONFIG debe ser Debug o Release.' }
$build = if ($env:MARINE_DDS_BUILD_DIR) { $env:MARINE_DDS_BUILD_DIR } else { Join-Path $PSScriptRoot 'build-windows' }
$binary = Join-Path $build "$configuration\$($args[0]).exe"
if (!(Test-Path $binary)) { throw "Falta $binary. Ejecuta .\opendds\scripts\build.ps1 -Test (consulta opendds/README.md)." }
$suffix = if ($configuration -eq 'Debug') { 'd' } else { '' }
foreach ($dll in @("lib\OpenDDS_Dcps$suffix.dll", "lib\OpenDDS_Rtps$suffix.dll", "lib\OpenDDS_Rtps_Udp$suffix.dll", "ACE_wrappers\lib\ACE$suffix.dll", "ACE_wrappers\lib\TAO$suffix.dll")) {
    if (!(Test-Path (Join-Path $root $dll))) { throw "Falta dependencia $dll en $root." }
}
$forward = @($args | Select-Object -Skip 1)
& $binary @forward
if ($LASTEXITCODE -eq -1058471934) {
    Write-Host 'Windows bloqueo una dependencia por integridad/firma (0xc0e90002). Consulta los eventos CodeIntegrity y opendds/README.md.'
}
exit $LASTEXITCODE
