// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_rotor.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
        // Independent source164 fixture: joint35 lies at the physical socket,
        // between the head baseY=-4.71484375 and housing frontY=-4.712890625.
        // Merely selecting an arbitrary point on the shaft masks axial gaps.
        const Vec3 pivot=Vec3{.16387811303138733f,-4.713099479675293f,2.4493489265441895f}*(.08294187486171722f/.1458333283662796f*1024);
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
        for(int i=0;i<10;++i){
            rotor.update(weapon,20,Phase::Windup,0,0,.016f,true,false);
            const auto q=f.get<std::array<float,4>>(head+0x10);const auto t=f.get<Vec3>(head+0x30);
            const auto scale=f.get<Vec3>(head+0x20);
            const auto transformed=[&](Vec3 p){const auto v=p+bind;
                return native_rotate(q,{v.x*scale.x,v.y*scale.y,v.z*scale.z})+t;};
            require(distance(transformed(pivot),pivot)<.001f,"head enlargement keeps the motor attachment fixed");
            const auto grown=transformed(pivot+Vec3{200,-1200,0});
            require(std::abs(distance(grown,pivot+Vec3{0,-1200*scale.y,0})-200*scale.x)<.002f,
                "only the head grows, around its real shaft pivot with the native inverse bind");
            for(unsigned joint=0;joint<4;++joint)
                require(distance(f.get<Vec3>(arena+joint*64+0x20),{1,1,1})==0,"motor housing and grip retain their original dimensions");
        }
        for(int i=0;i<24;++i)rotor.update(weapon,20,Phase::Idle,0,0,.016f,true,false);
        require(distance(f.get<Vec3>(head+0x20),{1,1,1})==0,"head restores original dimensions after the gesture");
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
        Fake f;NativeRotor rotor(&f.api,f.guest,false,true);
        rotor.update(weapon,0,Phase::Idle,0,0,0,false,true);
        const unsigned head=f.get<unsigned>(weapon+0x64),root=head-4*64;
        require(f.get<unsigned char>(root+3)==0,"acquisition keeps the authored wrist/root pose instead of placing a rigid drill at the scene origin");
        require(f.get<unsigned char>(head+3)==1,"acquisition still neutralizes the old glove fingers and keeps the drill head attached");
        require(f.get<unsigned>(klass+0x14)==skeleton,"scene posing never replaces a shared native skeleton");
    }
    // The equipped inventory Ratchet owns another Walloper actor. Its finger
    // animation must never leak into the separately skinned drill head, nor
    // share modifiers/rotation with the live gameplay weapon or rotating tile.
    for(const char* region:{"ntsc","pal"})for(unsigned render:{2016u,2018u}){
        Fake f;f.api.version=region;
        constexpr unsigned preview=0x22000,preview_original=0x32000;
        for(unsigned offset=0;offset<256;offset+=4)
            f.put(preview+offset,f.get<unsigned>(weapon+offset));
        f.put(preview+0x38,29u);f.put(preview+0x78,0x29000u);
        f.put(preview+0x64,preview_original);
        // A native glove animation/modifier exists before the drill override.
        f.put<unsigned char>(preview_original+3,1);
        f.put(preview_original+4,0x70000100u);
        f.put(preview_original+0x30,Vec3{1800,-900,2200});
        NativeRotor gameplay(&f.api,f.guest),equipped(&f.api,f.guest,false,true);
        for(unsigned frame=0;frame<12;++frame)
            gameplay.update(weapon,20,Phase::Windup,0,0,.016f,true,false);
        const unsigned gameplay_head=f.get<unsigned>(weapon+0x64);
        const auto gameplay_rotation=f.get<std::array<float,4>>(gameplay_head+0x10);
        const auto gameplay_scale=f.get<Vec3>(gameplay_head+0x20);
        for(unsigned frame=0;frame<24;++frame){
            // The preview callback canonicalizes the aliased model before
            // applying native class-specific pose and the neutralizer.
            f.put<unsigned short>(preview+0xa6,180);
            equipped.update(preview,0,Phase::Idle,0,0,0,false,true);
            const unsigned head=f.get<unsigned>(preview+0x64),root=head-4*64;
            require(head!=gameplay_head,"equipped preview and gameplay own independent joint chains");
            require(f.get<unsigned char>(root+3)==0,"equipped preview preserves the native animated wrist/root");
            for(unsigned joint=1;joint<5;++joint)
                require(f.get<unsigned char>(root+joint*64+3)==1,"equipped preview replaces every animated glove finger");
            const auto q=f.get<std::array<float,4>>(head+0x10);
            const auto t=f.get<Vec3>(head+0x30);
            const Vec3 point{180,-2000,700},bind{-132,3060,-10};
            const auto rigid=native_rotate(q,point+bind)+t;
            require(distance(rigid,point)<.001f,"equipped preview head retains its exact rigid position on the housing");
            require(distance(f.get<Vec3>(head+0x20),{1,1,1})==0,"equipped preview does not inherit the gameplay attack enlargement");
            require(f.get<std::array<float,4>>(gameplay_head+0x10)==gameplay_rotation&&
                distance(f.get<Vec3>(gameplay_head+0x20),gameplay_scale)==0,"preview posing never edits gameplay rotation or scale");
            f.put<unsigned short>(preview+0xa6,static_cast<unsigned short>(render));
        }
        equipped.reset();
        require(f.get<unsigned>(preview+0x64)==preview_original,"leaving inventory restores the exact native preview modifier chain");
        require(f.get<unsigned>(weapon+0x64)==gameplay_head,"leaving inventory preserves the live gameplay rotor");
    }
    // The authored head and motor surfaces nearly meet at this socket. This
    // real-mesh fixture checks their separation, not just a chosen math pivot.
    for(const char* region:{"ntsc","pal"})for(unsigned render:{2017u,2018u})
    for(float class_scale:{.08f,.1458333283662796f,.25f}){
        Fake f;f.api.version=region;f.put(klass+0x24,class_scale);
        NativeRotor rotor(&f.api,f.guest);
        const float units=.08294187486171722f/class_scale*1024.f;
        const Vec3 socket=Vec3{.16387811303138733f,-4.713099479675293f,2.4493489265441895f}*units;
        const Vec3 head_base=Vec3{1.466796875f,-4.71484375f,2.45703125f}*units;
        const float housing_front=-4.712890625f*units;
        const Vec3 bind{-132,3060,-10};
        for(unsigned frame=0;frame<24;++frame){
            f.put<unsigned short>(weapon+0xa6,180);
            rotor.update(weapon,20,Phase::Windup,0,0,1.f/60,true,false);
            const unsigned head=f.get<unsigned>(weapon+0x64),root=head-4*64;
            const auto q=f.get<std::array<float,4>>(head+0x10);
            const auto scale=f.get<Vec3>(head+0x20),translation=f.get<Vec3>(head+0x30);
            const auto transform=[&](Vec3 p){const auto v=p+bind;
                return native_rotate(q,{v.x*scale.x,v.y*scale.y,v.z*scale.z})+translation;};
            require(distance(transform(socket),socket)<.003f,"physical drill socket stays fixed throughout attack enlargement on both regions and variants");
            const float gap=housing_front-transform(head_base).y;
            require(gap>=0&&gap<.0025f*units+.003f,"the real head base stays against the housing rather than pulling outward when scaled");
            require(distance(f.get<Vec3>(root+0x30),{})==0&&distance(f.get<Vec3>(root+0x20),{1,1,1})==0,
                    "anchored enlargement leaves the motor housing unmoved and unscaled");
            if(frame>=3)require(std::abs(scale.x-2.f)<.0001f&&std::abs(scale.y-1.2f)<.0001f,
                               "repair retains the requested full 2x radial and 1.2x axial attack pulse");
            f.put<unsigned short>(weapon+0xa6,static_cast<unsigned short>(render));
        }
        f.put<unsigned short>(weapon+0xa6,180);
        rotor.update(weapon,20,Phase::Spinning,.5f,1.7f,1.f/60,true,false);
        const unsigned head=f.get<unsigned>(weapon+0x64);
        require(distance(f.get<Vec3>(head+0x20),{1,1,1})==0,"captured enemy immediately restores the normal-size spinning drill");
    }
    for(bool powered:{false,true}){
        Fake f;NativeRotor rotor(&f.api,f.guest);EquipAssembly assembly;
        assembly.begin(powered);
        const float units=.08294187486171722f/.1458333283662796f*1024.f;
        const Vec3 socket=Vec3{.16387811303138733f,-4.713099479675293f,2.4493489265441895f}*units;
        const Vec3 base=Vec3{1.466796875f,-4.71484375f,2.45703125f}*units;
        const Vec3 bind{-132,3060,-10};
        const float front=-4.712890625f*units;
        for(unsigned frame=0;frame<36;++frame){
            rotor.update(weapon,powered?20:0,Phase::Idle,0,0,1.f/60.f,true,false);
            assembly.update({1.f/60.f,true,false,false,powered});
            const auto pose=assembly.pose();rotor.set_assembly_pose(pose);rotor.apply();
            const unsigned head=f.get<unsigned>(weapon+0x64),root=head-4*64;
            const auto q=f.get<std::array<float,4>>(head+0x10);
            const auto scale=f.get<Vec3>(head+0x20),translation=f.get<Vec3>(head+0x30);
            const float housing=f.get<Vec3>(root+0x20).x;
            const auto transform=[&](Vec3 p){const auto v=p+bind;
                return (native_rotate(q,{v.x*scale.x,v.y*scale.y,v.z*scale.z})+translation)*housing;};
            require(distance(transform(socket),socket*housing)<.003f,"equip screw extension remains physically attached while the whole housing pops");
            const float gap=front*housing-transform(base).y;
            require(gap>=0&&gap<(.0025f*units+.003f)*housing,"collar seam remains closed from retracted to full-length equip pose");
            require(distance(f.get<Vec3>(root+0x20),{pose.housing_scale,pose.housing_scale,pose.housing_scale})==0,
                    "native root applies the exact cosmetic housing envelope");
            if(!powered)require(std::abs(q[0])+std::abs(q[1])+std::abs(q[2])==0,"an empty drill does not rotate during equip assembly");
        }
        assembly.begin(true);assembly.update({.06f,true,false,false,true});
        assembly.update({1.f/60.f,true,false,true,true});
        for(unsigned frame=0;frame<10;++frame){
            rotor.update(weapon,20,Phase::Windup,0,0,1.f/60.f,true,false);
            rotor.set_assembly_pose(assembly.pose());rotor.apply();
        }
        const unsigned head=f.get<unsigned>(weapon+0x64),root=head-4*64;
        require(distance(f.get<Vec3>(root+0x20),{1,1,1})==0&&distance(f.get<Vec3>(head+0x20),{2,1.2f,2})<.0001f,
                "the first real attack bypasses equip assembly and retains the full requested attack scale");
        rotor.set_assembly_pose({1,1,std::numeric_limits<float>::quiet_NaN(),true});
        rotor.apply();
        require(distance(f.get<Vec3>(root+0x20),{1,1,1})==0&&std::isfinite(f.get<float>(head+0x10)),
                "malformed cosmetic input cannot poison the native weapon pose");
    }
    {
        Fake f;f.guest.reset();NativeRotor rotor(&f.api,f.guest);
        rotor.update(weapon,10,Phase::Idle,0,0,.03f,true,false);
        require(f.allocations==0&&f.get<unsigned>(weapon+0x64)==original_node&&rotor.angle()==0,
            "without reserved storage the adapter must not fall back to the exhausted HUD heap");
    }
    std::printf("native rotor: %d pivot, skinning, lifecycle and ownership checks passed\n",checks);
}
