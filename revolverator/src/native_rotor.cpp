// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_rotor.hpp"
#include <cmath>
#include <cstring>

namespace revolverator {
namespace {
constexpr std::array<float,4> identity{0,0,0,1};
// Original Hoven class164: rigid corkscrew component (source joint35). These
// values are recorded by tools/build_model_pack.py in rotor.json as well.
constexpr Vec3 source_pivot{.16387811303138733f,0,2.4493489265441895f};
constexpr float source_scale=.08294187486171722f;
Vec3 turn_minus_y(Vec3 p,float angle){
    const float c=std::cos(angle),s=std::sin(angle);
    return {c*p.x-s*p.z,p.y,s*p.x+c*p.z};
}
bool unit_rotation(const std::array<float,16>& m){
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
        if(!std::isfinite(m[r*4+c])||std::abs(m[r*4+c]-(r==c?1.f:0.f))>.0001f)return false;
    return true;
}
}
NativeRotor::NativeRotor(const openrac_api* api,GuestArena& arena,bool hud_fallback):Memory(api),guest_(arena),hud_fallback_(hud_fallback){}
NativeRotor::~NativeRotor(){reset();}
bool NativeRotor::attached()const{
    return owner_&&api_->loaded_level()==level_&&valid(owner_,256)&&
        read<unsigned>(owner_+0x24)==klass_&&read<unsigned>(owner_+0x38)==stamp_&&
        read<unsigned>(owner_+0x78)==vars_;
}
bool NativeRotor::attach(unsigned weapon){
    if(weapon==owner_&&attached())return true;
    reset();
    if(!valid(weapon,256)||read<unsigned short>(weapon+0xa6)!=180)return false;
    const auto klass=read<unsigned>(weapon+0x24),skeleton=read<unsigned>(klass+0x14);
    if(!valid(klass,48)||read<unsigned char>(klass+8)!=5||!valid(skeleton,5*64))return false;
    const float scale=read<float>(klass+0x24);
    if(!std::isfinite(scale)||scale<=0)return false;
    const auto root_bind=read<std::array<float,16>>(skeleton),head_bind=read<std::array<float,16>>(skeleton+4*64);
    if(!unit_rotation(root_bind)||!unit_rotation(head_bind))return false;
    if(std::abs(root_bind[12])+std::abs(root_bind[13])+std::abs(root_bind[14])>.001f)return false;
    inverse_bind_offset_={head_bind[12],head_bind[13],head_bind[14]};
    if(!finite(inverse_bind_offset_))return false;
    pivot_=source_pivot*(source_scale/scale*1024.f);
    if(arena_level_!=api_->loaded_level()){arena_=0;arena_level_=api_->loaded_level();}
    if(!arena_)arena_=guest_.allocate(5*64);
    if(!arena_&&hud_fallback_){
        const auto state=global(0x19a3e8),cursor=read<unsigned>(state+0x10),end=read<unsigned>(state+0x14);
        if(valid(cursor,5*64)&&end>=cursor&&end<=0x2000000&&end-cursor>=5*64+16)
            arena_=function<unsigned(*)(unsigned)>("FUN_001ff288")(5*64);
    }
    if(!valid(arena_,5*64)){arena_=0;return false;}
    std::memset(api_->memory+arena_,0,5*64);
    owner_=weapon;klass_=klass;stamp_=read<unsigned>(weapon+0x38);
    vars_=read<unsigned>(weapon+0x78);level_=api_->loaded_level();
    for(unsigned joint=0;joint<5;++joint){
        const auto node=arena_+joint*64;
        write<unsigned char>(node+1,1);write<unsigned char>(node+3,1);
        write(node+4,0x70000000u+joint*64);write(node+8,read<unsigned>(weapon+0x64));
        write<float>(node+0xc,1.f);write(node+0x10,identity);write(node+0x20,Vec3{1,1,1});
        write(weapon+0x64,node);
    }
    return true;
}
void NativeRotor::update(unsigned weapon,int ammunition,Phase phase,float charge,float enemy_angle,
                         float seconds,bool playing,bool paused){
    const bool dry_request=pending_dry_;pending_dry_=false;
    if(!attach(weapon))return;
    if(dry_request)controller_.dry_fire();
    controller_.update({ammunition,phase,charge,enemy_angle,seconds,playing,paused});
    apply();
}
void NativeRotor::apply(){
    if(!attached())return;
    // Modes1 replace the native Walloper jab's five local records. Body geometry
    // is rigid on root0, so no collar, handle or motor housing can rotate.
    for(unsigned joint=0;joint<5;++joint){
        const auto node=arena_+joint*64;
        write(node+0x10,identity);write(node+0x20,Vec3{1,1,1});write(node+0x30,Vec3{});
    }
    const float angle=controller_.angle();
    const auto head=arena_+4*64;
    // Native joint quaternions use conjugated axis images. Positive native Y
    // therefore rotates the head about its outward -Y shaft, matching +Z enemy
    // rotation when the hand holds the drill upright.
    write(head+0x10,std::array<float,4>{0,std::sin(angle*.5f),0,std::cos(angle*.5f)});
    // The port evaluates the guest Part's P4*S4, not the GLB's bind matrices.
    // Choose P4 translation so the effective palette is T(pivot)*R*T(-pivot),
    // without changing the shared native skeleton or importer-compatible IBMs.
    write(head+0x30,pivot_-turn_minus_y(pivot_+inverse_bind_offset_,angle));
}
void NativeRotor::unlink(unsigned node){
    unsigned link=owner_+0x64;
    for(unsigned count=0;count<64;++count){
        const auto at=read<unsigned>(link);
        if(at==node){write(link,read<unsigned>(node+8));return;}
        if(!valid(at,64))return;
        link=at+8;
    }
}
void NativeRotor::reset(bool discard){
    if(attached())for(unsigned joint=0;joint<5;++joint)unlink(arena_+joint*64);
    owner_=klass_=stamp_=vars_=0;level_=-1;controller_.reset();
    pending_dry_=false;
    pivot_=inverse_bind_offset_={};
    if(discard){arena_=0;arena_level_=-1;}
}
}
