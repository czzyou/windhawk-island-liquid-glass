[CmdletBinding()]
param([switch]$InitializeKey, [switch]$PrepareOnly, [switch]$PublishPrepared, [string]$NotesFile)
. (Join-Path $PSScriptRoot 'core.ps1')
. (Join-Path $PSScriptRoot 'publish-checks.ps1')
$sourceRoot=Split-Path $PSScriptRoot -Parent
$release=Read-JsonFile (Join-Path $sourceRoot 'release.json')
Assert-Repository $release.repository
[void](Get-ReleaseVersion $release.version)
if ($release.repository.Split('/')[0] -cne $release.author) { throw 'Release author must own the configured repository.' }
$keyRoot=Join-Path $env:LOCALAPPDATA 'Windhawk-Island-Publisher'
$privatePath=Join-Path $keyRoot 'signing-key.dpapi'
Add-Type -AssemblyName System.Security
if ($InitializeKey) {
    if (Test-Path -LiteralPath $privatePath) { throw 'Signing key already exists; refusing to replace it.' }
    New-Item -ItemType Directory -Path $keyRoot -Force | Out-Null
    $acl=New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true,$false)
    $user=[Security.Principal.WindowsIdentity]::GetCurrent().User
    $acl.SetOwner($user)
    $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule($user,'FullControl','ContainerInherit,ObjectInherit','None','Allow')))
    Set-Acl -LiteralPath $keyRoot -AclObject $acl
    $rsa=New-Object Security.Cryptography.RSACryptoServiceProvider(3072)
    $rsa.PersistKeyInCsp=$false
    try {
        $privateBytes=$script:Utf8.GetBytes($rsa.ToXmlString($true))
        $encrypted=[Security.Cryptography.ProtectedData]::Protect($privateBytes,$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
        [IO.File]::WriteAllBytes($privatePath,$encrypted)
        [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'public-key.xml'),$rsa.ToXmlString($false),$script:Utf8)
        [Array]::Clear($privateBytes,0,$privateBytes.Length)
    } finally { $rsa.Dispose() }
    Write-Host ('Created user-bound signing key outside the repository: '+$privatePath)
    exit
}
if (-not (Test-Path -LiteralPath $privatePath)) { throw 'Run publish.ps1 -InitializeKey once first.' }
$tag='v'+$release.version
$output=Join-Path $sourceRoot ('release-output\'+$tag)
$source=Join-Path $sourceRoot 'dynamic-island-liquid-glass.wh.cpp'
$asset=Join-Path $output 'dynamic-island-liquid-glass.wh.cpp'
$zip=Join-Path $output 'island-setup.zip'
$manifestPath=Join-Path $output 'update.json'
if ($PublishPrepared -and $PrepareOnly) { throw 'Choose preparation or publication, not both.' }
if (-not $PublishPrepared) {
Push-Location $sourceRoot
try {
    & python build.py --release --source-only
    if ($LASTEXITCODE -ne 0) { throw 'Release source generation failed.' }
    Assert-ActionsBuild $release.repository $sourceRoot
} finally { Pop-Location }
New-Item -ItemType Directory -Path $output -Force | Out-Null
Copy-Item -LiteralPath $source -Destination $asset -Force
$defaultsPath=Join-Path $output 'settings.json'
& python (Join-Path $PSScriptRoot 'package_release.py') $source $defaultsPath
if ($LASTEXITCODE -ne 0) { throw 'Extracting defaults failed.' }
$manifest=[ordered]@{
    schema=1; version=$release.version; tag=$tag; modId=$script:PublicId
    sourceAsset='dynamic-island-liquid-glass.wh.cpp'; sourceSha256=(Get-FileDigest $asset)
    sourceBytes=(Get-Item -LiteralPath $asset).Length; minUpdaterVersion=$script:UpdaterVersion.ToString(3)
    settings=@(Read-JsonFile $defaultsPath)
}
$manifestPath=Join-Path $output 'update.json'
Write-JsonFile $manifestPath $manifest
$rsa=New-Object Security.Cryptography.RSACryptoServiceProvider
$rsa.PersistKeyInCsp=$false
$privateBytes=$null
try {
    $privateBytes=[Security.Cryptography.ProtectedData]::Unprotect([IO.File]::ReadAllBytes($privatePath),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    $rsa.FromXmlString($script:Utf8.GetString($privateBytes))
    $publicKey=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'public-key.xml'))
    if ($rsa.ToXmlString($false) -cne $publicKey) { throw 'Public/private signing keys do not match.' }
    [IO.File]::WriteAllBytes((Join-Path $output 'update.sig'), $rsa.SignData([IO.File]::ReadAllBytes($manifestPath),'SHA256'))
} finally { if ($privateBytes) { [Array]::Clear($privateBytes,0,$privateBytes.Length) }; $rsa.Dispose() }
$verified=Get-VerifiedManifest ([IO.File]::ReadAllBytes($manifestPath)) ([IO.File]::ReadAllBytes((Join-Path $output 'update.sig'))) $publicKey
Assert-Source $asset $verified
$setupRoot=Join-Path $output 'island-setup'
New-Item -ItemType Directory -Path $setupRoot -Force | Out-Null
foreach ($file in @('core.ps1','update.ps1','check.ps1','setup.ps1','uninstall.ps1','install.cmd','public-key.xml','README.md')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $setupRoot $file) -Force
}
Write-JsonFile (Join-Path $setupRoot 'config.json') ([ordered]@{repository=$release.repository})
$zip=Join-Path $output 'island-setup.zip'
Compress-Archive -LiteralPath $setupRoot -DestinationPath $zip -Force
Remove-Item -LiteralPath $defaultsPath -Force
} else {
    foreach ($path in @($asset,$manifestPath,(Join-Path $output 'update.sig'),$zip)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw 'Prepared release is incomplete. Run -PrepareOnly first.' }
    }
    $publicKey=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'public-key.xml'))
    $verified=Get-VerifiedManifest ([IO.File]::ReadAllBytes($manifestPath)) ([IO.File]::ReadAllBytes((Join-Path $output 'update.sig'))) $publicKey
    if ($verified.version -cne $release.version) { throw 'Prepared version mismatch. Run -PrepareOnly again.' }
    Assert-Source $asset $verified
    Assert-Source $source $verified
}
if ($PrepareOnly) { Write-Host ('Ready for review: '+$output); exit }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try {
    if (@($archive.Entries | Where-Object { $_.FullName -match '\.(ttf|otf|ttc)$' }).Count) { throw 'Font binaries must not be published in the setup package.' }
    foreach ($file in @('core.ps1','update.ps1','check.ps1','setup.ps1','uninstall.ps1','install.cmd','public-key.xml','README.md')) {
        $entry=@($archive.Entries | Where-Object { $_.FullName.Replace('\','/') -ceq ('island-setup/'+$file) })
        if ($entry.Count -ne 1) { throw ('Prepared setup file missing: '+$file) }
        $stream=$entry[0].Open(); $sha=[Security.Cryptography.SHA256]::Create()
        try { $digest=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
        finally { $stream.Dispose(); $sha.Dispose() }
        if ($digest -cne (Get-FileDigest (Join-Path $PSScriptRoot $file))) { throw 'Installer inputs changed. Run -PrepareOnly again.' }
    }
} finally { $archive.Dispose() }
if (-not $NotesFile -or -not (Test-Path -LiteralPath $NotesFile)) { throw 'Provide -NotesFile with the release notes, or use -PrepareOnly.' }
$assets=@($asset,$manifestPath,(Join-Path $output 'update.sig'),$zip)
& gh release create $tag @assets --repo $release.repository --draft --title ('Island '+$release.version) --notes-file $NotesFile
if ($LASTEXITCODE -ne 0) { throw 'Draft release upload failed; nothing was published.' }
& gh release edit $tag --repo $release.repository --draft=false --latest
if ($LASTEXITCODE -ne 0) { throw 'Draft uploaded but publication failed. Inspect the release before retrying.' }
Write-Host ('Published: https://github.com/'+$release.repository+'/releases/tag/'+$tag)
