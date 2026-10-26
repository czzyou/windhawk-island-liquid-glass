function Assert-ActionsBuild([string]$Repository, [string]$SourceRoot) {
    $commit = & git -C $SourceRoot rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $commit -cnotmatch '^[a-f0-9]{40}$') { throw 'Cannot determine the release commit.' }
    $changes = & git -C $SourceRoot status --porcelain
    if ($LASTEXITCODE -ne 0 -or $changes) { throw 'Commit and push all release inputs before preparing signed assets.' }
    foreach ($workflow in @('source-check.yml', 'build.yml')) {
        $runs = & gh run list --repo $Repository --workflow $workflow --commit $commit --branch main --status success --event push --limit 1 --json headSha,headBranch,event,status,conclusion,url
        if ($LASTEXITCODE -ne 0) { throw "Cannot query Actions: $workflow" }
        $runs = @($runs | ConvertFrom-Json)
        if ($runs.Count -ne 1 -or $runs[0].headSha -cne $commit -or $runs[0].headBranch -cne 'main' -or
            $runs[0].event -cne 'push' -or $runs[0].status -cne 'completed' -or $runs[0].conclusion -cne 'success') {
            throw "No successful $workflow run for $commit on main. Push the commit and wait for Actions."
        }
        Write-Host ('Verified Actions: ' + $runs[0].url)
    }
}
