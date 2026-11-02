// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "equip_assembly.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool value,const char* message){
    ++checks;if(!value){std::fprintf(stderr,"equip assembly: %s\n",message);std::exit(1);}
}
bool near(float a,float b){return std::abs(a-b)<.00001f;}
bool same(const EquipAssemblyPose& a,const EquipAssemblyPose& b){
    return a.housing_scale==b.housing_scale&&a.head_axial_scale==b.head_axial_scale&&a.head_turn==b.head_turn&&a.active==b.active;
}
}
int main(){
    for(float dt:{1.f/30.f,1.f/50.f,1.f/60.f,1.f/120.f}){
        EquipAssembly assembly;
        require(!assembly.active()&&same(assembly.pose(),{}),"untriggered presentation is exactly neutral");
        assembly.begin(true);
        require(assembly.pose().active&&near(assembly.pose().head_axial_scale,.025f)&&near(assembly.pose().housing_scale,.001f),"equip starts from near-hidden body and a nested drill, like native weapon construction");
        float elapsed=0,last_extension=0,last_turn=0,peak=0;
        while(assembly.active()){
            assembly.update({dt,true,false,false,true});elapsed+=dt;
            const auto pose=assembly.pose();peak=std::max(peak,pose.housing_scale);
            require(std::isfinite(pose.housing_scale)&&pose.housing_scale>=.00099f&&pose.housing_scale<=1.22001f,"motor pop stays finite and within native-like bounds");
            require(pose.head_axial_scale>=last_extension&&pose.head_axial_scale<=1.f,"head screws outward monotonically without overshooting its socket length");
            require(pose.head_turn>=last_turn,"powered screw phase advances consistently");
            last_extension=pose.head_axial_scale;last_turn=pose.head_turn;
            require(elapsed<.7f,"equip envelope cannot remain active indefinitely");
        }
        require(elapsed>=EquipAssembly::duration-.00001f&&elapsed<EquipAssembly::duration+dt+.00001f,"duration is elapsed seconds rather than a PAL or NTSC frame count");
        require(peak>1.18f,"housing has a pronounced readable pop before settling");
        const auto done=assembly.pose();
        require(done.housing_scale==1&&done.head_axial_scale==1&&!done.active,"completed assembly restores full-size neutral geometry");
        require(near(std::sin(done.head_turn),0)&&near(std::cos(done.head_turn),1),"two screw turns finish at an equivalent neutral angle");
        for(unsigned i=0;i<120;++i)assembly.update({dt,true,false,false,true});
        require(same(assembly.pose(),done),"frame updates cannot replay a completed equip");
        assembly.reset();require(same(assembly.pose(),{}),"actor/epoch reset forgets all transient presentation state");
    }
    {
        EquipAssembly assembly;assembly.begin(true);
        assembly.update({.05f,true,false,false,true});
        const auto motor=assembly.pose();
        require(motor.housing_scale>.25f&&motor.head_axial_scale==EquipAssembly::initial_head_scale&&motor.head_turn==0,
                "motor visibly emerges before its drill begins to extend or turn");
        assembly.update({.08f,true,false,false,true});
        const auto pop=assembly.pose();
        require(pop.housing_scale>1.2f&&pop.head_axial_scale<.1f&&pop.head_turn>0,
                "motor reaches a full-size pop while the drill is still nested and starting to screw out");
        assembly.update({.08f,true,false,false,true});
        assembly.update({.08f,true,false,false,true});
        const auto extending=assembly.pose();
        require(extending.housing_scale==1&&extending.head_axial_scale>.4f&&extending.head_axial_scale<.6f,
                "body settles before the second half of the clearly visible drill construction");
        assembly.update({.08f,true,false,false,true});
        assembly.update({.08f,true,false,false,true});
        assembly.update({.10f,true,false,false,true});
        const auto locked=assembly.pose();
        require(locked.active&&locked.head_axial_scale==1&&near(std::sin(locked.head_turn),0),
                "complete drill locks at its original orientation before the presentation ends");
        assembly.update({.05f,true,false,false,true});
        require(!assembly.active(),"finished construction never adds a gameplay cooldown");
    }
    {
        EquipAssembly assembly;assembly.begin(true);assembly.update({.07f,true,false,false,true});
        const auto before=assembly.pose();
        for(unsigned i=0;i<240;++i)assembly.update({1.f/60.f,true,true,true,true});
        require(same(before,assembly.pose()),"pause freezes phase and extension even if menu input reports attack");
        assembly.update({.07f,false,false,false,true});require(same(before,assembly.pose()),"non-gameplay updates do not consume equip time");
        for(float dt:{0.f,-.1f,.101f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
            assembly.update({dt,true,false,false,true});require(same(before,assembly.pose()),"invalid delta cannot jump or poison the envelope");
        }
        assembly.update({1.f/60.f,true,false,true,true});
        const auto attack=assembly.pose();
        require(!assembly.active()&&attack.housing_scale==1&&attack.head_axial_scale==1,"first attack completes cosmetic assembly immediately without a gameplay wait");
        require(attack.head_turn==before.head_turn,"attack handover preserves the shaft phase rather than snapping");
    }
    {
        EquipAssembly assembly;assembly.begin(false);
        for(unsigned i=0;i<40;++i){assembly.update({1.f/60.f,true,false,false,false});require(assembly.pose().head_turn==0,"empty equipped drill never receives cosmetic screw rotation");}
        require(!assembly.active()&&assembly.pose().head_axial_scale==1,"unpowered assembly still settles to the complete weapon");
        assembly.begin(true);assembly.update({.08f,true,false,false,true});const float angle=assembly.pose().head_turn;
        for(unsigned i=0;i<40;++i){assembly.update({1.f/60.f,true,false,false,false});require(assembly.pose().head_turn==angle,"losing ammunition freezes the screw phase through completion");}
        require(!assembly.active(),"empty phase cannot leave the presentation stuck active");
    }
    std::printf("equip assembly: %u timing, pause, phase and gameplay-yield checks passed\n",checks);
}
