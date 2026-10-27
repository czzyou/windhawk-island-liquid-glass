function Assert-PublishCheckout([string]$SourceRoot) {
    $commit = & git -C $SourceRoot rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $commit -cnotmatch '^[a-f0-9]{40}$') { throw 'Cannot determine the release commit.' }
    $branch = & git -C $SourceRoot branch --show-current
    if ($LASTEXITCODE -ne 0 -or $branch -cne 'main') { throw 'Publish releases from main.' }
    $changes = & git -C $SourceRoot status --porcelain
    if ($LASTEXITCODE -ne 0 -or $changes) { throw 'Commit and push all release inputs before preparing signed assets.' }
    $commit
}
