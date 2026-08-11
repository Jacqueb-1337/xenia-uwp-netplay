param(
  [switch]$WrapperOnly
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$project = Join-Path $root 'xenia-canary-uwp\xenia-canary-uwp.vcxproj'
$cppWinRTProps = Join-Path $root 'build\packages\Microsoft.Windows.CppWinRT.2.0.220110.5\build\native\Microsoft.Windows.CppWinRT.props'

if (!(Test-Path $project)) {
  throw "UWP project not found: $project"
}

if (!(Test-Path $cppWinRTProps)) {
  throw 'Microsoft.Windows.CppWinRT 2.0.220110.5 is not restored under build\packages. Restore xenia-canary-uwp\packages.config first.'
}

$msbuildCandidates = @(
  'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
  'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
  'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
  'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'
)
$msbuild = $msbuildCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (!$msbuild) {
  throw 'Visual Studio 2022 MSBuild was not found.'
}

$subject = 'CN=Xenia Canary UWP'
$now = Get-Date
$cert = Get-ChildItem 'Cert:\CurrentUser\My' |
  Where-Object {
    $_.Subject -eq $subject -and
    $_.HasPrivateKey -and
    $_.NotAfter -gt $now.AddDays(30)
  } |
  Sort-Object NotAfter -Descending |
  Select-Object -First 1

if (!$cert) {
  Write-Host 'Creating a local Xbox Dev Mode signing certificate...'
  $cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject $subject `
    -FriendlyName 'Xenia Canary UWP Dev Mode' `
    -CertStoreLocation 'Cert:\CurrentUser\My' `
    -KeyExportPolicy Exportable `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -HashAlgorithm SHA256 `
    -NotAfter $now.AddYears(5)
}

Write-Host "Signing certificate: $($cert.Thumbprint)"
Write-Host "Certificate expires: $($cert.NotAfter)"

$buildArgs = @(
  $project,
  '/m',
  '/t:Build',
  '/p:Configuration=Release',
  '/p:Platform=x64',
  "/p:PackageCertificateThumbprint=$($cert.Thumbprint)",
  '/p:GenerateAppxPackageOnBuild=true',
  '/p:AppxBundle=Always',
  '/p:AppxBundlePlatforms=x64'
)

if ($WrapperOnly) {
  $buildArgs += '/p:BuildProjectReferences=false'
}

& $msbuild @buildArgs
if ($LASTEXITCODE -ne 0) {
  throw "MSBuild failed with exit code $LASTEXITCODE."
}

$appPackages = Join-Path $root 'xenia-canary-uwp\AppPackages'
$bundle = Get-ChildItem $appPackages -Filter '*_x64.appxbundle' -File -Recurse |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
if (!$bundle) {
  throw 'Build succeeded, but no x64 AppXBundle was found.'
}

$packageDir = $bundle.Directory.FullName
$dependency = Get-ChildItem (Join-Path $packageDir 'Dependencies\x64') -Filter 'Microsoft.VCLibs.x64*.appx' -File -ErrorAction SilentlyContinue |
  Select-Object -First 1
$certificate = Get-ChildItem $packageDir -Filter '*.cer' -File -ErrorAction SilentlyContinue |
  Select-Object -First 1

$artifactDir = Join-Path $root 'artifacts\xenia-canary-netplay-uwp-xbox-devmode'
if (Test-Path $artifactDir) {
  Remove-Item $artifactDir -Recurse -Force
}
New-Item -ItemType Directory -Path $artifactDir -Force | Out-Null
Copy-Item $bundle.FullName $artifactDir
if ($dependency) { Copy-Item $dependency.FullName $artifactDir }
if ($certificate) { Copy-Item $certificate.FullName $artifactDir }

$hash = (Get-FileHash $bundle.FullName -Algorithm SHA256).Hash
@(
  'Xenia Canary Netplay UWP - Xbox Dev Mode',
  "Bundle: $($bundle.Name)",
  "Bundle bytes: $($bundle.Length)",
  "SHA256: $hash",
  "Publisher: $subject",
  "Certificate expires: $($cert.NotAfter.ToString('yyyy-MM-dd'))",
  '',
  'Install the .appxbundle through Xbox Device Portal.',
  'If Device Portal requests a dependency, add Microsoft.VCLibs.x64.14.00.appx.'
) | Set-Content (Join-Path $artifactDir 'BUILD_INFO.txt')

$zipPath = Join-Path $root 'artifacts\xenia-canary-netplay-uwp-xbox-devmode.zip'
Compress-Archive -Path "$artifactDir\*" -DestinationPath $zipPath -Force

Write-Host ''
Write-Host 'Xbox Dev Mode package complete.'
Write-Host "Bundle: $($bundle.FullName)"
Write-Host "Artifact folder: $artifactDir"
Write-Host "Zip: $zipPath"
