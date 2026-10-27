Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:ModId = 'local@dynamic-island-liquid-glass'
$script:PublicId = 'dynamic-island-liquid-glass'
$script:UpdaterVersion = [version]'1.0.3'
$script:Utf8 = New-Object System.Text.UTF8Encoding($false, $true)

function Read-JsonFile([string]$Path) {
    $value=$script:Utf8.GetString([IO.File]::ReadAllBytes($Path)) | ConvertFrom-Json
    foreach ($entry in $value) { $entry }
}

function Write-JsonFile([string]$Path, $Value) {
    $tmp = $Path + '.tmp'
    [IO.File]::WriteAllText($tmp, ($Value | ConvertTo-Json -Depth 15), $script:Utf8)
    if ([IO.File]::Exists($Path)) { [IO.File]::Replace($tmp, $Path, ($Path+'.bak')); [IO.File]::Delete($Path+'.bak') }
    else { [IO.File]::Move($tmp, $Path) }
}

function Get-ReleaseVersion([string]$Text) {
    if ($Text -cnotmatch '^(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})$') {
        throw 'Invalid stable version.'
    }
    [version]$Text
}

function Assert-Repository([string]$Repository) {
    if ($Repository -cnotmatch '^[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9][A-Za-z0-9_.-]{0,99}$') {
        throw 'Invalid GitHub repository.'
    }
}

function Get-FileDigest([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }

function Get-VerifiedManifest([byte[]]$Bytes, [byte[]]$Signature, [string]$PublicKey) {
    if ($Bytes.Length -gt 262144 -or $Signature.Length -gt 1024) { throw 'Manifest too large.' }
    $rsa = New-Object Security.Cryptography.RSACryptoServiceProvider
    $rsa.PersistKeyInCsp = $false
    try {
        $rsa.FromXmlString($PublicKey)
        if ($rsa.KeySize -lt 3072 -or -not $rsa.VerifyData($Bytes, 'SHA256', $Signature)) { throw 'Release signature invalid.' }
    } finally { $rsa.Dispose() }
    $m = $script:Utf8.GetString($Bytes) | ConvertFrom-Json
    $required = @('schema','version','tag','modId','sourceAsset','sourceSha256','sourceBytes','minUpdaterVersion','settings')
    if (@($m.PSObject.Properties).Count -ne $required.Count) { throw 'Unexpected manifest fields.' }
    foreach ($name in $required) { if (-not $m.PSObject.Properties[$name]) { throw "Missing manifest field: $name" } }
    $v = Get-ReleaseVersion $m.version
    if ($m.schema -ne 1 -or $m.tag -cne "v$($m.version)" -or $m.modId -cne $script:PublicId -or
        $m.sourceAsset -cne 'dynamic-island-liquid-glass.wh.cpp' -or $m.sourceSha256 -cnotmatch '^[a-f0-9]{64}$' -or
        $m.sourceBytes -isnot [ValueType] -or $m.sourceBytes -lt 1 -or $m.sourceBytes -gt 8388608) {
        throw 'Invalid release manifest.'
    }
    if ((Get-ReleaseVersion $m.minUpdaterVersion) -gt $script:UpdaterVersion) { throw 'Please reinstall the latest setup package to upgrade the updater.' }
    if ($m.settings -isnot [array] -or $m.settings.Count -gt 512) { throw 'Invalid settings defaults.' }
    $seen = @{}
    foreach ($s in $m.settings) {
        if ($s.name -cnotmatch '^[A-Za-z0-9_.-]{1,128}$' -or $seen.ContainsKey($s.name) -or
            $s.kind -cnotin @('DWord','String')) { throw 'Invalid settings entry.' }
        $seen[$s.name] = $true
        if ($s.kind -eq 'DWord') {
            if ($s.value -isnot [ValueType] -or [long]$s.value -lt [int]::MinValue -or [long]$s.value -gt [int]::MaxValue) { throw 'Invalid integer default.' }
        } elseif ($s.value -isnot [string] -or $s.value.Length -gt 4096) { throw 'Invalid string default.' }
    }
    $m
}

function Assert-Source([string]$Path, $Manifest) {
    if ((Get-Item -LiteralPath $Path).Length -ne $Manifest.sourceBytes -or (Get-FileDigest $Path) -cne $Manifest.sourceSha256) { throw 'Source digest/length mismatch.' }
    $source = [IO.File]::ReadAllText($Path, $script:Utf8)
    $meta = [regex]::Match($source, '(?s)\A\s*// ==WindhawkMod==\r?\n(.*?)// ==/WindhawkMod==')
    if (-not $meta.Success) { throw 'Source metadata missing.' }
    foreach ($entry in @(@('id', $script:PublicId), @('version', $Manifest.version), @('include', 'windhawk.exe'))) {
        $matches = [regex]::Matches($meta.Groups[1].Value, '(?m)^// @' + $entry[0] + '\s+([^\r\n]+)\r?$')
        if ($matches.Count -ne 1 -or $matches[0].Groups[1].Value.Trim() -cne $entry[1]) { throw "Source $($entry[0]) mismatch." }
    }
    if (-not $source.Contains('UpdateHeartbeat') -or -not $source.Contains('UpdateRequested')) { throw 'Source lacks updater handoff.' }
}

function Get-RemoteBytes([string]$Url, [int]$Limit) {
    $uri = [uri]$Url
    if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'github.com' -or $uri.Port -ne 443 -or $uri.UserInfo) {
        throw 'Only the configured GitHub release source is allowed.'
    }
    $downloadFailures = @()
    foreach ($prefix in @('', 'https://ghfast.top/', 'https://gh-proxy.org/')) {
        try { return ,(Get-DownloadBytes ($prefix + $uri.AbsoluteUri) $Limit) }
        catch [Net.WebException], [IO.IOException] {
            $downloadFailures += ('{0}: {1}' -f ([uri]($prefix + $uri.AbsoluteUri)).Host, $_.Exception.Message)
        }
    }
    throw ('GitHub and backup download sources are unavailable. The update was not installed. Try again later. ' + ($downloadFailures -join '; '))
}

function Get-DownloadBytes([string]$Url, [int]$Limit) {
    $uri = [uri]$Url
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $request = [Net.HttpWebRequest]::Create($uri)
    $request.UserAgent = 'Windhawk-Island-Updater/' + $script:UpdaterVersion.ToString(3)
    $request.Timeout = 10000
    $request.ReadWriteTimeout = 15000
    $request.MaximumAutomaticRedirections = 5
    $request.CachePolicy = New-Object Net.Cache.RequestCachePolicy([Net.Cache.RequestCacheLevel]::NoCacheNoStore)
    $response = $null
    $buffer = New-Object byte[] 16384
    $output = New-Object IO.MemoryStream
    $deadline = [DateTime]::UtcNow.AddSeconds(60)
    try {
        $response = $request.GetResponse()
        if ($response.ResponseUri.Scheme -ne 'https' -or $response.ContentLength -gt $Limit) { throw 'Invalid download response.' }
        if ($response.ContentType -like 'text/html*') { throw [Net.WebException]::new('Download source returned an HTML page instead of a release asset.') }
        $stream = $response.GetResponseStream()
        while (($count = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            if ($output.Length + $count -gt $Limit -or [DateTime]::UtcNow -gt $deadline) { throw 'Download size/time limit exceeded.' }
            $output.Write($buffer, 0, $count)
        }
        if ($output.Length -eq 0 -or ($response.ContentLength -ge 0 -and $output.Length -ne $response.ContentLength)) {
            throw [Net.WebException]::new('Download response is empty or incomplete.')
        }
        ,$output.ToArray()
    } finally { if ($response) { $response.Close() }; $output.Dispose() }
}

function Get-WindhawkEnvironment {
    if (-not [Environment]::Is64BitProcess -or $env:PROCESSOR_ARCHITECTURE -ne 'AMD64') { throw 'Use 64-bit Windows PowerShell on x64 Windows.' }
    $root = Join-Path $env:ProgramFiles 'Windhawk'
    $ini = Join-Path $root 'windhawk.ini'
    if (-not (Test-Path -LiteralPath $ini)) { throw 'Windhawk installation not found.' }
    $cfg = @{}
    foreach ($line in [IO.File]::ReadAllLines($ini)) {
        if ($line -match '^([A-Za-z]+)=(.*)$') { $cfg[$matches[1]] = [Environment]::ExpandEnvironmentVariables($matches[2].Trim()) }
    }
    if ($cfg.Portable -ne '0' -or $cfg.EnginePath -ne 'Engine\1.7.3' -or $cfg.RegistryKey -ne 'HKLM\SOFTWARE\Windhawk') { throw 'This updater currently supports Windhawk 1.7.3 installed mode only.' }
    $compiler = Join-Path $root $cfg.CompilerPath
    $engine = Join-Path $root $cfg.EnginePath
    foreach ($file in @((Join-Path $compiler 'bin\clang++.exe'), (Join-Path $engine '64\windhawk.lib'), (Join-Path $engine '32\windhawk.lib'))) {
        if (-not (Test-Path -LiteralPath $file)) { throw "Windhawk component missing: $file" }
    }
    [pscustomobject]@{
        Root=$root; Compiler=$compiler; Engine=$engine; Data=$cfg.AppDataPath
        Key="HKLM:\SOFTWARE\Windhawk\Engine\Mods\$script:ModId"
        Storage="HKLM:\SOFTWARE\Windhawk\Engine\ModsWritable\$script:ModId\LocalStorage"
        Source=(Join-Path $cfg.AppDataPath "ModsSource\$script:ModId.wh.cpp")
    }
}

function Get-ModSnapshot($HostEnv) {
    if (-not (Test-Path -LiteralPath $HostEnv.Key)) { return $null }
    $key = Get-Item -LiteralPath $HostEnv.Key
    $values = @()
    foreach ($name in $key.GetValueNames()) {
        $values += [pscustomobject]@{ Name=$name; Kind=$key.GetValueKind($name).ToString(); Value=$key.GetValue($name) }
    }
    [pscustomobject]@{ Values=$values; Version=$key.GetValue('Version',''); Disabled=[int]$key.GetValue('Disabled',0); Library=$key.GetValue('LibraryFileName','') }
}

function Set-ModDisabled($HostEnv, [int]$Value) {
    New-ItemProperty -LiteralPath $HostEnv.Key -Name Disabled -PropertyType DWord -Value $Value -Force | Out-Null
}

function Get-ModProcesses {
    @(Get-CimInstance Win32_Process -Filter "Name='windhawk.exe'" | Where-Object { $_.CommandLine -match ('-tool-mod\s+"?' + [regex]::Escape($script:ModId) + '"?(?:\s|$)') })
}

function Wait-ModStopped {
    for ($i=0; $i -lt 50; $i++) { if (@(Get-ModProcesses).Count -eq 0) { return }; Start-Sleep -Milliseconds 200 }
    throw 'Island did not stop; no process was forcibly terminated.'
}

function Set-UpdateRequest($HostEnv, [int]$Value) {
    if (-not (Test-Path -LiteralPath $HostEnv.Storage)) { New-Item -Path $HostEnv.Storage -Force | Out-Null }
    New-ItemProperty -LiteralPath $HostEnv.Storage -Name UpdateRequested -PropertyType DWord -Value $Value -Force | Out-Null
}

function Set-UpdateAvailable($HostEnv, [int]$Value) {
    if (-not (Test-Path -LiteralPath $HostEnv.Storage)) { New-Item -Path $HostEnv.Storage -Force | Out-Null }
    New-ItemProperty -LiteralPath $HostEnv.Storage -Name UpdateAvailable -PropertyType DWord -Value $Value -Force | Out-Null
}

function Get-ModStorage($HostEnv) {
    if (Test-Path -LiteralPath $HostEnv.Storage) { Get-ItemProperty -LiteralPath $HostEnv.Storage }
}

function Test-Handoff($HostEnv, [string]$Version) {
    $storage = Get-ModStorage $HostEnv
    $processes = @(Get-ModProcesses)
    if (-not $storage -or -not $storage.PSObject.Properties['UpdateHeartbeat']) { return $false }
    $uptime = (Get-CimInstance Win32_PerfFormattedData_PerfOS_System).SystemUpTime
    $difference = [long]$uptime - [long]$storage.UpdateHeartbeat
    $processMatches = @($processes | Where-Object { $_.ProcessId -eq $storage.UpdatePid }).Count -eq 1
    ($storage.UpdateVersion -ceq $Version -and $processMatches -and $difference -ge -2 -and $difference -lt 8 -and $storage.UpdateReady -eq 1 -and $storage.UpdateBusy -eq 0)
}

function Request-ModHandoff($HostEnv, $Old) {
    if ($Old.Disabled -eq 1 -or @(Get-ModProcesses).Count -eq 0) { return $true }
    Set-UpdateRequest $HostEnv 1
    for ($i=0; $i -lt 4; $i++) { Start-Sleep -Seconds 1; if (Test-Handoff $HostEnv $Old.Version) { return $true } }
    Set-UpdateRequest $HostEnv 0
    $false
}

function Compile-Island($HostEnv, [string]$Source, [string]$Version, [string]$Stage) {
    $clang = Join-Path $HostEnv.Compiler 'bin\clang++.exe'
    $libraries = 'ole32 oleaut32 shcore d2d1 dwrite dwmapi gdi32 user32 shell32 runtimeobject windowscodecs avrt setupapi winhttp pdh winmm d3d11 dxgi dcomp d3dcompiler dxguid comctl32'.Split(' ')
    foreach ($bits in @('64','32')) {
        $target = if ($bits -eq '64') { 'x86_64-w64-mingw32' } else { 'i686-w64-mingw32' }
        $output = Join-Path $Stage "$bits.dll"
        $arguments = @('-std=c++23','-O2','-shared','-DUNICODE','-D_UNICODE','-DWH_MOD',
            '-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-D_WIN32_IE=0x0A00','-DNTDDI_VERSION=0x0A000008','-D__USE_MINGW_ANSI_STDIO=0',
            ('-DWH_MOD_ID=L\"' + $script:ModId + '\"'), ('-DWH_MOD_VERSION=L\"' + $Version + '\"'),
            (Join-Path $HostEnv.Engine "$bits\windhawk.lib"), '-x','c++',$Source,'-include','windhawk_api.h','-target',$target,
            '-Wl,--export-all-symbols','-o',$output) + @($libraries | ForEach-Object { '-l' + $_ })
        Push-Location $HostEnv.Compiler
        try {
            & $clang @arguments 2>&1 | Out-File -LiteralPath (Join-Path $Stage "compile-$bits.log") -Encoding utf8
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $output)) { throw "Compile failed ($bits-bit). See compile-$bits.log." }
        } finally { Pop-Location }
    }
}

function Add-MissingSettings($HostEnv, $Settings) {
    $path = Join-Path $HostEnv.Key 'Settings'
    if (-not (Test-Path -LiteralPath $path)) { New-Item -Path $path | Out-Null }
    $key = Get-Item -LiteralPath $path
    foreach ($s in $Settings) {
        if ($s.name -notin $key.GetValueNames()) { New-ItemProperty -LiteralPath $path -Name $s.name -Value $s.value -PropertyType $s.kind | Out-Null }
    }
}

function Set-NewMod($HostEnv, $Manifest, [string]$Library) {
    if (-not (Test-Path -LiteralPath $HostEnv.Key)) { New-Item -Path $HostEnv.Key -Force | Out-Null }
    Set-ModDisabled $HostEnv 1
    foreach ($pair in @(@('LibraryFileName',$Library), @('Version',$Manifest.version), @('Include','windhawk.exe'), @('Exclude',''), @('Architecture',''))) {
        New-ItemProperty -LiteralPath $HostEnv.Key -Name $pair[0] -Value $pair[1] -PropertyType String -Force | Out-Null
    }
    Add-MissingSettings $HostEnv $Manifest.settings
}

function Restore-ModSnapshot($HostEnv, $Old, [string]$Backup) {
    if ($null -eq $Old) {
        if (Test-Path -LiteralPath $HostEnv.Key) { Remove-Item -LiteralPath $HostEnv.Key -Recurse -Force }
        if (Test-Path -LiteralPath $HostEnv.Source) { Remove-Item -LiteralPath $HostEnv.Source -Force }
        return
    }
    foreach ($v in $Old.Values) {
        if ($v.Name -ne 'Disabled') { New-ItemProperty -LiteralPath $HostEnv.Key -Name $v.Name -Value $v.Value -PropertyType $v.Kind -Force | Out-Null }
    }
    Copy-Item -LiteralPath (Join-Path $Backup 'previous.wh.cpp') -Destination $HostEnv.Source -Force
    Set-ModDisabled $HostEnv $Old.Disabled
}

function Wait-ModHealthy($HostEnv, [string]$Version) {
    $expectedPid = 0
    $observation = $null
    for ($i=0; $i -lt 40; $i++) {
        Start-Sleep -Seconds 1
        $storage = Get-ModStorage $HostEnv
        $processes = @(Get-ModProcesses)
        if ($storage -and $storage.PSObject.Properties['UpdateVersion'] -and $storage.UpdateVersion -ceq $Version -and
            $storage.PSObject.Properties['UpdateRequiresGlass'] -and
            ($storage.UpdateRequiresGlass -eq 0 -or ($storage.PSObject.Properties['NativeGlassReady'] -and $storage.NativeGlassReady -eq 1)) -and
            @($processes | Where-Object { $_.ProcessId -eq $storage.UpdatePid }).Count -eq 1) {
            if ($expectedPid -eq 0) { $expectedPid = [int]$storage.UpdatePid; $observation=[Diagnostics.Stopwatch]::StartNew() }
            if ($expectedPid -ne $storage.UpdatePid) { throw 'Island restarted during startup observation.' }
            $uptime = (Get-CimInstance Win32_PerfFormattedData_PerfOS_System).SystemUpTime
            $age=[long]$uptime - [long]$storage.UpdateHeartbeat
            if ($age -ge -2 -and $age -lt 8 -and $observation.Elapsed.TotalSeconds -ge 30) { return }
        } elseif ($expectedPid -ne 0) { throw 'Island failed during startup observation.' }
    }
    throw 'New island startup did not pass the 30-second health check.'
}

function Get-ManagedDllPath($HostEnv, [string]$Bits, [string]$Library) {
    if ($Bits -notin @('32','64') -or $Library -cnotmatch '^local@dynamic-island-liquid-glass_[0-9.]+_[a-f0-9]{12}\.dll$') { throw 'Invalid managed DLL name.' }
    Join-Path $HostEnv.Data "Engine\Mods\$Bits\$Library"
}

function Restore-Transaction($HostEnv, [string]$StateRoot, $Journal) {
    if ($Journal.schema -ne 1 -or $Journal.stage -cnotmatch '^[a-f0-9]{32}$') { throw 'Invalid transaction journal.' }
    $backup = Join-Path $StateRoot $Journal.stage
    if (Test-Path -LiteralPath $HostEnv.Key) { Set-ModDisabled $HostEnv 1; Wait-ModStopped }
    Restore-ModSnapshot $HostEnv $Journal.old $backup
    Set-UpdateRequest $HostEnv 0
    if ($Journal.old -and $Journal.old.Disabled -eq 0) {
        # shortcut: old pre-updater releases have no heartbeat; verify their tool process starts.
        for ($i=0; $i -lt 50; $i++) { if (@(Get-ModProcesses).Count -gt 0) { break }; Start-Sleep -Milliseconds 200 }
        if (@(Get-ModProcesses).Count -eq 0) { throw 'Rollback configuration restored, but the old island did not start.' }
    }
}

function Invoke-IslandInstall($HostEnv, [string]$StateRoot, $Manifest, [string]$Source, $State) {
    $old = Get-ModSnapshot $HostEnv
    if ($old) {
        if (-not (Test-Path -LiteralPath $HostEnv.Source)) { throw 'Existing source missing; preserving installed mod.' }
        if ($State.sourceSha256 -and (Get-FileDigest $HostEnv.Source) -cne $State.sourceSha256) { throw 'Local source was edited; refusing to overwrite it.' }
        if (-not $State.sourceSha256) { throw 'Existing mod is not managed by this updater. Back it up and migrate explicitly.' }
        if ((Get-ReleaseVersion $old.Version) -ge (Get-ReleaseVersion $Manifest.version)) { return 'Already current.' }
    }
    $stageId = [guid]::NewGuid().ToString('N')
    $stage = Join-Path $StateRoot $stageId
    New-Item -ItemType Directory -Path $stage | Out-Null
    Copy-Item -LiteralPath $Source -Destination (Join-Path $stage 'source.wh.cpp')
    try { Compile-Island $HostEnv (Join-Path $stage 'source.wh.cpp') $Manifest.version $stage }
    catch { $State.failedVersion=$Manifest.version; Write-JsonFile (Join-Path $StateRoot 'state.json') $State; throw }
    $journalPath = Join-Path $StateRoot 'transaction.json'
    try {
        if ($old -and -not (Request-ModHandoff $HostEnv $old)) { return 'Island is busy; check again after finishing the timer/dialog.' }
        $journal = [pscustomobject]@{ schema=1; stage=$stageId; version=$Manifest.version; old=$old; oldState=$State.PSObject.Copy() }
        if ($old) { Copy-Item -LiteralPath $HostEnv.Source -Destination (Join-Path $stage 'previous.wh.cpp') }
        Write-JsonFile $journalPath $journal
    } catch { Set-UpdateRequest $HostEnv 0; throw }
    try {
        if ($old) { Set-ModDisabled $HostEnv 1; Wait-ModStopped }
        foreach ($dir in @((Join-Path $HostEnv.Data 'Engine\Mods\64'),(Join-Path $HostEnv.Data 'Engine\Mods\32'),(Split-Path $HostEnv.Source -Parent))) {
            if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        }
        $library = "$script:ModId`_$($Manifest.version)_$($stageId.Substring(0,12)).dll"
        foreach ($bits in @('64','32')) { Copy-Item -LiteralPath (Join-Path $stage "$bits.dll") -Destination (Get-ManagedDllPath $HostEnv $bits $library) }
        Copy-Item -LiteralPath (Join-Path $stage 'source.wh.cpp') -Destination $HostEnv.Source -Force
        Set-NewMod $HostEnv $Manifest $library
        Set-UpdateRequest $HostEnv 0
        New-ItemProperty -LiteralPath $HostEnv.Storage -Name NativeGlassReady -Value 0 -PropertyType DWord -Force | Out-Null
        $disabled = if ($old) { $old.Disabled } else { 0 }
        Set-ModDisabled $HostEnv $disabled
        if ($disabled -eq 0) { Wait-ModHealthy $HostEnv $Manifest.version }
        $State.version=$Manifest.version
        $State.sourceSha256=$Manifest.sourceSha256
        $State.failedVersion=''
        Set-UpdateAvailable $HostEnv 0
        Write-JsonFile (Join-Path $StateRoot 'state.json') $State
        Write-JsonFile (Join-Path $StateRoot 'previous.json') $journal
        Remove-Item -LiteralPath $journalPath -Force
        'Updated to ' + $Manifest.version
    } catch {
        $failure = $_
        Restore-Transaction $HostEnv $StateRoot $journal
        $oldState = $journal.oldState
        $oldState.failedVersion=$Manifest.version
        Write-JsonFile (Join-Path $StateRoot 'state.json') $oldState
        Remove-Item -LiteralPath $journalPath -Force
        throw $failure
    } finally { Set-UpdateRequest $HostEnv 0 }
}

function Get-UpdaterState([string]$Root) {
    $path = Join-Path $Root 'state.json'
    if (Test-Path -LiteralPath $path) { return Read-JsonFile $path }
    [pscustomobject]@{ version='0.0.0'; sourceSha256=''; failedVersion=''; lastAutoCheck=0 }
}

function Clear-OldStages([string]$Root, [string[]]$Keep) {
    $prefix=[IO.Path]::GetFullPath($Root)+[IO.Path]::DirectorySeparatorChar
    foreach ($dir in Get-ChildItem -LiteralPath $Root -Directory) {
        if ($dir.Name -cnotmatch '^[a-f0-9]{32}$' -or $dir.Name -in $Keep) { continue }
        $target=[IO.Path]::GetFullPath($dir.FullName)
        if (-not $target.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe stage cleanup path.' }
        $contents=@($dir)+@(Get-ChildItem -LiteralPath $target -Recurse -Force)
        if (@($contents | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'Stage contains a link; cleanup refused.' }
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

function Invoke-IslandUpdate($HostEnv, [string]$StateRoot, $Config, [string]$PublicKey, [string]$Mode) {
    Assert-Repository $Config.repository
    $state = Get-UpdaterState $StateRoot
    $journalPath = Join-Path $StateRoot 'transaction.json'
    if ($Mode -eq 'Auto') {
        if (Test-Path -LiteralPath $journalPath) { return 'Interrupted update pending; use the manual update command to recover.' }
        $now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        $last = 0L
        if ($state.PSObject.Properties['lastAutoCheck'] -and
            [long]::TryParse([string]$state.lastAutoCheck, [ref]$last) -and
            $now -ge $last -and $now - $last -lt 10800) { return 'Already checked within three hours.' }
        $state | Add-Member -NotePropertyName lastAutoCheck -NotePropertyValue $now -Force
        Write-JsonFile (Join-Path $StateRoot 'state.json') $state
    } else {
        if (Test-Path -LiteralPath $journalPath) {
            $journal = Read-JsonFile $journalPath
            Restore-Transaction $HostEnv $StateRoot $journal
            $state = $journal.oldState
            $state.failedVersion=$journal.version
            Write-JsonFile (Join-Path $StateRoot 'state.json') $state
            Remove-Item -LiteralPath $journalPath -Force
        }
        $keep=@()
        $previousPath=Join-Path $StateRoot 'previous.json'
        if (Test-Path -LiteralPath $previousPath) { $keep+= (Read-JsonFile $previousPath).stage }
        $latestStage=@(Get-ChildItem -LiteralPath $StateRoot -Directory | Where-Object { $_.Name -cmatch '^[a-f0-9]{32}$' } | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
        if ($latestStage.Count) { $keep+=$latestStage[0].Name }
        Clear-OldStages $StateRoot $keep
        Set-UpdateRequest $HostEnv 0
    }
    $base = 'https://github.com/' + $Config.repository + '/releases/'
    $bytes = Get-RemoteBytes ($base + 'latest/download/update.json') 262144
    $signature = Get-RemoteBytes ($base + 'latest/download/update.sig') 1024
    $manifest = Get-VerifiedManifest $bytes $signature $PublicKey
    if ((Get-ReleaseVersion $manifest.version) -le (Get-ReleaseVersion $state.version)) {
        Set-UpdateAvailable $HostEnv 0
        return 'Already current.'
    }
    Set-UpdateAvailable $HostEnv 1
    if ($Mode -eq 'Auto') { return 'Update available: ' + $manifest.version }
    if ($state.failedVersion -ceq $manifest.version) { throw 'This version previously failed. Waiting for a newer release.' }
    $source = Get-RemoteBytes ($base + 'download/' + $manifest.tag + '/' + $manifest.sourceAsset) 8388608
    $download = Join-Path $StateRoot 'download.wh.cpp'
    [IO.File]::WriteAllBytes($download, $source)
    Assert-Source $download $manifest
    Invoke-IslandInstall $HostEnv $StateRoot $manifest $download $state
}

function Assert-Administrator {
    $principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator permission is required for setup/update.' }
}

function Protect-UpdaterDirectory([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        if ((Get-Item -LiteralPath $Path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Updater directory must not be a link.' }
    } else { New-Item -ItemType Directory -Path $Path | Out-Null }
    $acl = New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true,$false)
    $admins = New-Object Security.Principal.SecurityIdentifier('S-1-5-32-544')
    $acl.SetOwner($admins)
    foreach ($sid in @('S-1-5-32-544','S-1-5-18')) {
        $identity = New-Object Security.Principal.SecurityIdentifier($sid)
        $rule = New-Object Security.AccessControl.FileSystemAccessRule($identity,'FullControl','ContainerInherit,ObjectInherit','None','Allow')
        $acl.AddAccessRule($rule)
    }
    $users = New-Object Security.Principal.SecurityIdentifier('S-1-5-32-545')
    $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule($users,'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow')))
    Set-Acl -LiteralPath $Path -AclObject $acl
    $entries=@(Get-ChildItem -LiteralPath $Path -Force -Recurse)
    foreach ($entry in $entries) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Updater contents must not contain links.' }
    }
    foreach ($entry in $entries) {
        if ($entry.PSIsContainer) { $childAcl=New-Object Security.AccessControl.DirectorySecurity }
        else { $childAcl=New-Object Security.AccessControl.FileSecurity }
        $childAcl.SetOwner($admins)
        $childAcl.SetAccessRuleProtection($true,$false)
        foreach ($pair in @(@('S-1-5-32-544','FullControl'),@('S-1-5-18','FullControl'),@('S-1-5-32-545','ReadAndExecute'))) {
            $identity=New-Object Security.Principal.SecurityIdentifier($pair[0])
            if ($entry.PSIsContainer) { $rule=New-Object Security.AccessControl.FileSystemAccessRule($identity,$pair[1],'ContainerInherit,ObjectInherit','None','Allow') }
            else { $rule=New-Object Security.AccessControl.FileSystemAccessRule($identity,$pair[1],'Allow') }
            $childAcl.AddAccessRule($rule)
        }
        Set-Acl -LiteralPath $entry.FullName -AclObject $childAcl
    }
}
