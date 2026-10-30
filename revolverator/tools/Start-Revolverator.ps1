# SPDX-License-Identifier: MIT
# Copyright (c) 2026 noire342
param(
    [string]$Runtime = '',
    [string]$InstalledData = '',
    [string]$Settings = '',
    [string]$SessionRoot = '',
    [ValidateRange(-1,18)][int]$Level = -1,
    [switch]$PrepareOnly
)

# Private, reversible test delivery. PowerShell 5.1+; no Python installation is
# needed on the player's PC. Existing game files, plugins, settings and saves
# are read as inputs, never patched or overwritten.
$ErrorActionPreference = 'Stop'
function Write-RevolveratorUtf8([string]$Path,[string]$Text) {
    [IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false))
}
$revBundle = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not $Runtime) { $Runtime = Join-Path $env:LOCALAPPDATA 'OpenRAC Launcher\openrac\port\build\release\openrac-rac1-ntsc.exe' }
if (-not $InstalledData) { $InstalledData = Join-Path $env:USERPROFILE 'OpenRAC\active\rac1\data' }
if (-not $Settings) { $Settings = Join-Path $env:LOCALAPPDATA 'openrac\settings\rac1-ntsc.ini' }
if (-not $SessionRoot) { $SessionRoot = Join-Path $revBundle ('.sessions\' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')) }
$Runtime = [IO.Path]::GetFullPath($Runtime)
$InstalledData = [IO.Path]::GetFullPath($InstalledData)
$SessionRoot = [IO.Path]::GetFullPath($SessionRoot)
$revBaseLevels = Join-Path $InstalledData 'port\ntsc'
$revDeliveryPath = Join-Path $revBundle 'delivery.json'

foreach ($revRequired in @($Runtime,$revDeliveryPath,(Join-Path $InstalledData 'iso_data'),$revBaseLevels)) {
    if (-not (Test-Path -LiteralPath $revRequired)) { throw "Required installed runtime input is missing: $revRequired" }
}
if (Test-Path -LiteralPath $SessionRoot) { throw 'The session directory already exists. Choose a new -SessionRoot.' }
$revSourcePrefix = $InstalledData.TrimEnd('\') + '\'
if ($SessionRoot.Equals($InstalledData,[StringComparison]::OrdinalIgnoreCase) -or
    $SessionRoot.StartsWith($revSourcePrefix,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'The private session must be outside the installed data directory.'
}
$revDelivery = Get-Content -LiteralPath $revDeliveryPath -Raw | ConvertFrom-Json
if ($revDelivery.format -ne 1 -or $revDelivery.kind -ne 'revolverator-private-test-bundle') { throw 'Unsupported private delivery format.' }
if ($revDelivery.game -ne 'rac1' -or $revDelivery.disc -ne 'ntsc') { throw 'This delivery supports R&C1 NTSC-U only.' }
foreach ($revEntry in $revDelivery.files.PSObject.Properties) {
    $revRelative = $revEntry.Name
    if ([IO.Path]::IsPathRooted($revRelative) -or $revRelative.Split('/') -contains '..') { throw 'Unsafe delivery path.' }
    $revFile = Join-Path $revBundle ($revRelative.Replace('/','\'))
    if (-not (Test-Path -LiteralPath $revFile -PathType Leaf)) { throw "Delivery file missing: $revRelative" }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $revFile).Hash.ToLowerInvariant() -ne $revEntry.Value.sha256) {
        throw "Delivery checksum failed: $revRelative"
    }
}
$revPlugin = Join-Path $revBundle 'plugin\revolverator'
$revPluginManifest = Get-Content -LiteralPath (Join-Path $revPlugin 'openrac-mod.json') -Raw | ConvertFrom-Json
if ($revPluginManifest.format -ne 2 -or $revPluginManifest.id -ne 'revolverator') { throw 'The native mod manifest is invalid.' }
if (-not $revPluginManifest.plugin.'windows-x86_64') { throw 'This bundle has no Windows x86-64 plugin.' }
$revArt = Join-Path $revBundle 'ammunition'
$revArtInfo = Get-Content -LiteralPath (Join-Path $revArt 'asset.json') -Raw | ConvertFrom-Json
if ($revArtInfo.native_pickup_class -ne 225 -or $revArtInfo.world_class -ne 32000 -or $revArtInfo.vendor_class -ne 2016) {
    throw 'The ammunition asset identities do not match this mod.'
}

New-Item -ItemType Directory -Path $SessionRoot | Out-Null
$revLevels = Join-Path $SessionRoot 'levels'
$revData = Join-Path $SessionRoot 'data'
$revPlugins = Join-Path $SessionRoot 'plugins'
$revCards = Join-Path $SessionRoot 'cards'
$revLocal = Join-Path $SessionRoot 'local'
New-Item -ItemType Directory -Path $revLevels,$revData,$revPlugins,$revCards,$revLocal | Out-Null
New-Item -ItemType Junction -Path (Join-Path $revData 'iso_data') -Target (Join-Path $InstalledData 'iso_data') | Out-Null
$revSourceHashes = @{}
$revAdded = 0
$revManifests = 0
foreach ($revSource in (Get-ChildItem -LiteralPath $revBaseLevels -Directory | Sort-Object Name)) {
    $revManifestPath = Join-Path $revSource.FullName 'manifest.json'
    if (-not (Test-Path -LiteralPath $revManifestPath -PathType Leaf)) { continue }
    if ($revSource.Name -notmatch '^level_[0-9]{2}$') { continue }
    $revSourceHashes[$revManifestPath] = (Get-FileHash -Algorithm SHA256 -LiteralPath $revManifestPath).Hash
    $revManifest = Get-Content -LiteralPath $revManifestPath -Raw | ConvertFrom-Json
    if ($revManifest.format -ne 3 -or $revManifest.game -ne 'rac1' -or $revManifest.version -ne 'ntsc') {
        throw "Unsupported source level: $($revSource.Name)"
    }
    $revDestination = Join-Path $revLevels $revSource.Name
    New-Item -ItemType Directory -Path $revDestination | Out-Null
    foreach ($revChild in (Get-ChildItem -LiteralPath $revSource.FullName)) {
        if ($revChild.Name -eq 'manifest.json') { continue }
        $revTarget = Join-Path $revDestination $revChild.Name
        if ($revChild.PSIsContainer) {
            New-Item -ItemType Junction -Path $revTarget -Target $revChild.FullName | Out-Null
        } else {
            try { New-Item -ItemType HardLink -Path $revTarget -Target $revChild.FullName | Out-Null }
            catch { Copy-Item -LiteralPath $revChild.FullName -Destination $revTarget }
        }
    }
    if ($revManifest.level -ge 0 -and $revManifest.level -le 18) {
        $revNativeAmmo = $revManifest.classes.moby.'225'
        if (-not $revNativeAmmo -or [Math]::Abs([double]$revNativeAmmo.scale - [double]$revArtInfo.native_scale) -gt 0.00000001) {
            throw "The local disc has another native ammunition class scale: $($revSource.Name)"
        }
        if ($revManifest.classes.moby.'32000' -or $revManifest.classes.moby.'2016') { throw 'A required private visual class ID is already in use.' }
        $revOwnAssets = Join-Path $revDestination 'revolverator_ammo'
        New-Item -ItemType Directory -Path $revOwnAssets | Out-Null
        Copy-Item -LiteralPath (Join-Path $revArt 'model.glb'),(Join-Path $revArt 'texture.png') -Destination $revOwnAssets
        foreach ($revClass in @('32000','2016')) {
            $revClassName = if ($revClass -eq '32000') { 'revolverator_ammunition' } else { 'revolverator_ammunition_vendor_preview' }
            $revClassInfo = [ordered]@{mesh='revolverator_ammo/model.glb';scale=[double]$revNativeAmmo.scale;name=$revClassName}
            $revManifest.classes.moby | Add-Member -NotePropertyName $revClass -NotePropertyValue $revClassInfo
        }
        ++$revAdded
    }
    Write-RevolveratorUtf8 (Join-Path $revDestination 'manifest.json') ($revManifest | ConvertTo-Json -Depth 64)
    ++$revManifests
}
if ($revAdded -ne 19 -or $revManifests -lt 21) { throw 'Expected 19 gameplay levels and both native menu levels.' }
foreach ($revSourceManifest in $revSourceHashes.Keys) {
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $revSourceManifest).Hash -ne $revSourceHashes[$revSourceManifest]) {
        throw 'An installed source manifest changed during preparation.'
    }
}
Copy-Item -LiteralPath $revPlugin -Destination (Join-Path $revPlugins 'revolverator') -Recurse
$revPluginConfig = [ordered]@{format=1;mods=@([ordered]@{id='revolverator';enabled=$true;settings=@{}})}
Write-RevolveratorUtf8 (Join-Path $revPlugins 'plugins.json') ($revPluginConfig | ConvertTo-Json -Depth 16)
$revSettings = Join-Path $SessionRoot 'settings.ini'
if (Test-Path -LiteralPath $Settings -PathType Leaf) { Copy-Item -LiteralPath $Settings -Destination $revSettings }
else { Write-RevolveratorUtf8 $revSettings "window_size = 1280x800`naspect = 16:10`n" }
$revArguments = @('--data',$revData,'--levels',$revLevels,'--plugins',(Join-Path $revPlugins 'plugins.json'),
    '--mods',(Join-Path $revBundle 'graphics'),'--cards',$revCards,'--settings',$revSettings,'--window','--no-card')
if ($Level -ge 0) { $revArguments += @('--level',[string]$Level) }
$revReceipt = [ordered]@{format=1;private=$true;prepared=[DateTime]::UtcNow.ToString('O');
    session=$SessionRoot;exe=$Runtime;arguments=$revArguments;manifests=$revManifests;extended_levels=$revAdded;
    original_manifests_unchanged=$true;user_saves_copied=$false;other_installed_mods_copied=$false;
    native_gameplay_verified=$false;started=$false}
$revReceiptPath = Join-Path $SessionRoot 'session.json'
Write-RevolveratorUtf8 $revReceiptPath ($revReceipt | ConvertTo-Json -Depth 20)
if ($PrepareOnly) { $revReceipt | ConvertTo-Json -Depth 20; return }

function ConvertTo-RevolveratorArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    $revEscaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $revEscaped = [regex]::Replace($revEscaped, '(\\+)$', '$1$1')
    return '"' + $revEscaped + '"'
}
$revStart = [Diagnostics.ProcessStartInfo]::new()
$revStart.FileName = $Runtime
$revStart.WorkingDirectory = $SessionRoot
$revStart.UseShellExecute = $false
$revStart.CreateNoWindow = $true
$revStart.Arguments = (($revArguments | ForEach-Object { ConvertTo-RevolveratorArgument $_ }) -join ' ')
foreach ($revKey in @($revStart.EnvironmentVariables.Keys)) {
    if ($revKey.StartsWith('OPENRAC_',[StringComparison]::OrdinalIgnoreCase)) { $revStart.EnvironmentVariables.Remove($revKey) }
}
$revStart.EnvironmentVariables['LOCALAPPDATA']=$revLocal
$revStart.EnvironmentVariables['OPENRAC_DIRECT']='1'
$revStart.EnvironmentVariables['OPENRAC_GL_API']='vulkan'
$revStart.EnvironmentVariables['OPENRAC_MOD_DATA_DIR']=(Join-Path $revLocal 'mod-data')
$revProcess = [Diagnostics.Process]::Start($revStart)
$revReceipt.started=$true
$revReceipt.pid=$revProcess.Id
Write-RevolveratorUtf8 $revReceiptPath ($revReceipt | ConvertTo-Json -Depth 20)
Write-Output ('Private Revolverator session started, PID '+$revProcess.Id)
