// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "weapon_audio.hpp"
#include <openrac/openrac_mod.h>
namespace revolverator {
class NativeWeaponAudio final : public WeaponAudioSink,private WeaponAudioPlayback {
public:
 NativeWeaponAudio(const openrac_api*,openrac_mod*);
 ~NativeWeaponAudio();
 bool emit(WeaponAudioCue,float intensity=1.f)override;
 void update(const WeaponAudioFrame&);
 void reset();
 bool available(WeaponAudioCue)const override;
private:
 const openrac_api* api_;openrac_mod* mod_;
 std::array<int,weapon_audio_cue_count> clips_{};
 WeaponAudio policy_;
 bool supported_{};float retry_seconds_{};
 void load();
 Voice play(WeaponAudioCue,float,float,bool)override;
 bool set(Voice,float,float)override;
 void stop(Voice)override;
};
}
