// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_weapon_visual.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"weapon visual: %s\n",why);std::exit(1);}}
constexpr unsigned held=0x200000,standalone=0x200100,equipped=0x200200,scene=0x200300,stock=0x200400;
constexpr unsigned part=0x400000,skeleton=0x420000;
constexpr unsigned assembly_frames=unsigned(EquipAssembly::duration*60.f)+2;
struct Fixture;
Fixture* live;
struct Fixture {
    std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
    openrac_api api{};GuestArena level_arena,held_arena;
    const RegionBindings& region;int level=1;
    template<class T>T get(unsigned at)const{T result{};std::memcpy(&result,ram.data()+at,sizeof result);return result;}
    template<class T>void put(unsigned at,const T& value){std::memcpy(ram.data()+at,&value,sizeof value);}
    unsigned global(unsigned at)const{return region.data(at);}
    void actor(unsigned at){
        put<unsigned char>(at+0x20,2);put(at+0x24,part);put(at+0x38,at/256);
        put(at+0x78,at+0x400000);put<unsigned short>(at+0xa6,180);
        put(at+0x64,at+0x10000);put<float>(at+0x2c,2.f);
    }
    explicit Fixture(bool pal):region(pal?RegionBindings::pal():RegionBindings::ntsc()){
        live=this;api.memory=ram.data();api.game="rac1";api.version=region.version();
        api.loaded_level=+[]{return live->level;};api.relocate_data=+[](unsigned at){return at;};
        api.relocate_level_data=+[](int,unsigned at){return at;};
        std::memset(ram.data()+global(0x1b3ac0),255,2048);
        put<int>(global(0x15ff00),224);put<unsigned char>(global(0x1b3ac0)+180,5);
        put(global(0x1b3200)+5*4,part);put<unsigned short>(global(0x1b3900)+5*2,180);
        put(global(0x1b3580)+5*4,0x2bb758u);put<unsigned char>(part+8,5);
        put(part+0x14,skeleton);put<float>(part+0x24,.1458333283662796f);
        for(unsigned joint=0;joint<5;++joint){
            std::array<float,16> bind{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            if(joint==4){bind[12]=-132;bind[13]=3060;bind[14]=-10;}
            put(skeleton+joint*64,bind);
        }
        for(unsigned at:{held,standalone,equipped,scene,stock})actor(at);
        held_arena.reserve(0x500000,4096);level_arena.reserve(0x510000,16384);
    }
    NativeWeaponVisual::HeldState state(Phase phase,bool golden)const{
        NativeWeaponVisual::HeldState result;result.actor=held;result.ammunition=20;
        result.phase=phase;result.charge=.5f;result.enemy_angle=.75f;result.golden=golden;result.logical_equipped=true;
        result.frame.equipped=result.frame.playing=result.frame.alive=true;return result;
    }
};
}
int main(){
    for(bool pal:{false,true})for(bool golden:{false,true})for(bool paused_first:{false,true}){
        Fixture f(pal);NativeWeaponVisual visual(&f.api,f.level_arena,f.held_arena);
        const auto stock_record=f.get<std::array<unsigned char,256>>(stock);
        const unsigned render=golden?2018:2017;
        auto parts=[&]{unsigned count=0,at=f.get<unsigned>(held+0x64);
            for(unsigned i=0;i<16&&Memory::valid(at,64);++i){
                const auto joint=f.get<unsigned>(at+4);
                if(joint>=0x70000000u&&joint<=0x70000100u&&(joint&63u)==0)++count;
                at=f.get<unsigned>(at+8);
            }return count;};
        // Actual native ordering: item update can publish the visual before
        // the first mod frame has attached any rotor modifiers.
        visual.begin_native_frame();require(visual.publish_held(held,true,golden),"native first sight can publish before a rotor owner exists");
        require(parts()==0&&f.get<unsigned short>(held+0xa6)==render,"bootstrap fixture starts with the native callback's projected actor and no pose");
        visual.begin_native_frame();require(f.get<unsigned short>(held+0xa6)==render,"sticky frame entry preserves the initially published actor");
        auto state=f.state(Phase::Idle,golden);state.frame.playing=!paused_first;state.frame.paused=paused_first;
        visual.update_held(state);
        require(parts()==5&&visual.held_rotor().owns(held),"owned render projection is canonicalized only inside attachment and receives all five modifiers");
        require(f.get<unsigned short>(held+0xa6)==render&&visual.assembly_pose().active,"bootstrap returns a complete logical model with its assembly active");
        const auto event=visual.poll_equip();
        require(paused_first?!event:(event&&event->actor==held),"bootstrap emits or defers exactly one actual equip event according to live mode");
        const auto head=f.get<unsigned>(held+0x64);
        const auto nodes=f.get<std::array<unsigned char,320>>(head-4*64);
        if(paused_first){
            for(unsigned i=0;i<3;++i){visual.begin_native_frame();visual.update_held(state);}
            require(parts()==5&&f.get<std::array<unsigned char,320>>(head-4*64)==nodes,"first-sight paused actor retains its attached frozen pose");
        }
        state.frame.playing=true;state.frame.paused=false;state.phase=Phase::Windup;
        for(unsigned i=0;i<6;++i){visual.begin_native_frame();visual.update_held(state);}
        require(parts()==5&&visual.controller().radial_scale()>1.5f&&f.get<Vec3>(head+0x20).x>1.5f,
                "native-order bootstrap still produces the real enlarged attack head");
        state.phase=Phase::Spinning;state.enemy_angle+=.6f;visual.begin_native_frame();visual.update_held(state);
        require(parts()==5&&f.get<Vec3>(head+0x20).x==1&&f.get<Vec3>(head+0x20).y==1&&!visual.assembly_pose().active,
                "capturing after bootstrap restores the normal-size synchronized drill");
        require(f.get<unsigned short>(held+0xa6)==render&&f.get<std::array<unsigned char,256>>(stock)==stock_record,
                "pose boundaries restore the custom draw identity and never touch a native Walloper");
    }
    for(bool pal:{false,true})for(bool golden:{false,true})for(auto phase:{Phase::Idle,Phase::Windup,Phase::Spinning}){
        Fixture f(pal);NativeWeaponVisual visual(&f.api,f.level_arena,f.held_arena);
        const auto stock_record=f.get<std::array<unsigned char,256>>(stock);
        const auto original_skeleton=f.get<std::array<unsigned char,320>>(skeleton);
        auto state=f.state(phase,golden);visual.begin_native_frame();visual.update_held(state);
        require(f.get<unsigned short>(held+0xa6)==(golden?2018:2017),"visual frame publishes a complete custom model before any possible pause transition");
        require(visual.publish_held(held,true,golden),"live render publishes the custom logical weapon");
        const unsigned render=golden?2018:2017,head=f.get<unsigned>(held+0x64);
        const auto frozen_parts=f.get<std::array<unsigned char,320>>(head-4*64);
        const float frozen_angle=visual.controller().angle(),frozen_scale=visual.controller().radial_scale();
        state.frame.playing=false;state.frame.paused=true;visual.set_mode(3);
        for(unsigned frame=0;frame<120;++frame){
            visual.begin_native_frame();
            require(f.get<unsigned short>(held+0xa6)==render,"frame entry retains the held model if pause begins before any native item callback");
            visual.update_held(state);
            require(f.get<unsigned short>(held+0xa6)==render&&visual.owns(held),"paused render keeps base or Gold identity without a combat callback");
            require(visual.controller().angle()==frozen_angle&&visual.controller().radial_scale()==frozen_scale,
                    "pause freezes the exact idle, enlarged or spinning rotor state");
            require(f.get<std::array<unsigned char,320>>(head-4*64)==frozen_parts,"paused presentation does not reset or advance any owned joint");
        }
        visual.begin_native_frame();state.frame.playing=true;state.frame.paused=false;state.enemy_angle+=.3f;
        visual.update_held(state);require(f.get<unsigned short>(held+0xa6)==render,"resume keeps the render model until a known native adapter needs its carrier");
        visual.prepare_native(held);require(f.get<unsigned short>(held+0xa6)==180,"native update boundary receives canonical carrier identity");
        require(visual.publish_held(held,true,golden)&&visual.controller().angle()!=frozen_angle,"resume advances existing rotor state once and republishes the logical model");
        require(f.get<std::array<unsigned char,256>>(stock)==stock_record&&f.get<std::array<unsigned char,320>>(skeleton)==original_skeleton,
                "custom visual lifecycle never edits another Walloper or the shared skeleton");
        state.frame.equipped=false;state.logical_equipped=false;visual.begin_native_frame();visual.update_held(state);
        require(f.get<unsigned short>(held+0xa6)==180&&f.get<unsigned>(held+0x64)==held+0x10000,
                "switching to the original weapon restores its unmodified model and native joint chain");
    }
    for(bool pal:{false,true}){
        Fixture f(pal);NativeWeaponVisual visual(&f.api,f.level_arena,f.held_arena);
        auto state=f.state(Phase::Windup,false);visual.begin_native_frame();visual.update_held(state);visual.publish_held(held,true,false);
        const float angle=visual.controller().angle(),scale=visual.controller().radial_scale();
        visual.set_mode(3);
        require(visual.publish_preview(NativeWeaponVisual::Preview::Standalone,standalone,true,false,.65f),"standalone model uses its own pose owner");
        require(visual.publish_preview(NativeWeaponVisual::Preview::Equipped,equipped,true,true),"Ratchet preview uses a separate authored-root pose owner");
        require(visual.controller().angle()==angle&&visual.controller().radial_scale()==scale&&f.get<unsigned short>(held+0xa6)==2017,
                "menu previews cannot reset, resize or recolor the frozen held weapon");
        const auto root=f.get<unsigned>(equipped+0x64)-4*64;
        require(f.get<unsigned char>(root+3)==0,"equipped preview retains the native wrist/root transform");
        const float fitted=f.get<float>(standalone+0x2c);
        visual.publish_preview(NativeWeaponVisual::Preview::Standalone,standalone,true,false,.65f);
        require(f.get<float>(standalone+0x2c)==fitted,"repeated preview draws never compound panel fitting");
        visual.prepare_native(equipped);require(f.get<unsigned short>(equipped+0xa6)==180,"native preview updates receive only their owned carrier projection");
        const auto stock_record=f.get<std::array<unsigned char,256>>(stock);visual.prepare_native(stock);
        require(f.get<std::array<unsigned char,256>>(stock)==stock_record,"foreign Walloper actors are never canonicalized or edited");
        visual.publish_preview(NativeWeaponVisual::Preview::Acquisition,scene,true,true);
        require(f.get<unsigned short>(scene+0xa6)==2018&&visual.controller().angle()==angle,"acquisition owns independent Gold identity without advancing gameplay");
        const auto scene_head=f.get<unsigned>(scene+0x64);
        const auto scene_parts=f.get<std::array<unsigned char,320>>(scene_head-4*64);
        visual.publish_preview(NativeWeaponVisual::Preview::Acquisition,stock,false,false);
        require(f.get<unsigned>(scene+0x64)==scene_head&&f.get<std::array<unsigned char,320>>(scene_head-4*64)==scene_parts&&
                f.get<unsigned short>(scene+0xa6)==2018&&f.get<std::array<unsigned char,256>>(stock)==stock_record,
                "an interleaved original Walloper cannot clear another acquisition actor's pending pose or render identity");
        visual.publish_preview(NativeWeaponVisual::Preview::Equipped,equipped,false,false);
        require(f.get<unsigned short>(equipped+0xa6)==180&&f.get<unsigned>(equipped+0x64)==equipped+0x10000&&
                f.get<unsigned>(scene+0x64)==scene_head,
                "switching the same preview actor back to Walloper removes its neutralizers without disturbing acquisition");
        visual.reset();
        for(unsigned actor:{held,standalone,equipped,scene})require(f.get<unsigned short>(actor+0xa6)==180&&f.get<unsigned>(actor+0x64)==actor+0x10000,
                "level teardown removes only the component's model and pose leases");
        require(f.get<unsigned char>(f.global(0x1b3ac0)+2017)==255&&f.get<unsigned char>(f.global(0x1b3ac0)+2018)==255,
                "level teardown restores both private render map slots");
    }
    for(bool pal:{false,true})for(bool golden:{false,true}){
        Fixture f(pal);NativeWeaponVisual visual(&f.api,f.level_arena,f.held_arena);
        auto state=f.state(Phase::Idle,golden);visual.begin_native_frame();visual.update_held(state);
        const auto event=visual.poll_equip();
        require(event&&event->actor==held&&!visual.poll_equip(),"a validated real equip emits its exact actor once");
        auto pose=visual.assembly_pose();
        require(pose.active&&pose.housing_scale<.1f&&pose.head_axial_scale<.1f&&pose.head_turn==0,
                "the actual held weapon begins with a compact housing and retracted drill before the motor engages");
        visual.publish_held(held,true,golden);state.frame.playing=false;state.frame.paused=true;
        for(unsigned i=0;i<30;++i){
            visual.begin_native_frame();visual.update_held(state);const auto paused=visual.assembly_pose();
            require(paused.housing_scale==pose.housing_scale&&paused.head_axial_scale==pose.head_axial_scale&&paused.head_turn==pose.head_turn,
                    "pause freezes the complete assembly envelope rather than restarting it");
            require(!visual.poll_equip(),"pause and repeated paused render publication never repeat the equip event");
        }
        state.frame.playing=true;state.frame.paused=false;
        for(unsigned i=0;i<assembly_frames;++i){
            visual.begin_native_frame();visual.update_held(state);visual.publish_held(held,true,golden);
            require(!visual.poll_equip(),"live frames complete assembly without replaying sound events");
            if(i==6)require(visual.assembly_pose().active&&visual.assembly_pose().head_turn>0,
                    "the held drill engages its screw rotation after the motor housing appears");
        }
        require(!visual.assembly_pose().active&&visual.assembly_pose().housing_scale==1&&visual.assembly_pose().head_axial_scale==1,
                "the finite assembly ends at exact normal geometry");
        const auto next_stamp=f.get<unsigned>(held+0x38)+1;f.actor(held);f.put(held+0x38,next_stamp);
        visual.begin_native_frame();visual.update_held(state);
        require(visual.poll_equip().has_value()&&visual.assembly_pose().active,"a genuinely recreated held actor starts one new assembly");
        state.phase=Phase::Windup;visual.update_held(state);
        require(!visual.assembly_pose().active&&visual.assembly_pose().housing_scale==1&&visual.assembly_pose().head_axial_scale==1,
                "the first real attack immediately gets full size without waiting for presentation");
        state.frame.equipped=false;state.logical_equipped=false;visual.update_held(state);require(!visual.poll_equip(),"switching away cancels any pending equip event");
    }
    {
        Fixture f(true);NativeWeaponVisual visual(&f.api,f.level_arena,f.held_arena);
        auto state=f.state(Phase::Idle,false);state.frame.playing=false;state.frame.paused=true;state.ammunition=0;
        visual.begin_native_frame();visual.update_held(state);require(!visual.poll_equip(),"equipping while paused defers audio until gameplay");
        require(visual.assembly_pose().head_turn==0,"an empty drill does not screw or spin while assembled");
        visual.publish_preview(NativeWeaponVisual::Preview::Equipped,equipped,true,false);
        visual.publish_preview(NativeWeaponVisual::Preview::Acquisition,scene,true,false);
        require(!visual.poll_equip(),"inventory and acquisition previews never produce a gameplay equip event");
        state.frame.playing=true;state.frame.paused=false;visual.update_held(state);
        const auto event=visual.poll_equip();require(event&&event->actor==held&&!visual.poll_equip(),"a deferred actual equip emits once on resume");
        for(unsigned i=0;i<assembly_frames;++i){visual.begin_native_frame();visual.update_held(state);visual.publish_held(held,true,false);}
        require(!visual.assembly_pose().active&&visual.assembly_pose().head_turn==0&&visual.controller().angle()==0,
                "zero ammunition remains stationary through and after the complete intro");
        state.frame.equipped=false;state.logical_equipped=false;visual.update_held(state);
        state.frame.equipped=true;state.logical_equipped=true;state.frame.playing=false;state.frame.paused=true;
        visual.begin_native_frame();visual.update_held(state);visual.reset();require(!visual.poll_equip(),"reset cancels a paused pending cue without leaving stale native actors");
    }
    for(bool pal:{false,true})for(bool golden:{false,true})for(bool initial_pending:{false,true}){
        Fixture f(pal);NativeWeaponVisual visual(&f.api,f.level_arena,f.held_arena);
        auto state=f.state(Phase::Idle,golden);
        state.frame.playing=!initial_pending;state.frame.paused=initial_pending;
        visual.begin_native_frame();visual.update_held(state);
        if(initial_pending)require(!visual.poll_equip(),"an initial paused equip still owns one deferred cue");
        else{
            require(visual.poll_equip().has_value(),"live equip cue is consumed before visiting the inventory");
            for(unsigned i=0;i<assembly_frames;++i){visual.begin_native_frame();visual.update_held(state);}
        }
        state.frame.playing=false;state.frame.paused=true;visual.begin_native_frame();visual.update_held(state);
        const auto paused=visual.assembly_pose();
        constexpr unsigned temporary=0x200500,rebound=0x200600;
        f.actor(temporary);f.put<unsigned short>(temporary+0xa6,0);
        const auto foreign=f.get<std::array<unsigned char,256>>(temporary);
        for(unsigned i=0;i<3;++i){
            state.actor=i?temporary:0;state.frame.equipped=false;
            visual.begin_native_frame();visual.update_held(state);
            const auto current=visual.assembly_pose();
            require(current.active==paused.active&&current.housing_scale==paused.housing_scale&&
                    current.head_axial_scale==paused.head_axial_scale&&current.head_turn==paused.head_turn&&!visual.poll_equip(),
                    "missing or class0 pause actors preserve semantic equip and the exact paused envelope");
            require(f.get<std::array<unsigned char,256>>(temporary)==foreign,"pause body/preview placeholders never receive weapon identity or rotor modifiers");
        }
        f.actor(rebound);state.actor=rebound;state.frame.equipped=true;state.frame.playing=true;state.frame.paused=false;
        visual.begin_native_frame();visual.update_held(state);
        const auto resumed=visual.assembly_pose();const auto event=visual.poll_equip();
        require(visual.held_rotor().owns(rebound)&&f.get<unsigned short>(rebound+0xa6)==(golden?2018:2017)&&
                f.get<unsigned>(rebound+0x64)!=rebound+0x10000,
                "same logical weapon rebinds a rebuilt native actor with its complete pose and custom model");
        require(initial_pending?(event&&event->actor==rebound&&!visual.poll_equip()):!event,
                "inventory actor recreation never adds a cue; an initial deferred cue follows the new actor once");
        require(initial_pending?(resumed.active&&resumed.head_axial_scale>=paused.head_axial_scale):!resumed.active,
                "same-weapon resume continues a pending intro or stays assembled without replaying it");
        state.frame.playing=false;state.frame.paused=true;visual.begin_native_frame();visual.update_held(state);
        state.logical_equipped=false;state.frame.equipped=false;visual.update_held(state);
        require(!visual.assembly_pose().active&&!visual.poll_equip(),"an actual different logical selection in the menu cancels the old equip lifecycle");
        state.logical_equipped=true;state.frame.equipped=true;visual.begin_native_frame();visual.update_held(state);
        require(visual.assembly_pose().active&&!visual.poll_equip(),"selecting Rotatore again in the menu is a real new equip with a deferred cue");
        state.frame.playing=true;state.frame.paused=false;visual.begin_native_frame();visual.update_held(state);
        require(visual.poll_equip().has_value()&&!visual.poll_equip(),"an actual menu re-equip emits exactly one cue on resume");
        state.frame.equipped=false;visual.update_held(state);state.frame.equipped=true;
        visual.begin_native_frame();visual.update_held(state);
        require(visual.assembly_pose().active&&visual.poll_equip().has_value(),"a real live-gameplay physical unequip retains its normal new-equip behavior");
    }
    std::printf("weapon visual: %u paused-render, ownership and lifecycle checks passed\n",checks);
}
