# Native component inventory

`NativePresentation` owns the Componenti / Components content page, using the
native weapon-inventory panels, font, navigation and model-preview space.
Inventory rules, component identities and transactions remain in the pure core.
The reusable `arsenal-compat/src/native_pause_menu.*` adapter owns pause-root
composition, physical rows and transitions; it has no component/catalog logic.

The copy-only `openrac_arsenal_pause_query(1)` registry supplies stable entry IDs,
localized captions, visibility and order. One generation-checked presenter owns
the native integration. A single producer appears directly at the pause root.
Multiple producers share Altri menu / Other menus, a native directory with seven
entries and a page button, accommodating 32 entries over five pages. It uses at
most eight physical rows and leaves all original pause entries available.

## Navigation and input ownership

Every visible root row receives both circular neighbours. Up from the first row
therefore reaches the custom last entry, including after native initializers
change Goodies visibility. Unknown foreign links are preserved. An early Cross
waits for a valid opened Root pose before entering the directory.

Triangle requests native back navigation and owns that whole physical press
until release. Holding it cannot close a second page or trigger a world pickup.
Start/Select preserves native close behaviour. Native page callbacks are routed
only for owned arguments; browsing components cannot equip or grant a weapon.

## Native rendering and transitions

Root and directory rows use the original panel class and animation sequences.
Text chooses the original descriptor's font bank (normal, small or large), uses
native measurement and clips long captions with measured ellipsis. The content
page uses PAL font advances for its list, name and details panes. Full names,
provenance and exact grouped uint64 quantities remain in the details pane.
The thumbnail renderer reads the real mesh from the player's extracted assets;
no game mesh or texture is shipped with this module.

Physical poses and visibility are leased through the frontend capture, then
restored field by field only when still owned. Actor/class/pvars identity and
level epochs invalidate stale leases. Animation references are copied only
where portable; an instance's manipulator pointer is never borrowed.

The native engine temporarily sets `MenuSystem.current` to zero during the
12-tick Root/directory transition. Forward navigation uses the destination's
animations, while back navigation reverses the old page's animations. The
adapter resolves that native relationship while the transition is active and
continues fitting the appropriate eight rows and hiding unused preview actors.
It preserves live frame/blend/speed data and horizontal animation, fitting along
the opened Root's vertical axis. Open-pose caches are not applied during this
transition. Other native pages keep their own animation/layout ownership.

This distinction fixes the extra overlapping panel seen when Goodies/Premi was
present. The same defect occurred with several fonts: the font was not its cause.
Private PAL fixture `pal-menu-transition-fixed-30` passed 20 functional checks,
exit0, with 107 original inputs unchanged; images 600/610/620 and 750/760/770 show
stable menus and both transitions. Its predecessor29b retains the failing
visual evidence. The host regression failed on the former implementation and
passes520 checks with the transition fix. These are scoped adapter proofs;
combined-package gameplay, saves and long-label rendering have their own receipts.

## Collection guidance

An anchored prompt is drawn after native HUD rendering, inside its existing
texture-paging scope. It uses native projection, clipping, shadow and the actual
Triangle glyph byte0x12. It does not restart paging or add a world draw-list
callback in this integration. Offscreen/behind-camera targets are rejected.
A native subtitle supplies fallback guidance and brief pickup confirmations.
Existing subtitles and native interaction/dialog channel12 have priority.

Projection output is read only from the adapter's bounded SDK stack allocation;
that mapped stack lives outside ordinary32MiB actor RAM. Native actor-address
validation is unchanged. The real-drop PAL prompt fixture20 passed21 checks,
including held Triangle and an equipped Suck Cannon, exit0 and107 unchanged
originals; frame1410 shows the native glyph and Testa di Birdbot cleanly.

Loading mode6 or an invalid overlay suspends binding, input and rendering.
Reset then forgets old leases without writing into storage which may already
belong to incoming compressed level data. The content page and shared registry
are tested separately from physical PAL rendering. Detailed historical and
rejected experiments remain under `work/salvage-native-menu-qa`, outside the mod
payload; private raw game-memory snapshots must not be packaged.
