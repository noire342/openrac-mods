# Native acquisition presentation

## Integrated 0.3.1 evidence

On the unchanged official OpenRAC 0.10 NTSC-U client, the final-pulse shop fixture
without Weapon XP completes the actual Gold purchase and full acquisition
scene. Combat/leveling fixtures verify the Gold model 2018 rather than the
Walloper, exactly one visible Ratchet and locked input. Actual enemy XP then
triggers one additional version 2 acquisition while retaining Gold identity.
That leveling fixture lowers only its session thresholds to exercise the
transition; it does not seed or award XP. Original progression rules are
restored afterward. See [native validation](NATIVE_VALIDATION.md) for receipts,
artifact hashes and scope. These observations do not establish full-campaign
or PAL acquisition coverage.

`NativeAcquisition` owns a presentation lifecycle, independent of Weapon XP.
The Golden purchase and custom level-up events enqueue the native item 18
(Colpitore/Walloper) animation. Ordinary vendor acquisition retains its native
entry point; the optional starting unlock does not force a cutscene. Session
projects the logical Rotatore metadata, localized/versioned name and base/Gold model.
The presenter never calls `Item_Give`, awards XP, refills ammo or changes prices,
ownership or Gold flags.

Construct it with `(api, mod, session_arena, Context{session, project, restore, pose_item})`.
The callbacks run on the game thread: `project` once immediately before the
native scene starts and `restore` after its final cleanup. While `active()` is
true, the Session's ordinary context restoration must continue projecting the
Rotatore. Native scene model callbacks can use `presenting()` to apply its
base/Gold renderer alias without requiring that it was equipped before the scene.
`pose_item` visits the native scene's class 180 actor after asynchronous scene
loading. It applies the custom renderer even when the ordinary held-item hook
does not run. Its rotor preserves the authored root/wrist animation while
neutralizing the original glove's finger joints; it never moves Ratchet's bones.

Call `request()` once for each acquisition/purchase/level event. It returns false
when the bounded 64-event queue is full. Call `frame(safe, external_active)` every
game frame, including during cutscenes. `safe` excludes capture/throw ownership;
`external_active` reports an already active XP presentation. A nonzero native
game mode independently prevents starting over another scene. Do not also send
these custom events to XP's presenter: there should be one owner per event.

In the primary native pad hook call `filter_report(raw, size)` **before** the
original native report processor. `active()` includes quiet preparation and the
last gameplay transition frame; clear any cached mod attack input while active.
Neutral controls let movement and held attacks end, then two quiet samples are
required before snapshotting. The native scene itself locks the hero.

The trigger is a 0x100-byte snapshot of Ratchet in 0x120 bytes of explicitly
reserved Session arena storage. The native presenter is
`FUN_L01_002aea70(trigger,18)`; restoration uses the complete placement routine
`FUN_L00_00216f90(position,rotation,0,1)`. The presenter snapshots and restores
the native vendor/menu state, exact hero position/rotation/velocity and equipped
item bindings. It requests the previous item through native switching rather
than restoring stale weapon-actor pointers. A later foreign vendor owner is
preserved.

The real hero is hidden only after confirming a separate class 0 scene actor.
If the scene uses the real hero, it stays visible. Cleanup restores only the
owned visibility bit, preserving unrelated flag changes. Input stays locked
through two successive mode 0 frames because native actors retire one frame
after gameplay mode resumes. Camera alignment keeps the original stage motion
and rotates/translates it toward the existing collision-aware gameplay camera.

Call `reset()` before releasing level storage or changing save context. Guest
cleanup is guarded by level, arena range and body identity; new-level pointers
are never dereferenced through an old token. Reset clears queued requests.

`native_acquisition_test.cpp` covers base/Golden projections, continued firing,
two-stage scene exit, duplicate-hero hiding, external XP scenes, exact restoration,
foreign vendor/body ownership, level changes, missing bindings/memory and input
span bounds. These host tests do not replace native scene/model/name validation.
