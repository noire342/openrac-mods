param([string]$Name='protected', [double]$Minimum=5, [bool]$Protect=$true,
      [string]$Press='100:4000:5,k30:8:5', [int]$Level=2, [int]$Length=120,
      [int]$Frames=6000, [switch]$Movies, [switch]$TravelFixture)
$ErrorActionPreference = 'Stop'
$modRoot = Split-Path -Parent $PSScriptRoot
$qaRoot = Join-Path $modRoot ('qa-' + $Name)
if (Test-Path -LiteralPath $qaRoot) { throw 'Use a fresh QA name to preserve the previous evidence.' }
$installedData = Join-Path $env:USERPROFILE 'OpenRAC\active\rac1\data'
$dataRoot = Join-Path $qaRoot 'data'
$pluginRoot = Join-Path $qaRoot 'plugins'
New-Item -ItemType Directory -Path $qaRoot,$dataRoot,$pluginRoot,(Join-Path $qaRoot 'cards'),(Join-Path $qaRoot 'local'),(Join-Path $qaRoot 'shots') | Out-Null
New-Item -ItemType Junction -Path (Join-Path $dataRoot 'iso_data') -Target (Join-Path $installedData 'iso_data') | Out-Null
$modPath = Join-Path $pluginRoot 'cinematic-journeys'
Copy-Item -LiteralPath (Join-Path $modRoot 'build\package\cinematic_journeys') -Destination $modPath -Recurse
$plugins = @{format=1; mods=@(@{id='cinematic-journeys';enabled=$true;settings=@{protect_scenes=$Protect;minimum_flight_seconds=$Minimum}})}
if ($TravelFixture) {
    Copy-Item -LiteralPath (Join-Path $modRoot 'build\package\cinematic_qa') -Destination (Join-Path $pluginRoot 'cinematic-qa') -Recurse
    $plugins.mods += @{id='cinematic-qa';enabled=$true;settings=@{}}
}
$plugins |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $pluginRoot 'plugins.json') -Encoding utf8
$settings = Get-Content -LiteralPath (Join-Path $env:LOCALAPPDATA 'openrac\settings\rac1-ntsc.ini') -Raw
$settings = $settings -replace '(?m)^window_size = .*$', 'window_size = 960x600'
$settings = $settings -replace '(?m)^volume = .*$', 'volume = 0'
$settings = $settings -replace '(?m)^enhanced_rendering = .*$', 'enhanced_rendering = 0'
$settings = $settings -replace '(?m)^skip_cutscenes = .*$', 'skip_cutscenes = 1'
$settings | Set-Content -LiteralPath (Join-Path $qaRoot 'settings.ini') -Encoding utf8
$start = [System.Diagnostics.ProcessStartInfo]::new()
$start.FileName = Join-Path $env:LOCALAPPDATA 'OpenRAC Launcher\openrac\port\build\release\openrac-rac1-ntsc.exe'
$start.WorkingDirectory = $qaRoot
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
foreach ($arg in @('--data',$dataRoot,'--levels',(Join-Path $installedData 'port\ntsc'),
    '--plugins',(Join-Path $pluginRoot 'plugins.json'),'--cards',(Join-Path $qaRoot 'cards'),
    '--settings',(Join-Path $qaRoot 'settings.ini'),'--window','--frames',[string]$Frames,
    '--no-card','--level',[string]$Level)) { $start.ArgumentList.Add($arg) }
if (-not $Movies) { $start.ArgumentList.Add('--skip-movies') }
$start.Environment['OPENRAC_DIRECT'] = '1'
$start.Environment['OPENRAC_GL_API'] = 'vulkan'
$start.Environment['LOCALAPPDATA'] = Join-Path $qaRoot 'local'
$start.Environment['OPENRAC_LOG'] = 'verbose'
$start.Environment['CINEMATIC_JOURNEYS_TRACE'] = '1'
$start.Environment['OPENRAC_PRESS'] = $Press
$start.Environment['OPENRAC_CONTROL_FRAMES'] = [string]$Length
$start.Environment['OPENRAC_SHOT'] = '120:shots/frame'
$start.Environment['OPENRAC_TRACE'] = Join-Path $qaRoot 'trace.jsonl'
$gameProcess = [System.Diagnostics.Process]::Start($start)
@{pid=$gameProcess.Id; started=$gameProcess.StartTime.ToUniversalTime().ToString('O');
  root=$qaRoot; minimum=$Minimum; protect=$Protect;press=$Press;movies=[bool]$Movies; travelFixture=[bool]$TravelFixture} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $qaRoot 'run.json') -Encoding utf8
Get-Content -LiteralPath (Join-Path $qaRoot 'run.json')
