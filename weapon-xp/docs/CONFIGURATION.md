# Configuration

Use the launcher's existing Weapon Experience settings. Restart after changing launcher
settings; other native mods may use API v3 for session-only live changes.

| Key | Default | Meaning |
|---|---|---|
| profile | campaign-1 | Independent XP campaign profile |
| xp_rate | 1 | Global quarter-XP rate, .25 to 4 |
| max_kill_xp | 500 | Native bolt reward cap before multipliers, integer 0..100000 |
| scale_damage | false | Enable configured damage curve |
| scale_capacity | false | Enable configured ammo capacity curve; no refills |
| hud | true | XP in the original weapon tile/vertical gauge |
| reduced_motion | false | Reduce XP bar motion |
| level_up_animation | true | Native acquisition presentation on earned levels |
| versioned_names | true | Localized native names with vN suffix |
| flip_morph_model | false | Roll the held Morph-o-Ray upside down; barrel still toward the enemy |
| rules_file | rules.ini | Advanced rules filename in the XP profile directory |

On Windows the default file is `%LOCALAPPDATA%/OpenRAC/weapon-xp/rules.ini`.
The mod creates a commented template only when absent. The default file is shared by
profiles in that directory; choose a different filename for a separate ruleset.
Simple filenames up to 64 ASCII letters/digits/underscores/dots/hyphens are accepted.

The Morph-o-Ray option rolls its held model before the native muzzle-joint query,
so the original beam and effects start at the flipped barrel. It retains native
aiming/transformation code and XP rules. Item 21 is outside the twelve-weapon XP catalog.
API v3 consumers using this flag should require Weapon Experience >=0.6.1.

```ini
format=1

[weapon.15]
thresholds=0,600,2000,4500,8000,13000,20000,29000,41000,60000
damage=1,1.15,1.30,1.50,1.75,2,2.25,2.5,2.75,3
ammo=1,1.125,1.25,1.375,1.5,1.625,1.75,1.875,2,2.125
xp_multiplier=2
```

This doubles the Blaster's combat XP and gives it a slower curve. Damage/capacity curves
take effect only when their independent switches are enabled. Explicit API `award_xp`
uses the amount chosen by the caller, without applying the combat multiplier; a zero
weapon XP multiplier also pauses that command. Stored XP is kept.

IDs: 9 Suck Cannon, 10 Bomb Glove, 11 Devastator, 13 Visibomb, 15 Blaster, 16 Pyrocitor,
17 Mine Glove, 18 Walloper, 19 Tesla Claw, 20 Glove of Doom, 23 RYNO, 24 Drone Device.
Each omitted weapon/key uses shipped defaults. Comments start with `;` or `#`.

Validation requires exactly ten increasing integer thresholds, first 0, last at most
1,000,000,000. Multiplier arrays have ten finite nondecreasing values from 1 to 16,
starting at 1. `xp_multiplier` is an integer 0..16. Level count remains ten.
Unknown keys, duplicate keys/sections, overflow, malformed arrays and files over 32 KiB
are rejected. UTF-8, with optional BOM, and Windows/Unix line endings are accepted.
At startup invalid rules decline loading with a diagnostic; existing XP is preserved.
During an API reload, invalid rules leave the complete previous configuration active.

Changing thresholds recomputes displayed levels from existing XP. It does not award XP,
erase points or replay acquisition scenes. A lower cap never truncates old XP; raising
the cap permits earning again. XP persistence uses checksummed format 4, with the old
format 1/2/3 file backed up before migration. Restore its corresponding backup before
downgrading to an older DLL.
