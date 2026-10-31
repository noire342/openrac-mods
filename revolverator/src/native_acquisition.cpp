// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_acquisition.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace revolverator {
namespace {
constexpr unsigned Hero=0x13f350,Equipment=0x141660,ItemInHand=0x140408;
constexpr unsigned BufferBytes=0x120;
template<std::size_t N>bool finite(const std::array<float,N>& values){
    return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});
}
}
NativeAcquisition::NativeAcquisition(const openrac_api* api,openrac_mod* mod,GuestArena& arena,Context context)
    :Memory(api),mod_(mod),arena_(arena),context_(context){}
NativeAcquisition::~NativeAcquisition(){reset();}
bool NativeAcquisition::request()noexcept{
    if(requests_>=maximum_pending)return false;
    ++requests_;return true;
}
bool NativeAcquisition::supported()const{
    return api_&&api_->memory&&api_->loaded_level&&api_->relocate_data&&api_->relocate_level_data&&api_->function&&
        api_->game&&api_->version&&!std::strcmp(api_->game,"rac1")&&RegionBindings::for_version(api_->version)==&bindings()&&
        api_->loaded_level()>=0&&api_->loaded_level()<=18;
}
bool NativeAcquisition::same_level()const{
    return supported()&&api_->loaded_level()==level_&&hero_==global(Hero)&&
        mode_==level(0x15f5c4)&&vendor_==level(1,0x1ca940)&&
        arena_.owns(buffer_,BufferBytes);
}
bool NativeAcquisition::bindings_ready(){
    present_=reinterpret_cast<Present>(find_function("FUN_L01_002aea70"));
    place_=reinterpret_cast<Place>(find_function("FUN_L00_00216f90"));
    hero_=global(Hero);mode_=level(0x15f5c4);
    vendor_=level(1,0x1ca940);scene_=level(1,0x16cce0);
    const auto scenes=level(1,0x1ca4a0);
    if(!present_||!place_||!valid(hero_,0x22d8)||!valid(mode_,4)||!valid(vendor_,vendor_snapshot_.size())||
       !valid(scene_,0x1c0)||!valid(scenes+18*4,4)||read<int>(scenes+18*4)<0||
       !valid(global(Equipment),sizeof equipment_)||!valid(global(ItemInHand),4))return false;
    if(!arena_.owns(buffer_,BufferBytes))buffer_=arena_.allocate(BufferBytes);
    return arena_.owns(buffer_,BufferBytes)&&valid(buffer_,BufferBytes);
}
bool NativeAcquisition::body_valid()const{
    return same_level()&&body_.address&&valid(body_.address,256)&&read<unsigned>(hero_+0x2080)==body_.address&&
        read<unsigned>(body_.address+0x24)==body_.pclass&&read<unsigned>(body_.address+0x78)==body_.pvars&&
        read<unsigned>(body_.address+0x38)==body_.stamp&&read<unsigned short>(body_.address+0xa6)==0&&
        read<unsigned char>(body_.address+0x20)<0x80;
}
bool NativeAcquisition::snapshot_ready()const{
    const auto body=read<unsigned>(hero_+0x2080);
    const int state=read<int>(hero_+0x2084);
    return valid(body,256)&&read<unsigned short>(body+0xa6)==0&&read<unsigned char>(body+0x20)<0x80&&
        read<int>(hero_+0x22a8)>0&&(state==0||state==1||state==2)&&
        finite(read<std::array<float,4>>(hero_+0x80))&&finite(read<std::array<float,4>>(hero_+0x90))&&
        finite(read<std::array<float,4>>(hero_+0xe0));
}
bool NativeAcquisition::quiet_player()const{
    const auto velocity=read<std::array<float,4>>(hero_+0xe0);
    const int held=read<int>(global(ItemInHand)),pending=read<int>(hero_+0x20b8);
    return !read<unsigned char>(hero_+0x20a8)&&finite(velocity)&&
        velocity[0]*velocity[0]+velocity[1]*velocity[1]<=.0001f&&(pending==0||pending==held);
}
void NativeAcquisition::frame(bool safe,bool external_active){
    if(!supported()){reset();return;}
    const int current=api_->loaded_level();
    if(current!=level_){if(level_>=0)reset();level_=current;}
    if(presenting_){
        if(!same_level()){reset();return;}
        pose_items();hide_player();
        if(read<int>(mode_)!=0){settled_=0;return;}
        // This is a native update barrier, not a wall-clock delay. Retain two
        // completed updates on both discs before actors retire. The host may
        // run PAL at60 updates; disc timing is not an update-rate assumption.
        if(++settled_<2)return;
        finish(true);return;
    }
    if(!requests_)return;
    if(!bindings_ready()){
        preparing_=false;quiet_=0;
        if(!reported_&&api_->log){api_->log(mod_,OPENRAC_LOG_WARN,"Revolverator acquisition awaits native scene18 bindings and reserved level memory");reported_=true;}
        return;
    }
    if(!safe||external_active||read<int>(mode_)!=0||!snapshot_ready()){
        preparing_=false;quiet_=0;return;
    }
    if(cooldown_seconds_>0){
        const auto seconds=read<float>(global(0x15ed7c));
        if(std::isfinite(seconds)&&seconds>0&&seconds<=.1f)
            cooldown_seconds_=seconds>=cooldown_seconds_-1.e-5f?0.f:cooldown_seconds_-seconds;
        return;
    }
    // Neutral input lets walking and held attacks finish before snapshotting.
    if(!preparing_){preparing_=true;quiet_=0;return;}
    if(!quiet_player()){quiet_=0;return;}
    if(++quiet_<2)return;
    begin();
}
void NativeAcquisition::begin(){
    const auto body=read<unsigned>(hero_+0x2080);
    body_={body,read<unsigned>(body+0x24),read<unsigned>(body+0x78),read<unsigned>(body+0x38)};
    position_=read<decltype(position_)>(hero_+0x80);rotation_=read<decltype(rotation_)>(hero_+0x90);
    velocity_=read<decltype(velocity_)>(hero_+0xe0);
    equipment_=read<decltype(equipment_)>(global(Equipment));pending_items_=read<decltype(pending_items_)>(hero_+0x20b8);
    previous_item_=read<int>(global(ItemInHand));
    lock_a_=read<unsigned char>(hero_+0x20a5);lock_b_=read<unsigned char>(hero_+0x20ac);
    std::memcpy(vendor_snapshot_.data(),api_->memory+vendor_,vendor_snapshot_.size());
    std::memcpy(api_->memory+buffer_,api_->memory+body,256);
    const auto camera_at=level(0x166c80)+0x140;
    const auto camera=read<std::array<float,3>>(camera_at);
    preparing_=false;presenting_=true;settled_=0;
    try{
        projected_=true;if(context_.project)context_.project(context_.user);
        // The native item remains18. Context owns the logical Revo/Gold name,
        // model and metadata; the scene contains only native safe item IDs.
        present_(buffer_,18);
        if(!same_level()){reset();return;}
        align_scene(camera);pose_items();hide_player();
    }catch(...){finish(false);throw;}
}
void NativeAcquisition::hide_player(){
    if(!same_level())return;
    write<unsigned char>(hero_+0x20a5,1);write<unsigned char>(hero_+0x20ac,1);
    if(!body_valid())return;
    const int count=read<short>(scene_+0x44);
    if(count<1||count>14)return;
    bool separate=false;
    for(int i=0;i<count;++i){
        const auto actor=read<unsigned>(scene_+0x178+unsigned(i)*4);
        // A scene may reuse the real body. Hiding it would hide the animation.
        if(actor==body_.address){restore_body();return;}
        separate|=valid(actor,256)&&read<unsigned short>(actor+0xa6)==0&&read<unsigned char>(actor+0x20)<0x80;
    }
    if(!separate)return;
    if(!hidden_){body_flags_=read<unsigned short>(body_.address+0x34);hidden_=true;}
    write<unsigned short>(body_.address+0x34,read<unsigned short>(body_.address+0x34)|1u);
}
void NativeAcquisition::pose_items(){
    if(!same_level()||!context_.pose_item)return;
    const int count=read<short>(scene_+0x44);
    if(count<1||count>14)return;
    for(int i=0;i<count;++i){
        const auto actor=read<unsigned>(scene_+0x178+unsigned(i)*4);
        if(valid(actor,256)&&read<unsigned short>(actor+0xa6)==180&&read<unsigned char>(actor+0x20)<0x80)
            context_.pose_item(context_.user,actor);
    }
}
void NativeAcquisition::restore_body()noexcept{
    if(hidden_&&body_valid()){
        const auto flags=read<unsigned short>(body_.address+0x34);
        write<unsigned short>(body_.address+0x34,static_cast<unsigned short>((flags&~1u)|(body_flags_&1u)));
    }
    hidden_=false;
}
void NativeAcquisition::align_scene(const std::array<float,3>& camera){
    if(!same_level()||!finite(camera))return;
    const auto camera_keys=read<unsigned>(scene_+0x54);const int count=read<short>(scene_+0x44);
    if(!valid(camera_keys,32)||count<1||count>14)return;
    unsigned keys=0;
    for(int i=0;i<count;++i){const auto actor=read<unsigned>(scene_+0x178+unsigned(i)*4);
        if(valid(actor,256)&&read<unsigned short>(actor+0xa6)==0){keys=read<unsigned>(actor+0x78);break;}}
    if(!valid(keys,16))return;
    const auto actor_position=read<std::array<float,3>>(keys),view=read<std::array<float,3>>(camera_keys);
    if(!finite(actor_position)||!finite(view))return;
    const float dx=view[0]-actor_position[0],dy=view[1]-actor_position[1];
    if(dx*dx+dy*dy<.01f)return;
    const float yaw=std::atan2(camera[1]-position_[1],camera[0]-position_[0])-std::atan2(dy,dx);
    const float c=std::cos(yaw),s=std::sin(yaw);
    const std::array<float,16> basis{{c,s,0,0,-s,c,0,0,0,0,1,0,0,0,yaw,0}};
    write(vendor_+0x80,basis);
    write(vendor_+0xc0,std::array<float,4>{{position_[0]-(c*actor_position[0]-s*actor_position[1]),
        position_[1]-(s*actor_position[0]+c*actor_position[1]),position_[2]-actor_position[2],0}});
}
void NativeAcquisition::restore_context()noexcept{
    const bool owned=projected_;projected_=false;
    if(owned&&context_.restore)try{context_.restore(context_.user);}catch(...){}
}
void NativeAcquisition::finish(bool place_player)noexcept{
    if(same_level()){
        if(place_player&&body_valid()&&read<int>(mode_)==0){
            write(buffer_+0x100,position_);write(buffer_+0x110,rotation_);
            try{place_(buffer_+0x100,buffer_+0x110,0,1);}catch(...){}
            write(hero_+0xe0,velocity_);
            write(global(Equipment),equipment_);
            auto pending=pending_items_;
            // Native scene consumes pending18. Request the previously held item
            // again through native switching; never restore stale moby pointers.
            if(!pending[0])pending[0]=previous_item_;
            write(hero_+0x20b8,pending);
        }
        restore_body();
        if(read<unsigned char>(hero_+0x20a5)==1)write(hero_+0x20a5,lock_a_);
        if(read<unsigned char>(hero_+0x20ac)==1)write(hero_+0x20ac,lock_b_);
        // A later foreign vendor/scene retains its own state.
        if(read<unsigned>(vendor_+0x28)==buffer_)std::memcpy(api_->memory+vendor_,vendor_snapshot_.data(),vendor_snapshot_.size());
    }else hidden_=false;
    presenting_=preparing_=false;quiet_=settled_=0;body_={};
    if(requests_)--requests_;
    cooldown_seconds_=.5f;
    restore_context();
}
void NativeAcquisition::filter_report(unsigned char* report,int size)const noexcept{
    if(!active()||!report||size<6)return;
    report[0]=report[1]=0xff;
    std::fill(report+2,report+6,static_cast<unsigned char>(128));
    if(size>6)std::fill(report+6,report+std::min(size,18),static_cast<unsigned char>(0));
}
void NativeAcquisition::reset()noexcept{
    if(presenting_)finish(false);else restore_context();
    requests_=quiet_=settled_=0;cooldown_seconds_=0;preparing_=presenting_=hidden_=reported_=false;
    level_=-1;buffer_=hero_=vendor_=scene_=mode_=0;body_={};present_=nullptr;place_=nullptr;
}
}
