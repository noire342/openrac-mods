// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_gold_vendor.hpp"
#include "localization.hpp"
#include "gold_shop_layout.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace revolverator {
namespace {
constexpr unsigned Hero=0x13f350,Bolts=0x15ed98,Challenge=0x15ee20;
// Continuation of the native Novalis Gold room's ring, on its existing floor.
constexpr std::array<float,4> Position{{250.8f,123.6f,54.4966202f,1.f}};
// The complete2.22m rigid display is centered at the native Gold-room height.
constexpr auto DisplayPosition=gold_shop_layout::display_origin(Position);
constexpr std::array<unsigned,3> PromptCallbacks{{0x24c828,0x24c878,0x24c898}};
}
NativeGoldVendor::NativeGoldVendor(const openrac_api* api,openrac_mod* mod,
    arsenal::Registry& registry,NativeWeaponAlias&):Memory(api),registry_(registry),mod_(mod){}
NativeGoldVendor::~NativeGoldVendor(){reset();}
bool NativeGoldVendor::supported()const{
    return api_&&api_->memory&&api_->loaded_level&&api_->relocate_data&&api_->relocate_level_data&&
        api_->function&&api_->function_at&&api_->game&&api_->version&&
        !std::strcmp(api_->game,"rac1")&&RegionBindings::for_version(api_->version)==&bindings()&&api_->loaded_level()==1;
}
bool NativeGoldVendor::accessible()const{return supported()&&read<int>(global(Challenge))>0;}
bool NativeGoldVendor::valid_actor(const Actor& actor,bool dome)const{
    if(!actor.address||!supported()||level_!=api_->loaded_level()||!valid(actor.address,256)||
        !valid(actor.pvars,1)||read<unsigned char>(actor.address+0x20)!=3||
        read<unsigned>(actor.address+0x24)!=actor.pclass||read<unsigned>(actor.address+0x78)!=actor.pvars||
        read<unsigned>(actor.address+0x38)!=actor.stamp)return false;
    const auto klass=read<unsigned short>(actor.address+0xa6);
    return dome?klass==case_class:(klass==case_class||klass==display_class);
}
unsigned NativeGoldVendor::dome_moby()const{return valid_actor(dome_,true)?dome_.address:0;}
unsigned NativeGoldVendor::preview_moby()const{return valid_actor(preview_,false)?preview_.address:0;}
bool NativeGoldVendor::ready()const{return accessible()&&dome_moby()!=0;}
bool NativeGoldVendor::resource_available(unsigned klass)const{
    const auto slot=read<unsigned char>(global(0x1b3ac0)+klass);
    return slot<224&&slot<read<int>(global(0x15ff00))&&
        read<unsigned short>(global(0x1b3900)+unsigned(slot)*2)==klass&&
        valid(read<unsigned>(global(0x1b3200)+unsigned(slot)*4),0x4c);
}
bool NativeGoldVendor::display_mapping(){
    if(!resource_available(case_class))return false;
    const auto map=global(0x1b3ac0);
    if(!valid(map,display_class+1))return false;
    const auto source_slot=read<unsigned char>(map+case_class);
    if(owns_display_map_)
        return display_map_==map&&display_slot_==source_slot&&read<unsigned char>(map+display_class)==display_slot_;
    if(read<unsigned char>(map+display_class)!=0xff)return false;
    display_map_=map;display_slot_=source_slot;owns_display_map_=true;
    write<unsigned char>(map+display_class,source_slot);return true;
}
bool NativeGoldVendor::bind_functions(){
    create_=reinterpret_cast<Create>(find_function("FUN_0020c4f8"));
    remove_=reinterpret_cast<Remove>(find_function("FUN_0020c828"));
    basis_=reinterpret_cast<Basis>(find_function("FUN_001fa030"));
    queue_=reinterpret_cast<Queue>(find_function("FUN_001ff308"));
    keep_=reinterpret_cast<Keep>(find_function("FUN_L00_00235e18"));
    autosave_=reinterpret_cast<Autosave>(find_function("FUN_0020b178"));
    const char* names[]{"FUN_L00_00237190","FUN_L00_002371e0","FUN_L00_00237200"};
    for(unsigned i=0;i<3;++i){
        const auto named=find_function(names[i]),entry=api_->function_at(guest_callback(1,PromptCallbacks[i]));
        // The middle fade function also has a hookable moby alias. The SDK
        // returns that hook's thunk by name, whereas the HUD stores the guest
        // entry address. Both are valid; pointer equality is not its identity.
        if(!named||!entry||(i!=1&&entry!=named))return false;
    }
    return create_&&remove_&&basis_&&queue_&&keep_&&autosave_;
}
NativeGoldVendor::Actor NativeGoldVendor::spawn(unsigned klass,const std::array<float,4>& pos,float yaw,bool dome){
    const auto address=create_(int(klass));
    if(!valid(address,256))return {};
    Actor actor{address,read<unsigned>(address+0x24),read<unsigned>(address+0x78),read<unsigned>(address+0x38)};
    if(!valid(actor.pclass,0x4c)||!valid(actor.pvars,1)||read<unsigned char>(address+0x20)>=0xfe){remove_(address);return {};}
    // Ownership stays in the host token. Native pvars and the parent pointer
    // retain their native semantics; a self-parent would cycle bolt-source walks.
    write<unsigned char>(address+0x20,3); // Native case's safe, non-selling idle.
    write(address+0x10,pos);
    const std::array<float,4> rotation{{0,0,yaw,0}};write(address+0x40,rotation);
    std::array<float,4> sphere=pos;for(unsigned i=0;i<3;++i)sphere[i]*=1024.f;sphere[3]=2048.f;write(address,sphere);
    write<unsigned char>(address+0x30,0xff);write<unsigned char>(address+0x31,1);
    write<short>(address+0x32,0x7fff);write<unsigned char>(address+0xb0,0xff);write<unsigned short>(address+0xb2,0xffff);
    if(!dome){write<unsigned>(address+0x74,0);write<unsigned>(address+0x94,0);write<int>(address+0x98,1);}
    inherit_lighting(actor,dome);
    basis_(address+0xc0,address+0x40);return actor;
}
bool NativeGoldVendor::inherit_lighting(Actor& actor,bool dome){
    const auto first=read<unsigned>(global(0x15ff18)),last=read<unsigned>(global(0x15ff20));
    if(!valid(first,256)||last<=first||last-first>4096*256||(last-first)%256||!valid(last-256,256))return false;
    const auto origin=read<std::array<float,4>>(actor.address+0x10);
    const auto nearest=[&](bool cases){
        unsigned selected=0;float distance=1e30f;
        for(auto at=first;at<last;at+=256){
            if(at==actor.address||at==dome_.address||at==preview_.address||read<unsigned char>(at+0x20)>=0x80)continue;
            const auto klass=read<unsigned short>(at+0xa6);
            if(cases?klass!=case_class:(klass<1456||klass>1465))continue;
            const auto point=read<std::array<float,4>>(at+0x10);
            const float x=point[0]-origin[0],y=point[1]-origin[1],z=point[2]-origin[2],d=x*x+y*y+z*z;
            if(std::isfinite(d)&&d<distance){selected=at;distance=d;}
        }
        return selected;
    };
    auto source=nearest(dome);if(!source&&!dome)source=nearest(true);
    if(!source)return false;
    // Alive mobys use+38 for the light-set pair/cross-fade and+3C for ambient
    // RGB. +B0 is a mission/save index, not a lighting field: keep it untouched.
    write(actor.address+0x38,read<std::array<unsigned,2>>(source+0x38));
    write<unsigned char>(actor.address+0x73,read<unsigned char>(source+0x73));
    write(actor.address+0x90,read<unsigned>(source+0x90));
    const auto mode=read<unsigned short>(actor.address+0x34),native_mode=read<unsigned short>(source+0x34);
    write<unsigned short>(actor.address+0x34,static_cast<unsigned short>((mode&~0x10u)|(native_mode&0x10u)));
    actor.stamp=read<unsigned>(actor.address+0x38);
    return true;
}
void NativeGoldVendor::retire(Actor& actor,bool dome){
    if(valid_actor(actor,dome)&&remove_){
        if(!dome){
            const auto render=read<unsigned short>(actor.address+0xa6);
            if(render==display_class)write<unsigned short>(actor.address+0xa6,case_class);
        }
        remove_(actor.address);
    }
    actor={};
}
void NativeGoldVendor::frame(){
    const int current=api_&&api_->loaded_level?api_->loaded_level():-1;
    if(current!=level_){reset();level_=current;}
    if(!accessible()){
        if(dome_.address||preview_.address){retire(preview_,false);retire(dome_,true);}return;
    }
    ++frames_;
    if(read<int>(level(0x15f5c4))!=0)return;
    functions_ready_=bind_functions();
    if(!functions_ready_||!resource_available(case_class)){
        if(!reported_&&api_->log){api_->log(mod_,OPENRAC_LOG_WARN,"Golden shop unavailable: native Novalis resources/prompt bindings are not ready");reported_=true;}return;
    }
    if(!valid_actor(dome_,true)){
        dome_={};if(frames_<retry_)return;
        dome_=spawn(case_class,Position,.05f,true);retry_=frames_+60;
        if(!dome_.address)return;
    }
    if(registry_.golden(arsenal::RevolveratorKey)){retire(preview_,false);return;}
    // This static renderer identity shares only the always-resident case304
    // resource. It has no skin/joints and never borrows a held weapon buffer.
    if(!display_mapping()){retire(preview_,false);return;}
    if(!valid_actor(preview_,false)){
        preview_={};preview_=spawn(case_class,DisplayPosition,gold_shop_layout::display_yaw,false);
    }
    if(valid_actor(preview_,false)){
        write(preview_.address+0x10,DisplayPosition);
        write(preview_.address+0x40,std::array<float,4>{{0,0,gold_shop_layout::display_yaw,0}});
        basis_(preview_.address+0xc0,preview_.address+0x40);
        write<float>(preview_.address+0x2c,gold_shop_layout::display_scale);
        write<unsigned short>(preview_.address+0xa6,display_class);
    }
}
bool NativeGoldVendor::player_near()const{
    const auto hero=global(Hero);
    if(read<int>(level(0x15f5c4))!=0||read<int>(hero+0x22a8)<=0)return false;
    const int state=read<int>(hero+0x2084);
    if(state==0x3d||state==0x82||state==0x77)return false;
    const auto position=read<std::array<float,4>>(hero+0x80);
    const float x=position[0]-Position[0],y=position[1]-Position[1],z=position[2]-Position[2];
    return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&
        x*x+y*y<gold_shop_layout::interaction_radius*gold_shop_layout::interaction_radius&&std::abs(z)<2.5f;
}
void NativeGoldVendor::prompt(const char* text){
    // Native channel12 draws the overlay's own 128-byte array. The similarly
    // named resident shared-text buffer is a separate, unrelated 64-byte array.
    const auto buffer=level(0x17e530);if(!valid(buffer,128))return;
    std::snprintf(reinterpret_cast<char*>(api_->memory+buffer),128,"%s",text);
    if(keep_(prompt_,10))prompt_=queue_(12,0,guest_callback(1,PromptCallbacks[0]),guest_callback(1,PromptCallbacks[1]),guest_callback(1,PromptCallbacks[2]),dome_.address,0);
}
bool NativeGoldVendor::update(unsigned moby){
    if(moby&&moby==preview_moby())return true;
    const auto own=dome_moby();if(!own||moby!=own)return false;
    if(!accessible()||!functions_ready_||!player_near())return true;
    const bool italian=localization::language_from_native(read<int>(global(localization::native_language_address)))==localization::Language::Italian;
    const auto* state=registry_.custom_state(arsenal::RevolveratorKey);
    if(!state||!state->owned){prompt(italian?"Serve il Rotatore per acquistare il potenziamento d'oro.":"Own the Revolverator before buying its Gold upgrade.");return true;}
    if(state->golden){prompt(italian?"Rotatore d'oro gia acquistato.":"Gold Revolverator already purchased.");return true;}
    const auto balance_at=global(Bolts);if(!valid(balance_at,4))return true;
    int balance=read<int>(balance_at);
    if(balance<int(price)){prompt(italian?"Rotatore d'oro: servono 50.000 bolt.":"Gold Revolverator: 50,000 bolts required.");return true;}
    prompt(italian?"Premi Triangolo: Rotatore d'oro - 50.000 bolt.":"Press Triangle: Gold Revolverator - 50,000 bolts.");
    if((read<unsigned>(global(0x13c940)+0x1a4)&0x10)==0)return true;
    if(registry_.purchase_golden(arsenal::RevolveratorKey,balance,price)!=arsenal::GoldenPurchaseResult::Purchased)return true;
    write(balance_at,balance);bought_=true;
    retire(preview_,false);
    autosave_(0,-1);
    if(api_->log)api_->log(mod_,OPENRAC_LOG_INFO,"Gold Revolverator purchased: 50000 ordinary bolts; no Gold Bolt or native Gold flags changed");
    return true;
}
bool NativeGoldVendor::poll_purchase(){const bool result=bought_;bought_=false;return result;}
void NativeGoldVendor::reset(){
    retire(preview_,false);retire(dome_,true);
    if(owns_display_map_&&supported()&&api_->loaded_level()==level_&&global(0x1b3ac0)==display_map_&&
        read<unsigned char>(display_map_+display_class)==display_slot_)
        write<unsigned char>(display_map_+display_class,0xff);
    display_map_=0;display_slot_=0xff;owns_display_map_=false;
    level_=-1;prompt_=-1;frames_=retry_=0;bought_=reported_=functions_ready_=false;
}
}
