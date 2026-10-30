$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$results=@()
foreach($level in 0..18) {
    $run=& "$PSScriptRoot/run_native.ps1" -RunName ('level-'+$level.ToString('00')) -Level $level -Length 180 -Quick | ConvertFrom-Json
    $process=Get-Process -Id $run.pid -ErrorAction SilentlyContinue
    if($process){$process.WaitForExit()}
    $logPath=Join-Path $run.root 'data/openrac-rac1-ntsc.log'
    $log=Get-Content -LiteralPath $logPath -Raw
    $errors=@($log -split "`n" | Where-Object {$_ -match '\[error\].*\[mod skill-points-ra\]'})
    $ready=$log -match ('level='+$level+' ui_ready=1')
    $results+=@{level=$level;loaded=($log -match 'mod skill-points-ra 0.1.1 loaded');uiReady=$ready;errors=$errors;log=$logPath}
    $results|ConvertTo-Json -Depth 6|Set-Content "$projectRoot/native-level-smokes.json" -Encoding utf8
    [pscustomobject]@{level=$level;uiReady=$ready;errors=$errors.Count}|ConvertTo-Json -Compress
}
