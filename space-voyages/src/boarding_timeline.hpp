// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace spacevoyages::boarding {
// Retail camera shot markers are stored as flag1 in the camera keyframe's
// fourth position word. Both supported discs'60Hz boarding archives0/1/2/3/8
// mark103,243,363,503 and end at598. Source receipts (including archive hashes):
// work/space-voyages-tools/boarding-audit/{ntsc,pal}-retail-shot-metadata.json.
// These numbers belong to this verified clip, never the host window's frame.
inline constexpr int bank_ticks=96;
inline constexpr int clip_end=598;
inline constexpr int console_begin=103;
inline constexpr int exterior_begin=243;
inline constexpr int dashboard_begin=503;
// update_resident_gameplay_state's ship branch accumulates white fade only
// after0x1D0 (464), and releases it in the later cockpit shot. Exit before
// that effect begins, since the terminal cockpit shot is intentionally gone.
inline constexpr int native_white_flash_begin=465;

struct Playback {
    int time{};
    int frame{};
    int bank{};
    int end{};
    int variant{};
};

enum class Action {None,SeekExterior,FinishBoarding};
enum class EntryPresentation {OmitConsole,PreserveEntry};
struct Plan {
    Action action=Action::None;
    int time_before_next_tick{};
    int bank{};
    int frame_before_next_tick{};
    constexpr bool parse_required()const noexcept{return action==Action::SeekExterior;}
};

constexpr bool coherent(const Playback& state)noexcept {
    return state.end==clip_end&&state.variant>=0&&state.variant<=2&&
        state.time>=0&&state.time<clip_end&&state.bank>=0&&state.bank<7&&
        state.frame>=0&&state.frame<bank_ticks&&
        state.time==state.bank*bank_ticks+state.frame;
}

// Called after the native picture has been presented. The caller supplies the
// complete authorized departure scope (armed, original level, state0, mode6,
// no planet menu, native60Hz) and owns per-journey once flags. An unrecognized
// archive, a missed cue, or inconsistent playback retains original behavior.
constexpr Plan plan(const Playback& state,bool eligible,
                    bool exterior_already_skipped,bool finish_already_skipped,
                    EntryPresentation entry=EntryPresentation::OmitConsole)noexcept {
    if(!eligible||!coherent(state))return {};
    if(entry==EntryPresentation::OmitConsole&&!exterior_already_skipped&&state.time==console_begin-1){
        // First parse bank2 through the native helper. Then install these
        // pre-next-tick cursors together. The next native update increments
        // them and evaluates the authored camera AND every actor at tick243.
        return {Action::SeekExterior,exterior_begin-1,
                exterior_begin/bank_ticks,exterior_begin%bank_ticks-1};
    }
    if(!finish_already_skipped&&state.time==native_white_flash_begin-1){
        // Only write absolute time. The next native tick reaches end before
        // any camera/pose read, runs the original object cleanup and menu14
        // handoff. Preserve engine ignition through464; skip the white flash
        // and terminal cockpit together. No fader RAM write is required.
        return {Action::FinishBoarding,clip_end-1,state.bank,state.frame};
    }
    return {};
}
}
