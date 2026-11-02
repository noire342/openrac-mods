// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_weapon_visual.hpp"

namespace revolverator {
NativeWeaponVisual::NativeWeaponVisual(const openrac_api* api,GuestArena& level_arena,GuestArena& held_arena)
    :Memory(api),alias_(api),held_(api,held_arena),standalone_(api,level_arena,true),
     equipped_(api,level_arena,true,true),acquisition_(api,level_arena,true,true){}

void NativeWeaponVisual::begin_native_frame(){alias_.frame(true);}
void NativeWeaponVisual::restore_native(){alias_.frame();}
void NativeWeaponVisual::set_mode(int mode){
    if(mode!=3){standalone_.reset();equipped_.reset();}
    if(mode!=5)acquisition_.reset();
}
NativeRotor& NativeWeaponVisual::preview(Preview context){
    switch(context){
    case Preview::Standalone:return standalone_;
    case Preview::Equipped:return equipped_;
    case Preview::Acquisition:return acquisition_;
    }
    return standalone_;
}
void NativeWeaponVisual::prepare_native(unsigned actor){
    if(valid(actor,256)&&alias_.owns(actor))
        write<unsigned short>(actor+0xa6,NativeWeaponAlias::source_class);
}
NativeWeaponVisual::Identity NativeWeaponVisual::identity(unsigned actor)const{
    if(!valid(actor,256))return {};
    return {actor,read<unsigned>(actor+0x24),read<unsigned>(actor+0x78),read<unsigned>(actor+0x38)};
}
void NativeWeaponVisual::clear_held(){
    held_.reset();assembly_.reset();held_identity_={};equip_pending_=live_=logical_equipped_=pause_continuation_=false;
}
void NativeWeaponVisual::update_held(const HeldState& state){
    const auto& frame=state.frame;
    if(!state.logical_equipped){clear_held();alias_.apply(state.actor,false);return;}
    logical_equipped_=true;
    // Deep native weapon menus may temporarily retire the world item or put
    // a class0 preview/body in its slot. That is not a logical weapon switch.
    // Keep the semantic equip and paused envelope until the real item returns.
    if(frame.paused&&held_identity_.actor)pause_continuation_=true;
    const bool native_actor=valid(state.actor,256)&&
        (read<unsigned short>(state.actor+0xa6)==NativeWeaponAlias::source_class||alias_.owns(state.actor));
    if(!frame.equipped||!native_actor){
        live_=false;
        if(!frame.paused&&!pause_continuation_){clear_held();logical_equipped_=true;}
        alias_.apply(state.actor,false);return;
    }
    live_=frame.playing&&!frame.paused;
    // A native item callback can publish the render identity before the first
    // visual frame ever attaches its rotor. Canonicalize only our owned actor
    // for that pose boundary; otherwise an initial2017/2018 would permanently
    // prevent NativeRotor's deliberately strict native180 attachment.
    prepare_native(state.actor);
    struct Republish {
        NativeWeaponVisual& owner;unsigned actor;bool golden;
        ~Republish(){try{owner.publish_held(actor,true,golden);}catch(...){} }
    } render{*this,state.actor,state.golden};
    held_.update(state.actor,state.ammunition,state.phase,state.charge,state.enemy_angle,
                 frame.seconds,frame.playing,frame.paused);
    if(held_.owns(state.actor)){
        const auto current=identity(state.actor);
        if(!(current==held_identity_)){
            const bool continued=pause_continuation_&&held_identity_.actor;
            held_identity_=current;
            if(!continued){assembly_.begin(state.ammunition>0);equip_pending_=true;}
        }
        assembly_.update({frame.seconds,frame.playing,frame.paused,state.phase!=Phase::Idle,state.ammunition>0});
        held_.set_assembly_pose(assembly_.pose());held_.apply();
        if(live_)pause_continuation_=false;
    }
    // Every return restores the logical model, including the playing frame in
    // which native code may enter pause and omit the item's next update. The
    // rotor itself freezes its angle and activation while paused.
}
bool NativeWeaponVisual::publish_held(unsigned actor,bool custom,bool golden){
    if(!custom){
        if(logical_equipped_&&pause_continuation_)held_.reset();
        else clear_held();
        return alias_.apply(actor,false);
    }
    held_.apply();return alias_.apply(actor,true,golden);
}
bool NativeWeaponVisual::publish_preview(Preview context,unsigned actor,bool custom,bool golden,float fit){
    auto& pose=preview(context);
    // A foreign native actor can update while an acquisition actor is still
    // awaiting its draw. Releasing that actor's projection must not reset the
    // context's different pose owner. Actual menu focus/exit uses clear_preview.
    if(!custom){if(pose.owns(actor))pose.reset();return alias_.apply_preview(actor,false);}
    if(valid(actor,256)&&(read<unsigned short>(actor+0xa6)==NativeWeaponAlias::source_class||alias_.owns(actor)))
        pose.update(actor,0,Phase::Idle,0,0,0,false,true);
    const bool published=alias_.apply_preview(actor,true,golden);
    if(published&&context==Preview::Standalone)alias_.fit_preview(actor,fit);
    return published;
}
void NativeWeaponVisual::clear_preview(Preview context){preview(context).reset();}
std::optional<NativeWeaponVisual::EquipEvent> NativeWeaponVisual::poll_equip(){
    if(!equip_pending_||!live_)return {};
    equip_pending_=false;
    if(!held_.owns(held_identity_.actor)||!(identity(held_identity_.actor)==held_identity_))return {};
    return EquipEvent{held_identity_.actor};
}
void NativeWeaponVisual::reset(){
    clear_held();held_.reset(true);standalone_.reset(true);equipped_.reset(true);acquisition_.reset(true);alias_.reset();
}
}
