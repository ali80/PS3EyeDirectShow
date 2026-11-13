[CmdletBinding()]
param([ValidateSet('x64', 'Win32')][string]$Platform = 'x64')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 C++ build tools and a Windows 10/11 SDK.' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (!$msbuild) { throw 'Visual Studio C++ build tools were not found.' }
if (!(Test-Path -LiteralPath (Join-Path $projectRoot 'libusb\libusb\core.c'))) {
    throw 'Initialize dependencies first: git submodule update --init --recursive'
}
function Build-Project([string]$Project, [string]$SolutionDirectory, [string[]]$Extra = @()) {
    & $msbuild (Join-Path $projectRoot $Project) /m /nologo /v:minimal `
        /p:Configuration=Release "/p:Platform=$Platform" /p:PlatformToolset=v143 `
        /p:WindowsTargetPlatformVersion=10.0 "/p:SolutionDir=$SolutionDirectory" `
        "/p:ForceImportBeforeCppTargets=$PSScriptRoot\RuntimeAndHardening.props" @Extra
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $Project" }
}
Push-Location $projectRoot
try {
    Build-Project 'libusb\msvc\libusb_static_2017.vcxproj' "$projectRoot\libusb\msvc\"
    Build-Project 'PS3EyeDriverMSVC.sln' "$projectRoot\" @('/t:PS3EyeSourceFilter', "/p:Platform=$(if ($Platform -eq 'Win32') { 'x86' } else { 'x64' })")
    Build-Project 'tests\capture_contract_tests.vcxproj' "$projectRoot\"
    & (Join-Path $projectRoot "artifacts\$Platform\tests\capture_contract_tests.exe")
    if ($LASTEXITCODE -ne 0) { throw 'Capture contract tests failed.' }
    Build-Project 'tools\capture\ps3eye-capture.vcxproj' "$projectRoot\"
    & (Join-Path $projectRoot "artifacts\$Platform\capture\ps3eye-capture.exe") --self-test
    if ($LASTEXITCODE -ne 0) { throw 'Capture helper conversion tests failed.' }
} finally { Pop-Location }
