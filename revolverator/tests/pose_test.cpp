// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "arm_math.hpp"
#include "attachment.hpp"
#include "damage_targets.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace revolverator;
namespace {
int checks=0;
void require(bool condition,const char* message) {
    ++checks;
    if(!condition){std::fprintf(stderr,"pose: %s\n",message);std::exit(1);}
}
float distance(Vec3 a,Vec3 b){return std::sqrt(length_squared(a-b));}
bool near(Vec3 a,Vec3 b,float epsilon=.0002f){return distance(a,b)<epsilon;}
float segment_distance(Vec3 point,Vec3 a,Vec3 b) {
    const auto delta=b-a;
    const float fraction=std::clamp(dot(point-a,delta)/length_squared(delta),0.f,1.f);
    return distance(point,a+delta*fraction);
}
void quaternion_round_trip(Vec3 angles) {
    const auto original=rotation_from_euler(angles);
    const auto round_trip=rotation_from_euler(rotation_to_euler(original));
    for(const auto axis:{Vec3{1,0,0},Vec3{0,1,0},Vec3{0,0,1}})
        require(near(rotate(original,axis),rotate(round_trip,axis),.001f),
            "Euler/native basis must preserve the quaternion orientation, including vertical poses");
}
}
int main() {
    require(throw_lean(0)==0&&throw_lean(1)==0,"release begins and ends without an additive spine bend");
    require(throw_lean(-1)==0&&throw_lean(2)==0&&throw_lean(std::numeric_limits<float>::quiet_NaN())==0,
        "invalid or inactive release progress leaves the spine unchanged");
    float last_lean=0,peak_lean=0;
    for(unsigned frame=0;frame<=60;++frame){
        const float lean=throw_lean(frame/60.f);
        require(std::isfinite(lean)&&lean>=0&&lean<=.2618f,"the throw bends forward by at most fifteen degrees");
        require(std::abs(lean-last_lean)<.025f,"the spine gesture has no visible angular jump between frames");
        last_lean=lean;peak_lean=std::max(peak_lean,lean);
    }
    require(peak_lean>.1396f,"the spine bend is visible without folding the torso");
    // Reaching around the head must preserve both bone lengths and select the
    // outward elbow solution across Ratchet's facing directions.
    for(int i=0;i<32;++i) {
        const float yaw=i*6.283185307f/32;
        const Vec3 outward{std::sin(yaw),-std::cos(yaw),0};
        const Vec3 forward{std::cos(yaw),std::sin(yaw),0};
        const Vec3 shoulder=outward*.18f+Vec3{10,20,1.f};
        const Vec3 skull{10,20,1.23f};
        const auto goal=arm_goal(shoulder,.207f,.17f,outward,forward,1);
        require(bool(goal),"valid native limb dimensions must yield a pose");
        require(std::abs(distance(shoulder,goal->elbow)-.207f)<.0002f,"upper arm length must be preserved");
        require(std::abs(distance(goal->elbow,goal->hand)-.17f)<.0002f,"forearm length must be preserved");
        require(dot(goal->elbow-shoulder,outward)>.12f,"elbow must bend outside the torso");
        require(dot(goal->hand-shoulder,outward)>.20f,"wrist must stay outside the ear");
        require(goal->hand.z>shoulder.z+.20f,"wrist must be lifted above the shoulder");
        require(segment_distance(skull,shoulder,goal->elbow)>.21f,"upper arm must clear the head volume");
        require(segment_distance(skull,goal->elbow,goal->hand)>.21f,"forearm must clear the head volume");
    }
    require(!arm_goal({},0,.2f,{1,0,0},{0,1,0},1),"zero upper-arm length must be rejected");
    for(unsigned facing=0;facing<32;++facing){
        const float yaw=facing*6.283185307f/32;
        const Vec3 outward{std::sin(yaw),-std::cos(yaw),0},forward{std::cos(yaw),std::sin(yaw),0};
        const Vec3 shoulder=outward*.18f+Vec3{10,20,1.f},skull{10,20,1.23f};
        const auto raised=arm_goal(shoulder,.207f,.17f,outward,forward,1);
        auto previous=raised->hand;
        for(unsigned frame=0;frame<=30;++frame){
            const auto pose=throw_goal(shoulder,.207f,.17f,outward,forward,frame/30.f);
            require(bool(pose),"every throw trajectory sample has a reachable two-bone pose");
            require(std::abs(distance(shoulder,pose->elbow)-.207f)<.0002f&&std::abs(distance(pose->elbow,pose->hand)-.17f)<.0002f,
                "the forward throw preserves both native bone lengths");
            require(segment_distance(skull,shoulder,pose->elbow)>.19f&&segment_distance(skull,pose->elbow,pose->hand)>.19f,
                "the throwing elbow and forearm stay outside the head");
            require(distance(previous,pose->hand)<.04f,"the gesture has no hand teleport between frames");
            if(!frame)require(near(pose->hand,raised->hand),"release starts at the exact held pose");
            if(frame==18)require(dot(pose->hand-shoulder,forward)>.28f,"the release visibly reaches forward");
            previous=pose->hand;
        }
    }
    require(!arm_goal({},.2f,-1,{1,0,0},{0,1,0},1),"negative lower-arm length must be rejected");
    require(!arm_goal({},std::numeric_limits<float>::quiet_NaN(),.2f,{1,0,0},{0,1,0},1),"nonfinite limb dimensions must be rejected");

    // These are geometric expectations, not implementation snapshots: a point
    // captured on a torso/limb must remain exactly on the drill tip throughout
    // lift and rotation, independent of which body part was struck.
    for(const auto original_angles:{Vec3{},Vec3{.3f,-.4f,1.2f},Vec3{-.6f,.2f,-2.4f}}) {
        const Vec3 origin{15,26,4};
        const auto original=rotation_from_euler(original_angles);
        for(const auto local:{Vec3{.15f,.20f,.4f},Vec3{-.35f,.1f,.8f},Vec3{0,-.1f,.15f}}) {
            const Vec3 contact=origin+rotate(original,local);
            const auto grip=capture_grip(origin,original_angles,contact,{1,0,0},.7f);
            const auto initial=attach_to_tip(grip,contact,.7f,0);
            require(near(initial.origin,origin),"capture must preserve the original root when the tip is at the contact");
            require(near(grip.local_point,local),"capture point must be stored in the enemy's local space");
            for(int n=0;n<48;++n) {
                const float angle=.7f+n*.287f;
                const float lift=std::min(1.f,n/12.f);
                const Vec3 tip{17+std::sin(angle)*.3f,22+std::cos(angle)*.2f,5+lift};
                const auto posed=attach_to_tip(grip,tip,angle,lift);
                require(finite(posed.origin)&&finite(posed.angles),"attachment pose must remain finite");
                require(near(posed.origin+rotate(posed.rotation,local),tip),"the struck body point must stay on the tip through every spin angle");
                const auto native_rotation=rotation_from_euler(posed.angles);
                if(!near(posed.origin+rotate(native_rotation,local),tip,.001f))
                    std::fprintf(stderr,"reproduction: original=(%.6f,%.6f,%.6f) spin=%.6f lift=%.6f local=(%.3f,%.3f,%.3f) euler=(%.6f,%.6f,%.6f) contact-error=%.6f\n",original_angles.x,original_angles.y,original_angles.z,angle,lift,local.x,local.y,local.z,posed.angles.x,posed.angles.y,posed.angles.z,distance(posed.origin+rotate(native_rotation,local),tip));
                require(near(posed.origin+rotate(native_rotation,local),tip,.001f),"the native Euler pose must keep the same contact as the quaternion pose");
                if(lift==1) {
                    const auto approach_local=rotate({-original[0],-original[1],-original[2],original[3]},{1,0,0});
                    require(near(unit(rotate(posed.rotation,approach_local)),{0,0,1},.001f),"the grabbed surface must be aligned with the upright drill after lifting");
                }
            }
        }
    }
    for(const auto from:{Vec3{1,0,0},Vec3{-1,0,0},Vec3{0,1,0},Vec3{0,-1,0},Vec3{0,0,1},Vec3{0,0,-1}}) {
        const auto q=shortest_arc(from,{0,0,1});
        require(near(rotate(q,from),{0,0,1}),"shortest arc must work for axial and opposite directions");
    }
    for(const auto angles:{Vec3{.2f,.4f,-1.1f},Vec3{0,1.570796327f,0},Vec3{.3f,1.570796327f,-1.1f},Vec3{-.7f,-1.570796327f,.8f}})
        quaternion_round_trip(angles);
    for(int klass:{500,501,502,505,507,508,509,510,511})require(destructible_crate_class(klass),"native crate class must accept impacts");
    for(int klass:{-1,0,11,149,348,500-1,503,504,506,512,531,532,533,540,871,1218,1220})
        require(!destructible_crate_class(klass),"unrelated scenery, pickups, ships and fragments must not be treated as crates");
    std::printf("pose: %d checks passed (outward elbow, limb lengths, capture contact, tilt/spin, native rotation, crate eligibility)\n",checks);
}
