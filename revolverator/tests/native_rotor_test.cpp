// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_rotor.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace revolverator;
namespace {
int checks;
void require(bool test,const char* message){++checks;if(!test){std::fprintf(stderr,"native rotor: %s\n",message);std::exit(1);}}
constexpr unsigned weapon=0x20000,klass=0x24000,skeleton=0x26000,original_node=0x30000,extra_node=0x31000;
struct Fake;
Fake* live;
struct Fake {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x2000000);
    openrac_api api{};GuestArena guest;int level=1,allocations=0;unsigned cursor=0x40000;
    template<class T>T get(unsigned at)const{T v;std::memcpy(&v,bytes.data()+at,sizeof v);return v;}
    template<class T>void put(unsigned at,T value){std::memcpy(bytes.data()+at,&value,sizeof value);}
    static unsigned allocate(unsigned n){++live->allocations;const auto at=live->cursor;live->cursor+=(n+15)&~15u;return at;}
    Fake(){
        guest.reserve(0x40000,4096);
        live=this;api.memory=bytes.data();api.loaded_level=+[]{return live->level;};
        api.function=+[](const char* name)->void*{return std::strcmp(name,"FUN_001ff288")==0?reinterpret_cast<void*>(&allocate):nullptr;};
        put(weapon+0x24,klass);put(weapon+0x38,17u);put(weapon+0x78,0x28000u);
        put<unsigned short>(weapon+0xa6,180);put(weapon+0x64,original_node);
        put<unsigned char>(klass+8,5);put(klass+0x14,skeleton);put<float>(klass+0x24,.1458333283662796f);
        for(unsigned i=0;i<5;++i){
            std::array<float,16> bind{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            if(i==4){bind[12]=-132;bind[13]=3060;bind[14]=-10;}
            put(skeleton+i*64,bind);
        }
    }
};
Vec3 native_rotate(std::array<float,4> q,Vec3 p){
    const float x=q[0],y=q[1],z=q[2],w=q[3];
    // The native evaluator's quat_rows, whose rows are images of the axes.
    const Vec3 a{1-2*y*y-2*z*z,2*y*x-2*z*w,2*z*x+2*y*w};
    const Vec3 b{2*z*w+2*y*x,1-2*x*x-2*z*z,2*z*y-2*x*w};
    const Vec3 c{2*z*x-2*y*w,2*x*w+2*z*y,1-2*x*x-2*y*y};
    return a*p.x+b*p.y+c*p.z;
}
float distance(Vec3 a,Vec3 b){return std::sqrt(length_squared(a-b));}
}
int main(){
    {
        Fake f;NativeRotor rotor(&f.api,f.guest);
        rotor.update(weapon,20,Phase::Idle,0,0,.016f,true,false);
        require(f.guest.used()==320&&f.allocations==0,"one fixed-size block comes from the reserved level arena, never the HUD heap");
        const unsigned head=f.get<unsigned>(weapon+0x64),arena=head-4*64;
        for(unsigned i=0;i<5;++i){
            const auto node=arena+i*64;
            require(f.get<unsigned char>(node+3)==1&&f.get<float>(node+0xc)==1,"all five native jab records are replaced");
            require(f.get<unsigned>(node+4)==0x70000000u+i*64,"each modifier targets its own native joint");
            if(i!=4){
                require(f.get<std::array<float,4>>(node+0x10)==std::array<float,4>{0,0,0,1},"housing joints cannot rotate");
                require(length_squared(f.get<Vec3>(node+0x30))==0,"housing joints cannot translate");
            }
        }
        const Vec3 pivot=Vec3{.16387811303138733f,0,2.4493489265441895f}*(.08294187486171722f/.1458333283662796f*1024);
        const Vec3 bind{-132,3060,-10};
        for(int i=0;i<100;++i){
            rotor.update(weapon,20,Phase::Idle,0,0,.03f,true,false);
            const auto q=f.get<std::array<float,4>>(head+0x10);const auto t=f.get<Vec3>(head+0x30);
            const auto transformed=[&](Vec3 p){return native_rotate(q,p+bind)+t;};
            require(distance(transformed(pivot),pivot)<.001f,"the real offset shaft axis cannot orbit the motor housing");
            const Vec3 axis_point=pivot+Vec3{0,-4000,0};
            require(distance(transformed(axis_point),axis_point)<.001f,"every point on the shaft axis is fixed");
            const Vec3 rim=pivot+Vec3{200,-1200,0};
            require(std::abs(distance(transformed(rim),pivot+Vec3{0,-1200,0})-200)<.002f,
                "head vertices rotate rigidly with constant radius under the actual native inverse bind");
            const Vec3 delta=transformed(rim)-(pivot+Vec3{0,-1200,0});
            // An upright shaft maps local -Y to world +Z and local +Z to +Y.
            require(std::abs(delta.x-200*std::cos(rotor.angle()))<.002f&&
                    std::abs(delta.z-200*std::sin(rotor.angle()))<.002f,
                "the head rotates in the same signed direction as the held enemy");
        }
        require(f.guest.used()==320,"frames do not leak guest heap arenas");
        const float angle=rotor.angle();rotor.update(weapon,0,Phase::Idle,0,0,.03f,true,false);
        require(rotor.angle()==angle,"an empty native weapon is stationary");
        rotor.dry_fire();for(int i=0;i<10;++i)rotor.update(weapon,0,Phase::Idle,0,0,.03f,true,false);
        require(rotor.angle()==angle,"the native dry-fire motion returns to its initial angle");
        f.put(extra_node+8,head);f.put(weapon+0x64,extra_node);rotor.reset();
        require(f.get<unsigned>(weapon+0x64)==extra_node&&f.get<unsigned>(extra_node+8)==original_node,
            "reset removes only owned modifiers and preserves other mods' chain nodes");
        rotor.update(weapon,20,Phase::Idle,0,0,.01f,true,false);
        require(f.guest.used()==320,"re-equipping reuses a valid arena in the same level");
        f.put(weapon+0x38,19u);f.put(weapon+0x64,extra_node);rotor.reset();
        require(f.get<unsigned>(weapon+0x64)==extra_node,"stale identity cannot remove a new entity's modifiers");
    }
    {
        Fake f;NativeRotor rotor(&f.api,f.guest);rotor.dry_fire();
        rotor.update(weapon,0,Phase::Idle,0,0,.03f,true,false);
        require(rotor.angle()>0&&rotor.controller().dry_firing(),"a dry request queued before attachment is not lost");
        rotor.reset(true);f.level=2;f.guest.reserve(0x50000,4096);f.put(weapon+0x64,original_node);
        rotor.update(weapon,20,Phase::Idle,0,0,.01f,true,false);
        require(f.guest.used()==320&&f.get<unsigned>(weapon+0x64)==0x50000+4*64,"level reload obtains storage from the newly reserved range");
    }
    {
        Fake f;f.put<float>(skeleton+4*64,.5f);NativeRotor rotor(&f.api,f.guest);
        rotor.update(weapon,20,Phase::Idle,0,0,.03f,true,false);
        require(f.guest.used()==0&&f.get<unsigned>(weapon+0x64)==original_node,
            "an incompatible native bind fails conservatively without editing the skeleton");
    }
    {
        Fake f;f.guest.reset();NativeRotor rotor(&f.api,f.guest);
        rotor.update(weapon,10,Phase::Idle,0,0,.03f,true,false);
        require(f.allocations==0&&f.get<unsigned>(weapon+0x64)==original_node&&rotor.angle()==0,
            "without reserved storage the adapter must not fall back to the exhausted HUD heap");
    }
    std::printf("native rotor: %d pivot, skinning, lifecycle and ownership checks passed\n",checks);
}
