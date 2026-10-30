param([string]$RunName='menu',[int]$Length=1100,[switch]$WithWeapons,[switch]$Grant,[int]$Level=1,[switch]$Quick,[switch]$GoodiesRoute,[switch]$PauseRoute,[switch]$KerwanTimeTrial)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$qaRoot=Join-Path $projectRoot ('qa-'+$RunName)
$installed=Join-Path $env:USERPROFILE 'OpenRAC/active/rac1/data'
$exe=Join-Path $env:LOCALAPPDATA 'OpenRAC Launcher/openrac/port/build/release/openrac-rac1-ntsc.exe'
foreach($dir in @($qaRoot,"$qaRoot/data","$qaRoot/cards","$qaRoot/local","$qaRoot/shots","$qaRoot/plugins")){New-Item -ItemType Directory -Force -Path $dir|Out-Null}
if(-not(Test-Path -LiteralPath "$qaRoot/data/iso_data")){New-Item -ItemType Junction -Path "$qaRoot/data/iso_data" -Target "$installed/iso_data"|Out-Null}
$mods=@()
foreach($pair in @(@('skill-points-ra','skill_points_ra'),@('skill-points-qa','skill_points_qa'))) {
    $id=$pair[0];$target=$pair[1]
    $dest="$qaRoot/plugins/$id";New-Item -ItemType Directory -Force $dest|Out-Null
    Copy-Item -LiteralPath "$projectRoot/build/package/$target/lib" -Destination $dest -Recurse -Force
    Copy-Item -LiteralPath "$projectRoot/build/package/$target/openrac-mod.json" -Destination $dest -Force
    $settingsForMod=if($id -eq 'skill-points-ra'){@{profile='qa-isolated';notifications=$true;kerwan_time_trial=[bool]$KerwanTimeTrial}}else{@{}}
    $mods+=@{id=$id;enabled=$true;settings=$settingsForMod}
}
if($WithWeapons) {
    $source=Join-Path (Split-Path -Parent $projectRoot) 'rac1-mods/build-xp/package/weapon_xp'
    $dest="$qaRoot/plugins/weapon-xp";New-Item -ItemType Directory -Force $dest|Out-Null
    Copy-Item -LiteralPath "$source/lib" -Destination $dest -Recurse -Force
    Copy-Item -LiteralPath "$source/openrac-mod.json" -Destination $dest -Force
    $mods+=@{id='weapon-xp';enabled=$true;settings=@{profile='qa-isolated';xp_rate=1.0;hud=$true;reduced_motion=$true}}
}
$mods=@($mods | Sort-Object {if($_.id -eq 'weapon-xp'){0}elseif($_.id -eq 'skill-points-ra'){1}else{2}})
@{format=1;mods=$mods}|ConvertTo-Json -Depth 8|Set-Content "$qaRoot/plugins/plugins.json" -Encoding utf8
$settings=Get-Content (Join-Path $env:LOCALAPPDATA 'openrac/settings/rac1-ntsc.ini') -Raw
$settings=$settings -replace '(?m)^window_size = .*$','window_size = 1280x800'
$settings=$settings -replace '(?m)^volume = .*$','volume = 0'
$settings=$settings -replace '(?m)^enhanced_rendering = .*$','enhanced_rendering = 0'
$settings|Set-Content "$qaRoot/settings.ini" -Encoding utf8
$start=[System.Diagnostics.ProcessStartInfo]::new()
$start.FileName=$exe;$start.WorkingDirectory=$qaRoot;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
$frameLimit=if($Quick -and $Level -eq 0){2400}elseif($Quick){900}else{6000}
foreach($arg in @('--data',"$qaRoot/data",'--levels',"$installed/port/ntsc",'--plugins',"$qaRoot/plugins/plugins.json",'--cards',"$qaRoot/cards",'--settings',"$qaRoot/settings.ini",'--no-card','--skip-movies','--level',[string]$Level,'--window','--frames',[string]$frameLimit)){$start.ArgumentList.Add($arg)}
$start.Environment['LOCALAPPDATA']="$qaRoot/local"
$start.Environment['OPENRAC_DIRECT']='1'
$start.Environment['OPENRAC_GL_API']='vulkan'
if($Grant){$start.Environment['SP_QA_GRANT']='1'}
if($GoodiesRoute){$start.Environment['SP_QA_GOODIES']='1'}
if($PauseRoute){$start.Environment['SP_QA_PAUSE']='1'}
$press=@('100:4000:5','k30:8:5','c180:8:5','c300:4000:3')
for($i=0;$i -lt 35;$i++){$press+=('c'+(345+$i*15)+':40:3')}
if($PauseRoute){
    $press=@('100:4000:5','k30:8:5','c180:8:5')
    for($i=0;$i -lt 6;$i++){$press+=('c'+(280+$i*30)+':40:3')}
    $press+='c540:4000:3'
    $press+='c660:4000:3'
    for($i=0;$i -lt 35;$i++){$press+=('c'+(730+$i*15)+':40:3')}
    $press+=@('c1360:1000:3','c1440:1000:3','c1520:8:3','c1600:8:3')
}
$start.Environment['OPENRAC_PRESS']=$press -join ','
$start.Environment['OPENRAC_CONTROL_FRAMES']=[string]$Length
if(-not $Quick){$start.Environment['OPENRAC_SHOT']='90:shots/frame'}
$p=[System.Diagnostics.Process]::Start($start)
@{pid=$p.Id;started=$p.StartTime.ToUniversalTime().ToString('O');root=$qaRoot}|ConvertTo-Json|Set-Content "$qaRoot/run.json"
Get-Content "$qaRoot/run.json"
