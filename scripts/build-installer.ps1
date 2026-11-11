[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$WixBinPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$wixTools = (Resolve-Path -LiteralPath $WixBinPath).Path
$packageDir = Join-Path $projectRoot 'artifacts\installer'
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
$objectPath = Join-Path $packageDir 'Product.wixobj'
$msiPath = Join-Path $packageDir 'PS3EyeFilters-preview.msi'
$decompiledPath = Join-Path $packageDir 'verified-package.wxs'
& (Join-Path $wixTools 'candle.exe') -nologo -arch x64 "-dSolutionDir=$projectRoot/" `
    -out $objectPath (Join-Path $projectRoot 'PS3EyeInstaller\Product.wxs')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
& (Join-Path $wixTools 'light.exe') -nologo -ext (Join-Path $wixTools 'WixUIExtension.dll') `
    "-dWixUILicenseRtf=$projectRoot\PS3EyeInstaller\GPL3.rtf" -out $msiPath $objectPath
if ($LASTEXITCODE -ne 0) { throw 'Installer link/ICE validation failed.' }
& (Join-Path $wixTools 'dark.exe') -nologo $msiPath -o $decompiledPath
if ($LASTEXITCODE -ne 0) { throw 'Installer inspection failed.' }
[xml]$package = Get-Content -LiteralPath $decompiledPath -Raw
$files = @($package.SelectNodes('//*[local-name()="File"]'))
if ($files.Count -ne 2 -or @($files | Where-Object { $_.Name -notlike 'PS3EyeSourceFilter*.dll' }).Count) {
    throw 'Unexpected installer payload; expected only the two DirectShow DLLs.'
}
if ($package.SelectNodes('//*[local-name()="Certificate" or local-name()="Driver"]').Count) {
    throw 'The filter installer must not install certificates or USB drivers.'
}
$customActions = @($package.SelectNodes('//*[local-name()="CustomAction"]'))
if (@($customActions | Where-Object { $_.Id -match 'Certificate|DIFx|Driver' }).Count) {
    throw 'Unexpected certificate/driver custom action.'
}
Write-Output "Validated filter-only preview: $msiPath"
Write-Output 'This command does not install the MSI or register the DLLs.'
