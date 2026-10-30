# SPDX-License-Identifier: GPL-3.0-or-later
# Isolated NTSC smoke harness. Requires the player's own extracted data.
param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$InstalledData,
    [Parameter(Mandatory)][string]$BuildRoot,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateSet('points','points-kerwan','movie-start','movie-chord','flight')]
    [string]$Scenario='points',
    [int]$Frames=6000
)
$ErrorActionPreference='Stop'
$exe=(Resolve-Path -LiteralPath $Executable).Path
$installed=(Resolve-Path -LiteralPath $InstalledData).Path
$build=(Resolve-Path -LiteralPath $BuildRoot).Path
$qa=[IO.Path]::GetFullPath($OutputRoot)
if(Test-Path -LiteralPath $qa){throw 'OutputRoot must be new; previous evidence is preserved.'}
foreach($part in @('data','cards','local','shots','plugins')){
    New-Item -ItemType Directory -Path (Join-Path $qa $part) -Force | Out-Null
}
New-Item -ItemType Junction -Path "$qa/data/iso_data" -Target "$installed/iso_data" | Out-Null
$mods=@()
$components=@(@('skill-points-ra','skill_points_ra'),@('cinematic-journeys','cinematic_journeys'))
if($Scenario -like 'points*'){$components+=,@('skill-points-qa','skill_points_qa')}
if($Scenario -eq 'flight'){$components+=,@('cinematic-qa','cinematic_qa')}
foreach($component in $components){
    $id=$component[0]; $target=$component[1]
    Copy-Item -LiteralPath "$build/package/$target" -Destination "$qa/plugins/$id" -Recurse
    $config=if($id -eq 'skill-points-ra'){
        @{profile='compatibility-010';notifications=$true;kerwan_time_trial=($Scenario -eq 'points-kerwan')}
    }elseif($id -eq 'cinematic-journeys'){
        @{protect_scenes=$true;minimum_flight_seconds=5.0}
    }else{@{}}
    $mods+=@{id=$id;enabled=$true;settings=$config}
}
@{format=1;mods=$mods}|ConvertTo-Json -Depth 6|Set-Content "$qa/plugins/plugins.json" -Encoding utf8
# A new settings file and empty cards directory; no personal saves are opened.
@('window_size = 960x600','volume = 0','enhanced_rendering = 0','skip_cutscenes = 1') |
    Set-Content "$qa/settings.ini" -Encoding utf8
$start=[Diagnostics.ProcessStartInfo]::new()
$start.FileName=$exe; $start.WorkingDirectory=$qa
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
$level=if($Scenario -eq 'flight'){2}else{1}
foreach($arg in @('--data',"$qa/data",'--levels',"$installed/port/ntsc",'--plugins',"$qa/plugins/plugins.json",'--cards',"$qa/cards",'--settings',"$qa/settings.ini",'--window','--no-card','--level',[string]$level,'--frames',[string]$Frames)){
    $start.ArgumentList.Add($arg)
}
if($Scenario -notlike 'movie-*'){$start.ArgumentList.Add('--skip-movies')}
$press='100:4000:5,k30:8:5'
if($Scenario -like 'points*'){
    $presses=@($press,'c180:8:5')
    for($i=0;$i -lt 6;$i++){$presses+=('c'+(280+$i*30)+':40:3')}
    $presses+=@('c540:4000:3','c660:4000:3')
    for($i=0;$i -lt 35;$i++){$presses+=('c'+(730+$i*15)+':40:3')}
    $presses+=@('c1360:1000:3','c1440:1000:3','c1520:8:3','c1600:8:3')
    $press=$presses -join ','
    $start.Environment['SP_QA_PAUSE']='1'
    $start.Environment['SP_QA_GRANT']='1'
}
if($Scenario -like 'movie-*'){
    $presses=@('100:4000:5')
    $mask=if($Scenario -eq 'movie-chord'){'f08'}else{'8'}
    for($i=150;$i -le 2790;$i+=30){$presses+=('{0}:{1}:5' -f $i,$mask)}
    $press=$presses -join ','
}
$start.Environment['OPENRAC_PRESS']=$press
$start.Environment['OPENRAC_CONTROL_FRAMES']=if($Scenario -like 'points*'){'1800'}elseif($Scenario -eq 'flight'){'1800'}else{'120'}
$start.Environment['LOCALAPPDATA']="$qa/local"
$start.Environment['OPENRAC_DIRECT']='1'
$start.Environment['OPENRAC_GL_API']='vulkan'
$start.Environment['OPENRAC_LOG']='verbose'
$start.Environment['CINEMATIC_JOURNEYS_TRACE']='1'
$start.Environment['OPENRAC_SHOT']='90:shots/frame'
$start.Environment['OPENRAC_TRACE']="$qa/trace.jsonl"
$process=[Diagnostics.Process]::Start($start)
$record=@{pid=$process.Id;started=$process.StartTime.ToUniversalTime().ToString('O');scenario=$Scenario;root=$qa;executableSha256=(Get-FileHash $exe).Hash.ToLowerInvariant()}
$record | ConvertTo-Json | Set-Content "$qa/run.json" -Encoding utf8
$record | ConvertTo-Json
