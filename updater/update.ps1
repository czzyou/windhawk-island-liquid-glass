[CmdletBinding()]
param([ValidateSet('Auto','Manual','Install','Rollback')][string]$Mode='Manual')
. (Join-Path $PSScriptRoot 'core.ps1')
$stateRoot = Join-Path $env:ProgramData 'Windhawk-Island-Updater'
$mutex = New-Object Threading.Mutex($false, 'Global\Windhawk-Island-Updater')
$locked = $false
$message = ''
$code = 0
try {
    Assert-Administrator
    try { $locked = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $locked=$true }
    if (-not $locked) { $message='Another update/check is already running.' }
    else {
        $hostEnv = Get-WindhawkEnvironment
        $config = Read-JsonFile (Join-Path $PSScriptRoot 'config.json')
        if ($config.userSid -cne [Security.Principal.WindowsIdentity]::GetCurrent().User.Value) { throw 'Run the updater as the user who installed it.' }
        $publicKey = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'public-key.xml'))
        if ($Mode -eq 'Rollback') {
            $pending = Join-Path $stateRoot 'transaction.json'
            if (Test-Path -LiteralPath $pending) { throw 'An interrupted transaction exists. Run Check for updates to recover it first.' }
            $previous = Read-JsonFile (Join-Path $stateRoot 'previous.json')
            if (-not $previous.old) { throw 'There is no previous installed version.' }
            $current = Get-ModSnapshot $hostEnv
            if (-not (Request-ModHandoff $hostEnv $current)) { throw 'Island is busy; finish the timer/dialog first.' }
            Write-JsonFile $pending $previous
            Restore-Transaction $hostEnv $stateRoot $previous
            $state=$previous.oldState
            $state.failedVersion=$previous.version
            Write-JsonFile (Join-Path $stateRoot 'state.json') $state
            Remove-Item -LiteralPath $pending -Force
            Remove-Item -LiteralPath (Join-Path $stateRoot 'previous.json') -Force
            $message='Rolled back to ' + $previous.old.Version
        } else { $message = Invoke-IslandUpdate $hostEnv $stateRoot $config $publicKey $Mode }
    }
} catch { $message=$_.Exception.Message; $code=1 }
finally {
    if ($locked) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
if (Test-Path -LiteralPath $stateRoot) {
    $log = Join-Path $stateRoot 'update.log'
    if ((Test-Path -LiteralPath $log) -and (Get-Item -LiteralPath $log).Length -gt 1048576) { Move-Item -LiteralPath $log -Destination ($log+'.old') -Force }
    Add-Content -LiteralPath $log -Value ((Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' [' + $Mode + '] ' + $message) -Encoding utf8
}
if ($Mode -in @('Manual','Rollback','Install')) {
    Add-Type -AssemblyName System.Windows.Forms
    [void][Windows.Forms.MessageBox]::Show($message, 'Island updater')
}
exit $code
