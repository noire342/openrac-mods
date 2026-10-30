# Native text localization

`src/localization.hpp` is a pure text helper. It reads no memory, changes no
game state, and allocates nothing. Its strings use printable ASCII while the
native font's extended encoding remains unverified.

Read `Memory::global(localization::native_language_address)` as an `int`, then
pass the value to `localization::strings_for_language(value)`:

| Game value | Text |
| --- | --- |
| 0 | English |
| 2 | French |
| 3 | German |
| 4 | Spanish |
| 5 | Italian |
| 1 or any other value | English fallback |

These are native **game** columns. The console setting uses different numbering
for German and Spanish. The source-of-truth mappings are
`port/assets/disc/messages.h:34`, `port/game/rac1/host/options.c:43` and the NTSC
`src/ui/text/draw_subtitles.c:43`. The game's global is canonical NTSC-U address
`0x15ed88`, which the runtime caller resolves through the SDK.

Local native fixtures `combat-02`, `integrated-pose-03` and `play-06` each contain
game value5 at that address, consistent with their Italian setting. This proves
the value in those captures; it is not a visual validation of all translations.

The helper returns `weapon_name`, `description`, `pickup_singular` and
`pickup_plural`. Every pickup format contains exactly one `%d`, preserving the
native integer formatting contract. The parent adapters select the strings
when their text bank is bound. A language/bank reload must rebind them rather
than leaving pointers to the previous translation. Descriptions remain under
the existing256-byte slot capacity.

The instructions describe holding Circle to capture/spin and releasing to
throw, matching the controller's minimum0.18-second capture hold. They do not
claim that all large enemies are excluded: capture is governed by the actual
compatible-enemy policy. No timing value is exposed as a gameplay UI detail.
