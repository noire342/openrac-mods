# Native text localization

`src/localization.hpp` supplies text without reading game memory or changing
state. The adapter reads the native NTSC-U language field through SDK relocation
and rebinds its own text records when language/bank context changes.

| Native game value | Language |
| --- | --- |
| 0 | English |
| 2 | French |
| 3 | German |
| 4 | Spanish |
| 5 | Italian |
| 1 or unknown | English fallback |

These are game columns, not the console-language numbering. Strings remain
printable ASCII until the font's extended encoding is verified. Pickup formats
retain exactly one `%d`; descriptions fit the existing 256-byte slot.

Version 0.2 projects custom text only in Revolverator contexts. The original
Walloper name, description and native XP names remain separate. When the optional
XP provider is active and versioned names are enabled, the custom name uses its
own level (`Revolverator vN`); it does not read the Walloper's XP level.

The control text describes holding Circle to capture/spin and releasing to
throw. The controller's minimum hold is 0.18 seconds, without placing tuning
constants in gameplay UI text. Capture eligibility follows the native enemy
policy; bosses remain melee targets. Host text-format checks do not prove
translation layout or glyph rendering in every language; native 0.2 visual QA
remains open.
