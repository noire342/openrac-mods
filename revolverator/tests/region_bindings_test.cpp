// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "memory.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace revolverator;
namespace {
int checks=0;
void require(bool condition,const char* message){
    ++checks;if(!condition){std::fprintf(stderr,"region bindings: %s\n",message);std::exit(1);}
}
template<class F>void refuses(F&& action,const char* diagnostic,const char* reason){
    try{action();}
    catch(const std::runtime_error& error){require(std::strstr(error.what(),diagnostic)!=nullptr,reason);return;}
    require(false,reason);
}
void native_function(){}
struct Fixture;
Fixture* live=nullptr;
struct Fixture {
    std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
    openrac_api api{};
    unsigned global_input=0,level_input=0;
    int source_level=-1,global_calls=0,level_calls=0,function_calls=0;
    std::string function_name;
    bool missing_function=false;
    explicit Fixture(const char* version){
        live=this;api.memory=ram.data();api.game="rac1";api.version=version;
        api.relocate_data=+[](unsigned at){++live->global_calls;live->global_input=at;return at+0x400000;};
        api.relocate_level_data=+[](int overlay,unsigned at){
            ++live->level_calls;live->source_level=overlay;live->level_input=at;
            return at+0x500000+unsigned(overlay)*0x100000;
        };
        api.function=+[](const char* name)->void*{
            ++live->function_calls;live->function_name=name;
            return live->missing_function?nullptr:reinterpret_cast<void*>(&native_function);
        };
    }
};
}

int main(){
    const auto& ntsc=RegionBindings::ntsc();const auto& pal=RegionBindings::pal();
    require(RegionBindings::for_version("ntsc")==&ntsc&&RegionBindings::for_version("pal")==&pal,"exact SDK versions select immutable profiles");
    require(!RegionBindings::for_version(nullptr)&&!RegionBindings::for_version("")&&
            !RegionBindings::for_version("PAL")&&!RegionBindings::for_version("jp"),"profile selection refuses absent or unsupported runtime versions");
    require(ntsc.region()==Region::Ntsc&&pal.region()==Region::Pal,"profile identity is explicit");
    require(std::strcmp(pal.version(),"pal")==0&&std::strcmp(ntsc.version(),"ntsc")==0,"profile SDK names stay exact");
    require(std::strcmp(pal.card_serial(),"SCES_509.16")==0&&std::strcmp(pal.card_directory(),"BESCES-50916RATCHET")==0,"PAL sidecars bind the PAL card and save directory");
    require(std::strcmp(ntsc.card_serial(),"SCUS_971.99")==0&&std::strcmp(ntsc.card_directory(),"BASCUS-97199RATCHET")==0,"NTSC card identity remains unchanged");
    require(pal.nominal_hz()==50.f&&ntsc.nominal_hz()==60.f,"disc identity exposes its original field rate independently of the port tick rate");
    const auto& nm=ntsc.memcard();const auto& pm=pal.memcard();
    require(nm.card_stride==0xb8&&pm.card_stride==0xc0,"native memory-card descriptors have distinct verified sizes");
    require(nm.port==0&&pm.port==0&&nm.save_index==0x14&&pm.save_index==0x14,"physical port and save index stay inside the first card descriptor");
    require(nm.card_errors==0xac&&pm.card_errors==0xb4&&nm.main_size==0xb0&&pm.main_size==0xb8,"PAL inserts used-block/request fields before save-header metadata");
    require(nm.active_card==0xc0&&pm.active_card==0xc8&&nm.current_card==0xc4&&pm.current_card==0xcc,"driver card selectors use their regional fields");
    require(nm.operation==0xd4&&pm.operation==0xdc&&nm.pending_operation==0xdc&&pm.pending_operation==0xe4,"operation and queued operation cannot use NTSC fields on PAL");
    require(nm.error==0xe4&&pm.error==0xec&&nm.pending_card==0xe0&&pm.pending_card==0xe8,"PAL errors and pending card identity have explicit fields");
    require(nm.buffer==0xec&&pm.buffer==0xf4&&nm.pending_mask==0xf4&&pm.pending_mask==0xfc,"whole-save buffers and pending masks follow the native structs");
    require(pal.animation_transfer_offset()==0x46&&ntsc.animation_transfer_offset()==0x48,"animation transfer fields are hexadecimal structural offsets");

    require(ntsc.data(0x123456)==0x123456&&ntsc.level(18,0x123456)==0x123456&&ntsc.callback(18,0x123456)==0x123456,"NTSC retains existing canonical address behavior");
    require(pal.data(0x13f350)==0x13f450&&pal.data(0x13d4c0)==0x13d5c8,"Hero and ownership have different region deltas");
    require(pal.data(0x1b3200)==0x1b3580&&pal.data(0x1b3580)==0x1b3900&&pal.data(0x1b3900)==0x1b3c80,"class resources, update functions and ID slots retain separate bindings");
    require(pal.level(0,0x1c8ff0)==0x1c92f0&&pal.level(0,0x1ca4c0)==0x1ca7c0,"vendor preview and state are level data");
    require(pal.level(1,0x16cce0)==0x16cd60&&pal.level(1,0x1ca940)==0x1cabc0,"acquisition keeps its Level1 provenance");
    require(pal.level(0,0x15f7c8)==0x15f8a8&&pal.level(0,0x15f91c)==0x15f9f4&&
            pal.level(0,0x15f91c+16)==0x15fa04,"adjacent HUD policies use their exact fields rather than one broad delta");
    refuses([&]{pal.data(0x13f450);},"0x0013f450","an already-PAL address cannot silently enter the canonical data path");
    refuses([&]{pal.data(0x13d4c0+18);},"executable data","table indexing follows base resolution rather than an unbounded region interval");
    refuses([&]{pal.data(0x1ca4c0);},"executable data","level data cannot resolve through executable space");
    refuses([&]{pal.level(1,0x1ca4c0);},"overlay 1","Level0 symbols cannot silently resolve through Level1");
    refuses([&]{pal.level(19,0x15f5c4);},"overlay 19","unsupported source levels fail closed");
    refuses([&]{pal.level(0,0x123456);},"0x00123456","unknown PAL level bindings identify the missing address");

    require(pal.callback(1,0x24c828)==0x24d130&&pal.callback(1,0x24c878)==0x24d180&&
            pal.callback(1,0x24c898)==0x24d1a0,"Gold callbacks select the neighbouring HUD channel12 entries, not duplicate small-function copies");
    require(pal.callback(0,0x2a5dd8)==0x2a7078&&pal.callback(1,0x2bb758)==0x2bc920&&
            pal.callback(18,0x2a5878)==0x2a6a58,"HUD relays retain the loaded-level physical entry");
    refuses([&]{pal.callback(0,0x24c878);},"guest callback","callback translation rejects a different source overlay");

    require(std::strcmp(pal.function("FUN_L00_00233db8"),"func_L00_00234638")==0,"native ammo resolves the PAL function name");
    require(std::strcmp(pal.function("FUN_L01_002aea70"),"func_L01_002AFD00")==0,"acquisition resolves its PAL function family");
    require(std::strcmp(pal.function("FUN_L00_00260860"),"func_L00_002618D8")==0,"isolated fixtures grant items through the PAL native function");
    require(std::strcmp(pal.function("FUN_0020b178"),"func_0020BFC8")==0,"reviewed changed save families have an explicit binding");
    const char* stable="Vendor_BuildItemList";const char* pal_name="func_001FFAB8";
    require(pal.function(stable)==stable&&pal.function(pal_name)==pal_name,"semantic and already-PAL function names pass through");
    require(!pal.function(nullptr)&&!pal.function("FUN_00999999"),"unknown PAL FUN_ names have no NTSC fallback");
    const char* unknown="FUN_00999999";
    require(ntsc.function(unknown)==unknown&&!ntsc.function(nullptr),"NTSC keeps function lookup behavior");

    {Fixture f("pal");Memory memory(&f.api);
     require(&memory.bindings()==&pal,"Memory selects its runtime profile once");
     const auto hero=memory.global(0x13f350);
     require(hero==0x53f450&&f.global_input==0x13f450&&f.global_calls==1,"Memory maps an executable base before SDK relocation");
     const auto mode=memory.level(0x15f5c4);
     require(mode==0x65f6a8&&f.source_level==0&&f.level_input==0x15f6a8,"one-argument level remains a Level0 binding");
     const auto scene=memory.level(1,0x16cce0);
     require(scene==0x76cd60&&f.source_level==1&&f.level_input==0x16cd60,"source-level mapping occurs before loaded-program relocation");
     const int calls=f.global_calls+f.level_calls;
     memory.write(hero+0x80,0x12345678u);
     require(memory.read<unsigned>(hero+0x80)==0x12345678u&&f.global_calls+f.level_calls==calls,"live guest reads and writes never remap pointers");
     const auto callback=memory.guest_callback(1,0x24c878);
     require(callback==0x24d180&&f.global_calls+f.level_calls==calls,"guest code entries are physical callback addresses, not SDK data relocations");
     require(memory.function<void(*)()>("FUN_001ff288")==&native_function&&f.function_name=="func_001FFAB8","Memory function lookup maps names before calling the SDK");
     require(memory.find_function("FUN_001ff288")==reinterpret_cast<void*>(&native_function)&&f.function_name=="func_001FFAB8","optional function lookup uses the same PAL name mapping");
     const auto function_calls=f.function_calls;
     require(!memory.find_function("FUN_00999999")&&!memory.find_function(nullptr),"optional unknown/null function lookup fails without throwing");
     refuses([&]{memory.function<void(*)()>("FUN_00999999");},"no pal binding","missing PAL function mapping gives a contextual failure");
     require(f.function_calls==function_calls,"a missing PAL mapping never probes the NTSC symbol in the SDK");
     refuses([&]{memory.function<void(*)()>(nullptr);},"<null>","null canonical function names cannot reach the SDK");
     f.missing_function=true;
     require(!memory.find_function("FUN_001ff288"),"an optional native function can be absent from the SDK");
     refuses([&]{memory.function<void(*)()>("FUN_001ff288");},"func_001FFAB8","missing native target reports the resolved PAL symbol");
     require(f.function_name=="func_001FFAB8","failed PAL native lookup does not fall back to NTSC");
    }
    {Fixture f("ntsc");Memory memory(&f.api);
     require(memory.global(0x123456)==0x523456&&f.global_input==0x123456,"NTSC Memory uses unchanged relocation keys");
     require(memory.level(7,0x123456)==0xd23456&&f.source_level==7,"NTSC source-level overload preserves arbitrary native levels");
     memory.function<void(*)()>("FUN_00999999");require(f.function_name=="FUN_00999999","NTSC does not restrict existing function names");
    }
    {Fixture f(nullptr);Memory memory(&f.api);require(&memory.bindings()==&ntsc,"legacy test fakes with no version keep NTSC behavior");}
    {Fixture f("pal");f.api.function=nullptr;Memory memory(&f.api);
     require(!memory.find_function("FUN_001ff288"),"optional lookup tolerates a missing SDK function entry");
     refuses([&]{memory.function<void(*)()>("FUN_001ff288");},"func_001FFAB8","mandatory lookup still reports a missing SDK function entry");}
    {Memory memory(nullptr);require(!memory.find_function("FUN_001ff288"),"optional lookup tolerates a null SDK table");}
    {Fixture f("jp");refuses([&]{Memory memory(&f.api);},"game version jp","unsupported named versions never fall back to NTSC in Memory");}
    std::printf("region bindings: %d profile, provenance, fail-closed and live-pointer checks passed\n",checks);
}
