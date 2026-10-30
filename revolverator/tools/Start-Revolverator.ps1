# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
param(
    [string]$Runtime = '',
    [string]$InstalledData = '',
    [string]$Settings = '',
    [string]$SessionRoot = '',
    [ValidateRange(-1,18)][int]$Level = -1,
    [switch]$Trial,
    [Alias('UnlockedFromStart')][switch]$StartUnlocked,
    [switch]$PrepareOnly,
    [switch]$WithoutWeaponXP
)

# Portable preview delivery. PowerShell 5.1+; no Python installation is needed
# on the player's PC. Trial never copies cards or existing mod progress. Normal
# play keeps its own persistent copies of the player's initial save/settings.
$ErrorActionPreference = 'Stop'
function Write-RevolveratorUtf8([string]$Path,[string]$Text) {
    [IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false))
}
$revBundle = [IO.Path]::GetFullPath($PSScriptRoot)
$revOriginalLocal = $env:LOCALAPPDATA
if (-not $Runtime) { $Runtime = Join-Path $env:LOCALAPPDATA 'OpenRAC Launcher\openrac\port\build\release\openrac-rac1-ntsc.exe' }
if (-not $InstalledData) { $InstalledData = Join-Path $env:USERPROFILE 'OpenRAC\active\rac1\data' }
if (-not $Settings) { $Settings = Join-Path $env:LOCALAPPDATA 'openrac\settings\rac1-ntsc.ini' }
if (-not $SessionRoot) { $SessionRoot = Join-Path $revBundle ('.sessions\' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)) }
$Runtime = [IO.Path]::GetFullPath($Runtime)
$InstalledData = [IO.Path]::GetFullPath($InstalledData)
$SessionRoot = [IO.Path]::GetFullPath($SessionRoot)
$revBaseLevels = Join-Path $InstalledData 'port\ntsc'
$revDeliveryPath = Join-Path $revBundle 'delivery.json'
$revProfile = Join-Path $revBundle '.profile'
$revCards = Join-Path $revProfile 'cards'
$revModData = Join-Path $revProfile 'mod-data'
$revSettings = Join-Path $revProfile 'settings.ini'
$revUnlock = [bool]$Trial -or [bool]$StartUnlocked
$revSelectedLevel = if ($Trial) { 1 } else { $Level }
if ($Trial) {
    $revCards = Join-Path $SessionRoot 'cards'
    $revModData = Join-Path $SessionRoot 'mod-data'
    $revSettings = Join-Path $SessionRoot 'settings.ini'
}

foreach ($revRequired in @($Runtime,$revDeliveryPath,(Join-Path $InstalledData 'iso_data'),$revBaseLevels)) {
    if (-not (Test-Path -LiteralPath $revRequired)) { throw "Required installed runtime input is missing: $revRequired" }
}
if (Test-Path -LiteralPath $SessionRoot) { throw 'The session directory already exists. Choose a new -SessionRoot.' }
$revSourcePrefix = $InstalledData.TrimEnd('\') + '\'
if ($SessionRoot.Equals($InstalledData,[StringComparison]::OrdinalIgnoreCase) -or
    $SessionRoot.StartsWith($revSourcePrefix,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'The session must be outside the installed data directory.'
}
if ($Trial -and ($SessionRoot.Equals($revProfile,[StringComparison]::OrdinalIgnoreCase) -or
    $SessionRoot.StartsWith($revProfile.TrimEnd('\') + '\',[StringComparison]::OrdinalIgnoreCase))) {
    throw 'A trial session must be outside the persistent .profile directory.'
}
$revDelivery = Get-Content -LiteralPath $revDeliveryPath -Raw | ConvertFrom-Json
if ($revDelivery.format -ne 1 -or $revDelivery.kind -notin @('revolverator-preview-bundle','revolverator-private-test-bundle')) { throw 'Unsupported delivery format.' }
if ($revDelivery.game -ne 'rac1' -or $revDelivery.disc -ne 'ntsc') { throw 'This delivery supports R&C1 NTSC-U only.' }
if ($revDelivery.runtime_included -ne $false -or -not $revDelivery.compat_included) {
    throw 'This launcher requires the standalone arsenal-compat preview bundle and an installed OpenRAC client.'
}
# The compatibility provider is verified against exact installed builds. It
# changes only the running process; this script never patches a client file.
$revSupportedRuntimeHashes = @('B7F3762AA7076F0D93CAD4033B49B4E966975A17885A05D70F61C9FE8DF94081')
$revRuntimeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Runtime).Hash.ToUpperInvariant()
if ($revSupportedRuntimeHashes -notcontains $revRuntimeHash) {
    throw "Unsupported installed OpenRAC client SHA256: $revRuntimeHash. No profile or game file was changed. Use a verified NTSC-U build; -Runtime selects its existing executable."
}
foreach ($revEntry in $revDelivery.files.PSObject.Properties) {
    $revRelative = $revEntry.Name.Replace('\','/')
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
if ($revUnlock -and -not @($revPluginManifest.settings | Where-Object { $_.key -eq 'start_unlocked' }).Count) {
    throw 'This mod build does not support the starting-weapon setting required for this launch.'
}
$revCompat = Join-Path $revBundle 'plugin\arsenal-compat'
$revCompatManifest = Get-Content -LiteralPath (Join-Path $revCompat 'openrac-mod.json') -Raw | ConvertFrom-Json
if ($revCompatManifest.format -ne 2 -or $revCompatManifest.id -ne 'arsenal-compat' -or
    $revCompatManifest.version -ne '0.1.0' -or -not $revCompatManifest.plugin.'windows-x86_64') {
    throw 'The bundled arsenal-compat provider is invalid.'
}
$revXP = Join-Path $revBundle 'plugin\weapon-xp'
$revUseXP = [bool]$revDelivery.weapon_xp_included -and -not $WithoutWeaponXP
if ($revUseXP) {
    $revXPManifest = Get-Content -LiteralPath (Join-Path $revXP 'openrac-mod.json') -Raw | ConvertFrom-Json
    if ($revXPManifest.format -ne 2 -or $revXPManifest.id -ne 'weapon-xp' -or -not $revXPManifest.plugin.'windows-x86_64') {
        throw 'The bundled Weapon XP package is invalid.'
    }
}
$revArt = Join-Path $revBundle 'ammunition'
$revArtInfo = Get-Content -LiteralPath (Join-Path $revArt 'asset.json') -Raw | ConvertFrom-Json
if ($revArtInfo.native_pickup_class -ne 225 -or $revArtInfo.world_class -ne 32000 -or $revArtInfo.vendor_class -ne 2016 -or
    $revArtInfo.weapon_class -ne 2017 -or $revArtInfo.native_weapon_class -ne 180) {
    throw 'The ammunition asset identities do not match this mod.'
}

# Initialize one persistent copy. No existing profile file is refreshed from
# the original card/settings, so a second session cannot erase bundle progress.
$revProfileCreated = $false
$revProfileInfo = [pscustomobject]@{user_saves_copied=$false}
if (-not $Trial) {
    if (-not (Test-Path -LiteralPath $revProfile)) {
        $revStage = Join-Path $revBundle ('.profile-initializing-' + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $revStage,(Join-Path $revStage 'cards'),(Join-Path $revStage 'mod-data') | Out-Null
        $revCopiedInputs = @{}
        function Copy-RevolveratorInputTree([string]$Source,[string]$Destination) {
            if (-not (Test-Path -LiteralPath $Source -PathType Container)) { return $false }
            [IO.Directory]::CreateDirectory($Destination) | Out-Null
            foreach ($revInput in (Get-ChildItem -LiteralPath $Source -Recurse -File -Force)) {
                $revCopiedInputs[$revInput.FullName] = (Get-FileHash -Algorithm SHA256 -LiteralPath $revInput.FullName).Hash
            }
            foreach ($revChild in (Get-ChildItem -LiteralPath $Source -Force)) {
                Copy-Item -LiteralPath $revChild.FullName -Destination $Destination -Recurse
            }
            return $true
        }
        $revOriginalCards = Join-Path $revOriginalLocal 'openrac\memcard\SCUS_971.99'
        $revCopiedCards = Copy-RevolveratorInputTree $revOriginalCards (Join-Path $revStage 'cards')
        $revCopiedCard2 = Copy-RevolveratorInputTree ($revOriginalCards + '.slot2') (Join-Path $revStage 'cards.slot2')
        $revCopiedXP = Copy-RevolveratorInputTree (Join-Path $revOriginalLocal 'OpenRAC\weapon-xp') (Join-Path $revStage 'mod-data\OpenRAC\weapon-xp')
        if (Test-Path -LiteralPath $Settings -PathType Leaf) {
            $revCopiedInputs[$Settings] = (Get-FileHash -Algorithm SHA256 -LiteralPath $Settings).Hash
            Copy-Item -LiteralPath $Settings -Destination (Join-Path $revStage 'settings.ini')
        } else { Write-RevolveratorUtf8 (Join-Path $revStage 'settings.ini') "window_size = 1280x800`naspect = 16:10`n" }
        foreach ($revInputPath in $revCopiedInputs.Keys) {
            if ((Get-FileHash -Algorithm SHA256 -LiteralPath $revInputPath).Hash -ne $revCopiedInputs[$revInputPath]) {
                throw 'An original card/settings/XP input changed during its first profile copy. No original was written.'
            }
        }
        $revProfileInfo = [ordered]@{format=1;kind='revolverator-persistent-profile';disc='ntsc';
            created=[DateTime]::UtcNow.ToString('O');user_saves_copied=[bool]$revCopiedCards;
            second_card_copied=[bool]$revCopiedCard2;xp_profiles_copied=[bool]$revCopiedXP;
            source_card=$revOriginalCards;source_hashes=$revCopiedInputs;original_inputs_unchanged=$true}
        Write-RevolveratorUtf8 (Join-Path $revStage 'profile.json') ($revProfileInfo | ConvertTo-Json -Depth 16)
        # Directory.Move refuses an existing destination, including a concurrently
        # created profile. It never merges or overwrites a previous player's copy.
        [IO.Directory]::Move($revStage,$revProfile)
        $revProfileCreated = $true
    }
    $revProfileInfoPath = Join-Path $revProfile 'profile.json'
    if (-not (Test-Path -LiteralPath $revProfileInfoPath -PathType Leaf)) { throw 'The existing .profile is incomplete; it was preserved.' }
    $revProfileInfo = Get-Content -LiteralPath $revProfileInfoPath -Raw | ConvertFrom-Json
    if ($revProfileInfo.format -ne 1 -or $revProfileInfo.kind -ne 'revolverator-persistent-profile' -or $revProfileInfo.disc -ne 'ntsc') {
        throw 'The existing .profile belongs to another format; it was preserved.'
    }
    foreach ($revPersistentPath in @($revCards,$revModData,$revSettings)) {
        if (-not (Test-Path -LiteralPath $revPersistentPath)) { throw 'The existing persistent profile is incomplete; it was preserved.' }
    }
}

New-Item -ItemType Directory -Path $SessionRoot | Out-Null
$revLevels = Join-Path $SessionRoot 'levels'
$revData = Join-Path $SessionRoot 'data'
$revPlugins = Join-Path $SessionRoot 'plugins'
New-Item -ItemType Directory -Path $revLevels,$revData,$revPlugins | Out-Null
if ($Trial) {
    New-Item -ItemType Directory -Path $revCards,$revModData | Out-Null
    if (Test-Path -LiteralPath $Settings -PathType Leaf) {
        $revSettingsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Settings).Hash
        Copy-Item -LiteralPath $Settings -Destination $revSettings
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Settings).Hash -ne $revSettingsHash) {
            throw 'The original settings changed during preparation. No original was written.'
        }
    } else { Write-RevolveratorUtf8 $revSettings "window_size = 1280x800`naspect = 16:10`n" }
}
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
        $revNativeWeapon = $revManifest.classes.moby.'180'
        if (-not $revNativeWeapon) { throw "Native Walloper class180 is missing: $($revSource.Name)" }
        $revOriginalWeapon = $revNativeWeapon | ConvertTo-Json -Depth 64 -Compress
        if (-not $revNativeAmmo -or [Math]::Abs([double]$revNativeAmmo.scale - [double]$revArtInfo.native_scale) -gt 0.00000001) {
            throw "The local disc has another native ammunition class scale: $($revSource.Name)"
        }
        if ($revManifest.classes.moby.'32000' -or $revManifest.classes.moby.'2016' -or $revManifest.classes.moby.'2017') {
            throw 'A required private visual class ID is already in use.'
        }
        $revOwnAssets = Join-Path $revDestination 'revolverator_ammo'
        New-Item -ItemType Directory -Path $revOwnAssets | Out-Null
        Copy-Item -LiteralPath (Join-Path $revArt 'model.glb'),(Join-Path $revArt 'texture.png') -Destination $revOwnAssets
        foreach ($revClass in @('32000','2016')) {
            $revClassName = if ($revClass -eq '32000') { 'revolverator_ammunition' } else { 'revolverator_ammunition_vendor_preview' }
            $revClassInfo = [ordered]@{mesh='revolverator_ammo/model.glb';scale=[double]$revNativeAmmo.scale;name=$revClassName}
            $revManifest.classes.moby | Add-Member -NotePropertyName $revClass -NotePropertyValue $revClassInfo
        }
        $revOwnWeapon = Join-Path $revDestination 'revolverator_weapon'
        New-Item -ItemType Directory -Path $revOwnWeapon,(Join-Path $revOwnWeapon 'mobys'),(Join-Path $revOwnWeapon 'textures') | Out-Null
        $revWeaponArt = Join-Path $revBundle ('graphics\' + $revSource.Name)
        Copy-Item -LiteralPath (Join-Path $revWeaponArt 'mobys\moby_2017.glb') -Destination (Join-Path $revOwnWeapon 'mobys\moby_2017.glb')
        Copy-Item -LiteralPath (Join-Path $revWeaponArt 'textures\moby_revolverator_original.png') -Destination (Join-Path $revOwnWeapon 'textures\moby_revolverator_original.png')
        $revWeaponClassInfo = $revOriginalWeapon | ConvertFrom-Json
        $revWeaponClassInfo.mesh = 'revolverator_weapon/mobys/moby_2017.glb'
        $revWeaponClassInfo | Add-Member -NotePropertyName 'name' -NotePropertyValue 'revolverator_independent_weapon' -Force
        $revManifest.classes.moby | Add-Member -NotePropertyName '2017' -NotePropertyValue $revWeaponClassInfo
        if (($revManifest.classes.moby.'180' | ConvertTo-Json -Depth 64 -Compress) -ne $revOriginalWeapon) {
            throw 'The original Walloper definition was unexpectedly changed.'
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
Copy-Item -LiteralPath $revCompat -Destination (Join-Path $revPlugins 'arsenal-compat') -Recurse
$revModList = @([ordered]@{id='arsenal-compat';enabled=$true;settings=@{}})
if ($revUseXP) {
    Copy-Item -LiteralPath $revXP -Destination (Join-Path $revPlugins 'weapon-xp') -Recurse
    $revModList += [ordered]@{id='weapon-xp';enabled=$true;settings=@{scale_damage=$false;scale_capacity=$false}}
}
$revModList += [ordered]@{id='revolverator';enabled=$true;settings=@{start_unlocked=$revUnlock}}
$revPluginConfig = [ordered]@{format=1;mods=$revModList}
Write-RevolveratorUtf8 (Join-Path $revPlugins 'plugins.json') ($revPluginConfig | ConvertTo-Json -Depth 16)
$revArguments = @('--data',$revData,'--levels',$revLevels,'--plugins',(Join-Path $revPlugins 'plugins.json'),
    '--mods',(Join-Path $revBundle 'graphics'),'--cards',$revCards,'--settings',$revSettings,'--window')
if ($revSelectedLevel -ge 0) { $revArguments += @('--level',[string]$revSelectedLevel) }
if ($Trial) { $revArguments += @('--no-card','--skip-movies') }
$revEnvironment = [ordered]@{LOCALAPPDATA=$revModData;OPENRAC_DIRECT='1';OPENRAC_GL_API='vulkan';OPENRAC_MOD_DATA_DIR=$revModData}
if ($Trial) { $revEnvironment.OPENRAC_PRESS='100:4000:5,k30:8:5' }
$revReceipt = [ordered]@{format=1;private=$true;prepared=[DateTime]::UtcNow.ToString('O');
    mode=$(if ($Trial) {'trial'} else {'persistent'});start_unlocked=$revUnlock;no_card=[bool]$Trial;
    session=$SessionRoot;exe=$Runtime;arguments=$revArguments;environment=$revEnvironment;manifests=$revManifests;extended_levels=$revAdded;
    extended_classes=@(2017,32000,2016);native_walloper_preserved=$true;
    runtime_sha256=$revRuntimeHash.ToLowerInvariant();runtime_included=$false;installed_runtime_verified=$true;
    compat_enabled=$true;compat_version=$revCompatManifest.version;
    profile=$(if ($Trial) {$null} else {$revProfile});profile_created=$revProfileCreated;cards=$revCards;mod_data=$revModData;
    original_manifests_unchanged=$true;user_saves_copied=[bool]$revProfileInfo.user_saves_copied;
    original_inputs_unchanged=$true;weapon_xp_enabled=$revUseXP;other_installed_mods_copied=$false;
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
foreach ($revPair in $revEnvironment.GetEnumerator()) { $revStart.EnvironmentVariables[$revPair.Key]=[string]$revPair.Value }
$revProcess = [Diagnostics.Process]::Start($revStart)
$revReceipt.started=$true
$revReceipt.pid=$revProcess.Id
Write-RevolveratorUtf8 $revReceiptPath ($revReceipt | ConvertTo-Json -Depth 20)
Write-Output ('Revolverator '+$revReceipt.mode+' session started, PID '+$revProcess.Id)
