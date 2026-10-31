// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_acquisition.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
using namespace revolverator;
namespace {
constexpr unsigned Hero=0x13f350,Mode=0x15f5c4,Vendor=0x1ca940,Scene=0x16cce0;
constexpr unsigned Body=0x400000,Actor=0x410000,ActorKeys=0x530000,CameraKeys=0x530100;
unsigned char* ram{};int checks{},loaded=1,presentations{},placements{},projects{},restores{},posed_items{};
bool present_available=true,place_available=true,use_real_body=false,golden=false;
std::string scene_name;std::vector<std::string> calls;
void require(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"acquisition: %s\n",why);std::exit(1);}}
template<class T>void put(unsigned at,const T& value){std::memcpy(ram+at,&value,sizeof value);}
template<class T>T get(unsigned at){T value{};std::memcpy(&value,ram+at,sizeof value);return value;}
int loaded_level(){return loaded;}
unsigned relocate(unsigned at){return at;}
unsigned relocate_level(int,unsigned at){return at;}
void project(void*){++projects;calls.push_back("project");scene_name=golden?"Rotatore d'oro v3":"Rotatore v3";}
void restore(void*){++restores;calls.push_back("restore");}
void pose_item(void*,unsigned actor){require(actor==Actor+256,"only the native scene weapon is projected, never Ratchet or a world actor");++posed_items;}
void present(unsigned trigger,int item){
    require(item==18,"native scene uses safe Walloper item18 for base and Golden");
    require(projects==presentations+1,"logical name/model projection precedes every native scene");
    require(get<unsigned short>(trigger+0xa6)==0,"scene trigger is a snapshot of Ratchet, not a synthetic moby class");
    ++presentations;calls.push_back("present");
    std::memset(ram+Vendor,0xcc,0x220);put(Vendor,3);put(Vendor+0x28,trigger);
    put(Mode,5);put(Hero+0x2084,100);put<unsigned char>(Hero+0x20a5,1);put<unsigned char>(Hero+0x20ac,1);
    put(Hero+0x20b8,18);put(0x141660,18);
    put(Hero+0x80,std::array<float,4>{{90,80,70,1}});
    put<short>(Scene+0x44,2);put(Scene+0x54,CameraKeys);
    put(Scene+0x178,use_real_body?Body:Actor);put(Scene+0x17c,Actor+256);
    put<unsigned short>(Actor+0xa6,0);put(Actor+0x78,ActorKeys);
    put<unsigned short>(Actor+256+0xa6,180);
    put(ActorKeys,std::array<float,4>{{1,2,3,1}});
    put(CameraKeys,std::array<float,4>{{1,6,6,1}});
}
void place(unsigned position,unsigned rotation,int mode,int flag){
    require(mode==0&&flag==1,"restores using the complete native placement lifecycle");
    ++placements;calls.push_back("place");
    put(Hero+0x80,get<std::array<float,4>>(position));put(Body+0x10,get<std::array<float,4>>(position));
    put(Hero+0x90,get<std::array<float,4>>(rotation));put(Hero+0x2084,0);
    put(Hero+0xe0,std::array<float,4>{});
}
void* function(const char* name){
    if(!std::strcmp(name,"FUN_L01_002aea70"))return present_available?reinterpret_cast<void*>(present):nullptr;
    if(!std::strcmp(name,"FUN_L00_00216f90"))return place_available?reinterpret_cast<void*>(place):nullptr;
    return nullptr;
}
struct Fixture {
    std::vector<unsigned char> memory=std::vector<unsigned char>(0x2000000);
    openrac_api api{};GuestArena arena;NativeAcquisition acquisition;
    const std::array<float,4> position{{10,20,30,1}},rotation{{0,0,.5f,0}},velocity{{0,0,.125f,0}};
    const std::array<int,7> equipment{{15,1,2,3,4,5,6}},pending{{0,9,8,7,6,5,4}};
    std::array<unsigned char,0x220> vendor{};
    explicit Fixture(bool pose=false):acquisition(&api,nullptr,arena,{nullptr,project,restore,pose?pose_item:nullptr}){
        ram=memory.data();loaded=1;presentations=placements=projects=restores=posed_items=0;
        present_available=place_available=true;use_real_body=golden=false;scene_name.clear();calls.clear();
        api.memory=ram;api.game="rac1";api.version="ntsc";api.loaded_level=loaded_level;
        put(0x15ed7c,1.f/60.f);
        api.relocate_data=relocate;api.relocate_level_data=relocate_level;api.function=function;
        arena.reserve(0x600000,4096);put(Hero+0x2080,Body);put(Hero+0x2084,0);put(Hero+0x22a8,4);
        put(Body+0x24,0x500000u);put(Body+0x78,0x500100u);put<unsigned short>(Body+0xa6,0);
        put<unsigned short>(Body+0x34,0x104);put(Hero+0x80,position);put(Hero+0x90,rotation);put(Hero+0xe0,velocity);
        put(0x140408,15);put(0x141660,equipment);put(Hero+0x20b8,pending);
        put<unsigned char>(Hero+0x20a5,0);put<unsigned char>(Hero+0x20ac,0);
        put(0x1ca4a0+18*4,7);put(0x166c80+0x140,std::array<float,3>{{12,25,34}});
        for(unsigned i=0;i<vendor.size();++i)vendor[i]=static_cast<unsigned char>(i^0x6d);
        std::memcpy(ram+Vendor,vendor.data(),vendor.size());
        std::memset(ram+0x13d428,0x41,148);std::memset(ram+0x13d4c0,0x52,37);
        std::memset(ram+0x13e520,0x63,40);put(0x15ed98,76543);
    }
    void begin(){require(acquisition.request(),"request accepted");for(unsigned i=0;i<3;++i)acquisition.frame(true,false);require(acquisition.presenting()&&presentations==1,"starts after two quiet gameplay samples");}
    void end(){put(Mode,0);acquisition.frame(true,false);require(acquisition.active()&&placements==0,"first gameplay transition frame remains locked");acquisition.frame(true,false);}
    std::array<unsigned char,225> inventory()const{
        std::array<unsigned char,225> result{};std::memcpy(result.data(),ram+0x13d428,148);
        std::memcpy(result.data()+148,ram+0x13d4c0,37);std::memcpy(result.data()+185,ram+0x13e520,40);return result;
    }
};
}
int main(){
    {Fixture f(true);f.begin();require(posed_items==1,"scene model is projected as soon as native actors become available");
     f.acquisition.frame(true,false);require(posed_items==2,"scene model projection is refreshed after native animation updates");f.end();}
    {Fixture f;const auto inventory=f.inventory();f.begin();
        require(get<unsigned short>(Body+0x34)==0x105,"only the live duplicate Ratchet is hidden once a separate scene hero exists");
        require(scene_name=="Rotatore v3"&&get<int>(Mode)==5,"base scene reads the projected native name");
        const auto stage=get<std::array<float,16>>(Vendor+0x80);
        const auto origin=get<std::array<float,4>>(Vendor+0xc0);
        require(std::abs(stage[0]+2*stage[4]+origin[0]-10)<.0001f&&
                std::abs(stage[1]+2*stage[5]+origin[1]-20)<.0001f&&origin[2]+3==30,
                "scene actor origin aligns precisely with Ratchet's saved gameplay position");
        const float camera_dx=4*stage[4],camera_dy=4*stage[5];
        require(std::abs(camera_dx*5-camera_dy*2)<.0001f&&camera_dx*2+camera_dy*5>0,
                "retail camera path is rotated toward the side already visible to the gameplay camera");
        std::array<unsigned char,22> report{};report.fill(0x33);f.acquisition.filter_report(report.data(),20);
        require(report[0]==255&&report[1]==255&&report[2]==128&&report[5]==128&&report[6]==0&&report[17]==0&&report[18]==0x33&&report[21]==0x33,"held attack, movement, Start and pressure are neutralized without writing beyond native report bytes");
        put<unsigned short>(Body+0x34,0x305);put(Body+0x3c,0x777777u);f.end();
        require(!f.acquisition.active()&&!f.acquisition.pending()&&placements==1&&restores==1,"completion consumes one request and releases projection exactly once");
        require(get<std::array<float,4>>(Hero+0x80)==f.position&&get<std::array<float,4>>(Hero+0x90)==f.rotation&&get<std::array<float,4>>(Hero+0xe0)==f.velocity,"returns to the precise original position, facing and velocity");
        require(get<std::array<int,7>>(0x141660)==f.equipment&&get<int>(Hero+0x20b8)==15&&get<int>(Hero+0x20bc)==9,"restores equipment and other pending slots using native item switching, without restoring actor pointers");
        require(get<unsigned short>(Body+0x34)==0x304&&get<unsigned char>(Hero+0x20a5)==0&&get<unsigned char>(Hero+0x20ac)==0,"restores the owned hide bit and input locks while preserving unrelated flag changes and lighting");
        require(!std::memcmp(ram+Vendor,f.vendor.data(),f.vendor.size()),"restores the original native vendor/menu state");
        require(f.inventory()==inventory&&get<int>(0x15ed98)==76543,"presentation never grants ammunition, ownership, Gold flags, XP or spends bolts");
        const auto before=report;f.acquisition.filter_report(report.data(),20);require(report==before,"idle presenter leaves input untouched");
        require(calls==std::vector<std::string>({"project","present","place","restore"}),"projection surrounds the complete native scene and restoration lifecycle");}
    {Fixture f;golden=true;f.begin();require(scene_name=="Rotatore d'oro v3","Golden uses the same native animation with its current Gold/versioned name");f.end();}
    {Fixture f;require(f.acquisition.request(),"XP waiting request queued");
        f.acquisition.frame(true,true);require(!f.acquisition.active()&&f.acquisition.pending()&&presentations==0&&projects==0,"an existing XP presentation is never overlapped or projected over");
        put(Mode,5);f.acquisition.frame(true,false);require(!f.acquisition.active()&&presentations==0,"native mode protects even an external scene whose XP getter is unavailable");
        put(Mode,0);for(unsigned i=0;i<3;++i)f.acquisition.frame(true,false);require(presentations==1,"queued acquisition runs after the external scene releases gameplay");}
    {Fixture f;f.begin();require(f.acquisition.request(),"a level event can queue behind acquisition");f.end();
        require(f.acquisition.pending(),"completing acquisition retains the following level event");
        golden=true;for(unsigned i=0;i<33;++i)f.acquisition.frame(true,false);
        require(presentations==2&&f.acquisition.presenting()&&scene_name=="Rotatore d'oro v3","the next event uses the current Golden projection after the bounded separation interval");}
    {Fixture f;require(f.acquisition.request(),"moving-player request queued");put(Hero+0xe0,std::array<float,4>{{.2f,0,0,0}});
        for(unsigned i=0;i<9;++i)f.acquisition.frame(true,false);
        require(f.acquisition.active()&&presentations==0,"movement is allowed to settle under neutral input before scene start");
        put(Hero+0xe0,f.velocity);put<unsigned char>(Hero+0x20a8,1);for(unsigned i=0;i<3;++i)f.acquisition.frame(true,false);require(presentations==0,"a sustained shot must finish before the acquisition can own Ratchet");
        put<unsigned char>(Hero+0x20a8,0);f.acquisition.frame(true,false);f.acquisition.frame(true,false);require(presentations==1,"two quiet samples end preparation");}
    {Fixture f;use_real_body=true;f.begin();require(get<unsigned short>(Body+0x34)==0x104,"a scene that reuses the actual hero is never hidden");f.end();}
    {Fixture f;f.begin();put(Body+0x24,0x5a0000u);put<unsigned short>(Body+0x34,0x777);f.end();
        require(placements==0&&get<unsigned short>(Body+0x34)==0x777,"recycled hero identity is never repositioned or given the former actor's flags");}
    {Fixture f;f.begin();loaded=2;const auto before=f.memory;f.acquisition.frame(true,false);
        require(f.memory==before&&restores==1&&!f.acquisition.pending(),"a level change abandons old guest pointers without writing into the new epoch");}
    {Fixture f;f.begin();put(Vendor+0x28,0xabcdefu);put(Vendor+0x14,0x12345678u);f.end();
        require(get<unsigned>(Vendor+0x14)==0x12345678u,"a foreign replacement vendor context is preserved");}
    {Fixture f;present_available=false;require(f.acquisition.request(),"missing-binding request queued");f.acquisition.frame(true,false);
        require(!f.acquisition.active()&&f.acquisition.pending()&&presentations==0&&projects==0,"missing native presenter fails closed without locking input or changing projection");}
    {Fixture f;f.arena.reset();require(f.acquisition.request(),"missing-arena request queued");f.acquisition.frame(true,false);
        require(!f.acquisition.active()&&presentations==0,"no fallback guest heap is borrowed without reserved arena ownership");}
    {Fixture f;require(f.acquisition.request(),"unsafe gameplay request queued");f.acquisition.frame(false,false);
        require(!f.acquisition.active()&&f.acquisition.pending()&&presentations==0,"capture/throw owner keeps control until the caller reports a safe gameplay state");}
    {Fixture f;put(0x1ca4a0+18*4,-1);require(f.acquisition.request(),"missing-scene request queued");f.acquisition.frame(true,false);
        require(!f.acquisition.active()&&presentations==0,"missing scene18 cannot index a native archive");}
    {Fixture f;require(f.acquisition.request(),"unsafe-state request queued");put(Hero+0x90,std::array<float,4>{{0,0,std::numeric_limits<float>::quiet_NaN(),0}});f.acquisition.frame(true,false);
        require(!f.acquisition.active()&&presentations==0,"non-finite player transforms never reach native camera/scene math");}
    {Fixture f;for(unsigned i=0;i<NativeAcquisition::maximum_pending;++i)require(f.acquisition.request(),"bounded request accepted");require(!f.acquisition.request(),"request queue rejects overflow");f.acquisition.reset();require(!f.acquisition.pending()&&!f.acquisition.active(),"explicit reset cancels pending requests without starting a scene");}
    {Fixture f;f.begin();std::array<unsigned char,8> bytes{};bytes.fill(0x5a);const auto before=bytes;
        f.acquisition.filter_report(bytes.data(),5);f.acquisition.filter_report(nullptr,20);require(bytes==before,"short/null input spans are never partially overwritten");
        f.acquisition.reset();require(restores==1&&get<unsigned short>(Body+0x34)==0x104,"reset releases only the still-owned body visibility and projection");}
    std::printf("native acquisition: %d lifecycle, external-scene, input and ownership checks passed\n",checks);
}
