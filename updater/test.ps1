[CmdletBinding()]
param([switch]$Compile)
. (Join-Path $PSScriptRoot 'core.ps1')
$script:checks=0
function Assert([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message }; $script:checks++ }
function Must-Fail([scriptblock]$Action,[string]$Message) { $failed=$false; try { & $Action | Out-Null } catch { $failed=$true }; Assert $failed $Message }
$testRoot=Join-Path $PSScriptRoot ('.test-'+[guid]::NewGuid().ToString('N'))
$testKey='HKCU:\SOFTWARE\WindhawkIslandUpdaterTest-'+[guid]::NewGuid().ToString('N')
New-Item -ItemType Directory -Path $testRoot | Out-Null
$rsa=New-Object Security.Cryptography.RSACryptoServiceProvider(3072)
$rsa.PersistKeyInCsp=$false
try {
    foreach ($file in Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1') {
        $tokens=$null; $parseErrors=$null
        [void][Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$parseErrors)
        Assert ($parseErrors.Count -eq 0) ('Syntax: '+$file.Name)
    }
    Assert ((Get-ReleaseVersion '1.10.0') -gt (Get-ReleaseVersion '1.9.0')) 'Numeric version ordering'
    foreach ($bad in @('1.0.0-beta','1.0','v1.0.0','01.0.0','1.0.0/../evil')) { Must-Fail { Get-ReleaseVersion $bad } ('Reject version '+$bad) }
    foreach ($bad in @('a/b/c','../repo','a/repo?x=1','https://github.com/a/b')) { Must-Fail { Assert-Repository $bad } ('Reject repository '+$bad) }
    $sourcePath=Join-Path (Split-Path $PSScriptRoot -Parent) 'dynamic-island-liquid-glass.wh.cpp'
    $sourceBytes=[IO.File]::ReadAllBytes($sourcePath)
    $version=[regex]::Match([IO.File]::ReadAllText($sourcePath),'(?m)^// @version\s+(\S+)').Groups[1].Value
    $m=[ordered]@{schema=1;version=$version;tag=('v'+$version);modId=$script:PublicId;sourceAsset='dynamic-island-liquid-glass.wh.cpp';sourceSha256=(Get-FileDigest $sourcePath);sourceBytes=$sourceBytes.Length;minUpdaterVersion='1.0.0';settings=@([pscustomobject]@{name='Appearance.OffsetX';kind='DWord';value=0})}
    $bytes=$script:Utf8.GetBytes(($m | ConvertTo-Json -Depth 10))
    $sig=$rsa.SignData($bytes,'SHA256')
    $public=$rsa.ToXmlString($false)
    $manifest=Get-VerifiedManifest $bytes $sig $public
    Assert-Source $sourcePath $manifest
    $script:checks++
    $corrupt=$bytes.Clone(); $corrupt[0]=0
    Must-Fail { Get-VerifiedManifest $corrupt $sig $public } 'Tampered manifest rejected'
    $bad=[ordered]@{}; foreach ($key in $m.Keys) { $bad[$key]=$m[$key] }; $bad.modId='other-mod'; $badBytes=$script:Utf8.GetBytes(($bad | ConvertTo-Json -Depth 10))
    Must-Fail { Get-VerifiedManifest $badBytes ($rsa.SignData($badBytes,'SHA256')) $public } 'Wrong signed mod ID rejected'
    $bad=[ordered]@{}; foreach ($key in $m.Keys) { $bad[$key]=$m[$key] }; $bad.settings=@([pscustomobject]@{name='bad\path';kind='String';value='bad'}); $badBytes=$script:Utf8.GetBytes(($bad | ConvertTo-Json -Depth 10))
    Must-Fail { Get-VerifiedManifest $badBytes ($rsa.SignData($badBytes,'SHA256')) $public } 'Registry path injection rejected'
    $damaged=Join-Path $testRoot 'damaged.wh.cpp'; [IO.File]::WriteAllText($damaged,'bad')
    Must-Fail { Assert-Source $damaged $manifest } 'Bad source rejected'
    $jsonPath=Join-Path $testRoot 'atomic.json'
    Write-JsonFile $jsonPath @{a=1}; Write-JsonFile $jsonPath @{a=2}
    Assert ((Read-JsonFile $jsonPath).a -eq 2) 'Atomic JSON replacement'
    [IO.File]::WriteAllText($jsonPath,'[{"name":"A","kind":"DWord","value":1},{"name":"B","kind":"String","value":"x"}]')
    $roundtrip=@{settings=@(Read-JsonFile $jsonPath)}
    Write-JsonFile $jsonPath $roundtrip
    $roundtrip=Read-JsonFile $jsonPath
    Assert ($roundtrip.settings -is [array] -and $roundtrip.settings.Count -eq 2 -and $roundtrip.settings[0].name -eq 'A') 'PowerShell 5.1 defaults array serialization'

    if ($Compile) {
        $actual=Get-WindhawkEnvironment
        Compile-Island $actual $sourcePath $version $testRoot
        foreach ($bits in @('32','64')) { Assert ((Get-Item -LiteralPath (Join-Path $testRoot "$bits.dll")).Length -gt 100000) ('Real compiler '+$bits) }
    }

    # Actual registry and file operations run only in the isolated HKCU fixture.
    $data=Join-Path $testRoot 'data'
    foreach ($dir in @('Engine\Mods\32','Engine\Mods\64','ModsSource')) { New-Item -ItemType Directory -Path (Join-Path $data $dir) -Force | Out-Null }
    $fixture=[pscustomobject]@{Key=($testKey+'\Mod');Storage=($testKey+'\Storage');Source=(Join-Path $data 'ModsSource\test.wh.cpp');Data=$data}
    $script:compileFails=$false; $script:healthFails=$false; $script:busy=$false
    function Compile-Island($HostEnv,$Source,$Version,$Stage) {
        if ($script:compileFails) { throw 'Simulated compiler failure' }
        foreach ($bits in @('32','64')) { [IO.File]::WriteAllText((Join-Path $Stage "$bits.dll"),'fixture DLL') }
    }
    function Get-ModProcesses {
        if ((Test-Path -LiteralPath $fixture.Key) -and (Get-Item -LiteralPath $fixture.Key).GetValue('Disabled',0) -eq 0) { [pscustomobject]@{ProcessId=123} }
    }
    function Wait-ModStopped { Assert (@(Get-ModProcesses).Count -eq 0) 'Fixture stopped' }
    function Wait-ModHealthy($HostEnv,$Version) { if ($script:healthFails) { throw 'Simulated startup failure' } }
    function Request-ModHandoff($HostEnv,$Old) { -not $script:busy }

    $state=Get-UpdaterState $testRoot
    $result=Invoke-IslandInstall $fixture $testRoot $manifest $sourcePath $state
    Assert ($result -eq ('Updated to '+$version)) 'First install succeeds'
    Assert ((Get-ModSnapshot $fixture).Version -eq $version) 'Installed registry version'
    $settings=Join-Path $fixture.Key 'Settings'
    Set-ItemProperty -LiteralPath $settings -Name 'Appearance.OffsetX' -Value 88
    New-ItemProperty -LiteralPath $fixture.Storage -Name ImportantDays -Value 'user-data' -PropertyType String | Out-Null
    $oldHash=Get-FileDigest $fixture.Source
    $next=$manifest.PSObject.Copy(); $next.version='1.5.0'
    $script:compileFails=$true
    Must-Fail { Invoke-IslandInstall $fixture $testRoot $next $sourcePath (Get-UpdaterState $testRoot) } 'Compile failure propagated'
    Assert ((Get-ModSnapshot $fixture).Version -eq $version -and (Get-ModSnapshot $fixture).Disabled -eq 0) 'Compile failure preserves running version'
    $script:compileFails=$false; $script:healthFails=$true
    Must-Fail { Invoke-IslandInstall $fixture $testRoot $next $sourcePath (Get-UpdaterState $testRoot) } 'Startup failure propagated'
    Assert ((Get-ModSnapshot $fixture).Version -eq $version -and (Get-FileDigest $fixture.Source) -eq $oldHash) 'Startup failure rolls back source/version'
    Assert ((Get-Item -LiteralPath $settings).GetValue('Appearance.OffsetX') -eq 88) 'Settings preserved on rollback'
    Assert ((Get-Item -LiteralPath $fixture.Storage).GetValue('ImportantDays') -eq 'user-data') 'User storage preserved on rollback'
    Assert (-not (Test-Path -LiteralPath (Join-Path $testRoot 'transaction.json'))) 'Completed rollback removes journal'
    $script:healthFails=$false; $script:busy=$true
    $result=Invoke-IslandInstall $fixture $testRoot $next $sourcePath (Get-UpdaterState $testRoot)
    Assert ($result -like '*busy*' -and (Get-ModSnapshot $fixture).Version -eq $version) 'Busy island defers update'
    $script:busy=$false
    Set-ModDisabled $fixture 1
    $result=Invoke-IslandInstall $fixture $testRoot $next $sourcePath (Get-UpdaterState $testRoot)
    Assert ((Get-ModSnapshot $fixture).Disabled -eq 1 -and (Get-ModSnapshot $fixture).Version -eq '1.5.0') 'Disabled mod remains disabled after update'
    Assert ((Get-Item -LiteralPath $settings).GetValue('Appearance.OffsetX') -eq 88) 'Settings preserved on update'
    [IO.File]::AppendAllText($fixture.Source,'// local edit')
    $next.version='1.6.0'
    Must-Fail { Invoke-IslandInstall $fixture $testRoot $next $sourcePath (Get-UpdaterState $testRoot) } 'Local edits protected'
    Copy-Item -LiteralPath $sourcePath -Destination $fixture.Source -Force
    $journal=Read-JsonFile (Join-Path $testRoot 'previous.json')
    Write-JsonFile (Join-Path $testRoot 'transaction.json') $journal
    $script:requests=0
    function Get-RemoteBytes($Url,$Limit) { $script:requests++; throw 'Simulated offline' }
    $result=Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Auto'
    Assert ($result -like 'Interrupted update pending*' -and $script:requests -eq 0 -and (Test-Path -LiteralPath (Join-Path $testRoot 'transaction.json'))) 'Automatic check does not restart an interrupted update'
    Must-Fail { Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Manual' } 'Manual check recovers interrupted transaction before an offline failure'
    Assert ((Get-ModSnapshot $fixture).Version -eq $version -and (Get-FileDigest $fixture.Source) -eq $oldHash) 'Interrupted transaction recovered'
    Assert ($script:requests -eq 1) 'One network attempt during manual recovery'
    Must-Fail { Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Auto' } 'Automatic offline check fails without retry'
    $result=Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Auto'
    Assert ($result -eq 'Already checked within three hours.' -and $script:requests -eq 2) 'Automatic failures also respect the three-hour interval'
    Must-Fail { Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Manual' } 'Manual check bypasses automatic interval'
    Assert ($script:requests -eq 3) 'Manual check requests latest release'
    $script:downloadIndex=0
    function Get-RemoteBytes($Url,$Limit) { $script:downloadIndex++; if ($Url.EndsWith('update.json')) { ,$bytes } elseif ($Url.EndsWith('update.sig')) { ,$sig } else { throw 'Unexpected download' } }
    $result=Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Manual'
    Assert ($result -eq 'Already current.' -and $script:downloadIndex -eq 2) 'Current signed release skips source download'
    Assert ((Get-Item -LiteralPath $fixture.Storage).GetValue('UpdateAvailable',-1) -eq 0) 'No update clears the badge'

    $newManifest=$manifest.PSObject.Copy(); $newManifest.version='1.4.6'; $newManifest.tag='v1.4.6'
    $newBytes=$script:Utf8.GetBytes(($newManifest | ConvertTo-Json -Depth 10))
    $newSignature=$rsa.SignData($newBytes,'SHA256')
    $script:downloadIndex=0
    function Get-RemoteBytes($Url,$Limit) { $script:downloadIndex++; if ($Url.EndsWith('update.json')) { ,$newBytes } elseif ($Url.EndsWith('update.sig')) { ,$newSignature } else { throw 'Automatic check must not download source' } }
    $saved=Get-UpdaterState $testRoot
    $saved | Add-Member -NotePropertyName lastAutoCheck -NotePropertyValue ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds()-10800) -Force
    Write-JsonFile (Join-Path $testRoot 'state.json') $saved
    Set-UpdateRequest $fixture 1
    $script:compileFails=$true
    $result=Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Auto'
    Assert ($result -eq 'Update available: 1.4.6' -and $script:downloadIndex -eq 2) 'Three-hour check downloads only signed metadata'
    Assert ((Get-Item -LiteralPath $fixture.Storage).GetValue('UpdateAvailable',0) -eq 1) 'New signed version sets the badge'
    Assert ((Get-ModSnapshot $fixture).Version -eq $version -and (Get-FileDigest $fixture.Source) -ceq $oldHash) 'Background availability check preserves running code'
    Assert ((Get-Item -LiteralPath $fixture.Storage).GetValue('UpdateRequested',0) -eq 1) 'Background availability check does not alter active handoff'
    $result=Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Auto'
    Assert ($result -eq 'Already checked within three hours.' -and $script:downloadIndex -eq 2) 'Successful checks also respect the three-hour interval'
    $newSignature[0]=$newSignature[0] -bxor 1
    Set-UpdateAvailable $fixture 0
    Must-Fail { Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Manual' } 'Invalid signatures cannot advertise updates'
    Assert ((Get-Item -LiteralPath $fixture.Storage).GetValue('UpdateAvailable',-1) -eq 0) 'Unsigned update leaves no badge'
    $script:compileFails=$false
    function Get-RemoteBytes($Url,$Limit) { if ($Url.EndsWith('update.json')) { ,$bytes } elseif ($Url.EndsWith('update.sig')) { ,$sig } else { throw 'Unexpected download' } }
    $saved=Get-UpdaterState $testRoot; $saved.version='0.0.0'; $saved.failedVersion=$version; Write-JsonFile (Join-Path $testRoot 'state.json') $saved
    Must-Fail { Invoke-IslandUpdate $fixture $testRoot ([pscustomobject]@{repository='test/repo'}) $public 'Manual' } 'Failed release not installed repeatedly'
    & {
        $account=[Security.Principal.WindowsIdentity]::GetCurrent().Name
        $setup=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'setup.ps1'))
        $start=$setup.IndexOf('$trigger = New-ScheduledTaskTrigger')
        $end=$setup.IndexOf("foreach (`$mode in",$start)
        Invoke-Expression $setup.Substring($start,$end-$start)
        Assert ($trigger.Repetition.Interval -ceq 'PT3H' -and -not $trigger.Repetition.Duration) 'Time trigger repeats every three hours without an expiration'
    }
    Write-Host ("PASS: $script:checks checks; isolated registry/files; no live mod or task changes.")
} finally {
    $rsa.Dispose()
    if (Test-Path -LiteralPath $testKey) { Remove-Item -LiteralPath $testKey -Recurse -Force }
    $resolved=[IO.Path]::GetFullPath($testRoot)
    $parent=[IO.Path]::GetFullPath($PSScriptRoot)+[IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $resolved -Leaf) -notmatch '^\.test-[a-f0-9]{32}$') { throw 'Unsafe test cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
