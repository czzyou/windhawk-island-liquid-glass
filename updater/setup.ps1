[CmdletBinding()]
param()
. (Join-Path $PSScriptRoot 'core.ps1')
Assert-Administrator
$setupMutex=New-Object Threading.Mutex($false,'Global\Windhawk-Island-Updater')
$setupLocked=$false
try {
try { $setupLocked=$setupMutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $setupLocked=$true }
if (-not $setupLocked) { throw 'An update is running; wait for it to finish before installing.' }
$hostEnv = Get-WindhawkEnvironment
$config = Read-JsonFile (Join-Path $PSScriptRoot 'config.json')
Assert-Repository $config.repository
$installRoot = Join-Path $env:ProgramFiles 'Windhawk-Island-Updater'
$stateRoot = Join-Path $env:ProgramData 'Windhawk-Island-Updater'
$account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$baseName = 'Windhawk-Island-' + $sid
if ((Test-Path -LiteralPath $hostEnv.Key) -and -not (Test-Path -LiteralPath (Join-Path $stateRoot 'state.json'))) {
    throw 'An unmanaged liquid-glass mod already exists. Preserve it and migrate explicitly before running this setup.'
}
$others=@()
if (Test-Path 'HKLM:\SOFTWARE\Windhawk\Engine\Mods') { $others = @(Get-ChildItem 'HKLM:\SOFTWARE\Windhawk\Engine\Mods' | Where-Object {
    $_.PSChildName -ne $script:ModId -and $_.PSChildName -like '*dynamic-island*' -and $_.GetValue('Disabled',0) -eq 0
}) }
if ($others.Count) { throw 'Another Dynamic Island mod is enabled. Disable it in Windhawk before installing; setup will not change it.' }
Protect-UpdaterDirectory $installRoot
Protect-UpdaterDirectory $stateRoot
$existingConfig = Join-Path $installRoot 'config.json'
if (Test-Path -LiteralPath $existingConfig) {
    $existing = Read-JsonFile $existingConfig
    if ($existing.repository -cne $config.repository -or $existing.userSid -cne $sid -or
        [IO.File]::ReadAllText((Join-Path $installRoot 'public-key.xml')) -cne [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'public-key.xml'))) {
        throw 'Existing updater belongs to a different repository, signing key or user; refusing to replace it.'
    }
}
foreach ($file in @('core.ps1','update.ps1','check.ps1','uninstall.ps1','public-key.xml')) {
    $from = Join-Path $PSScriptRoot $file
    $to = Join-Path $installRoot $file
    if ([IO.Path]::GetFullPath($from) -cne [IO.Path]::GetFullPath($to)) { Copy-Item -LiteralPath $from -Destination $to -Force }
}
Write-JsonFile (Join-Path $installRoot 'config.json') ([ordered]@{ repository=$config.repository; userSid=$sid })
Protect-UpdaterDirectory $installRoot
$powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$principal = New-ScheduledTaskPrincipal -UserId $account -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 15) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Hours 3)
foreach ($mode in @('Auto','Manual','Rollback')) {
    $action = New-ScheduledTaskAction -Execute $powershell -Argument ('-NoProfile -NonInteractive -WindowStyle Hidden -File "' + (Join-Path $installRoot 'update.ps1') + '" -Mode ' + $mode)
    $taskArgs = @{ TaskName=($baseName+'-'+$mode); Action=$action; Principal=$principal; Settings=$settings; Force=$true }
    if ($mode -eq 'Auto') { $taskArgs.Trigger=$trigger }
    Register-ScheduledTask @taskArgs | Out-Null
    $service=New-Object -ComObject 'Schedule.Service'
    $service.Connect()
    $task=$service.GetFolder('\').GetTask($baseName+'-'+$mode)
    $task.SetSecurityDescriptor(('D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGX;;;'+$sid+')'),0)
}
$shell = New-Object -ComObject WScript.Shell
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Windhawk Island'
New-Item -ItemType Directory -Path $menu -Force | Out-Null
$legacyShortcut=Join-Path $menu 'Check for updates.lnk'
if (Test-Path -LiteralPath $legacyShortcut) { Remove-Item -LiteralPath $legacyShortcut -Force }
$link = $shell.CreateShortcut((Join-Path $menu 'Roll back previous version.lnk'))
$link.TargetPath=$powershell
$link.Arguments='-NoProfile -WindowStyle Hidden -File "'+(Join-Path $installRoot 'check.ps1')+'" -Mode Rollback'
$link.WindowStyle=7
$link.Save()
$remove = $shell.CreateShortcut((Join-Path $menu 'Uninstall updater.lnk'))
$remove.TargetPath=$powershell
$remove.Arguments='-NoProfile -File "'+(Join-Path $installRoot 'uninstall.ps1')+'"'
$remove.Save()
} finally { if ($setupLocked) { $setupMutex.ReleaseMutex() }; $setupMutex.Dispose() }
& $powershell -NoProfile -File (Join-Path $installRoot 'update.ps1') -Mode Install
if ($LASTEXITCODE -ne 0) { throw 'Initial installation failed. Check ProgramData\Windhawk-Island-Updater\update.log. Registered shortcuts can retry.' }
Write-Host 'Installed. Silent update checks run every three hours; use the island menu to install available updates.'
