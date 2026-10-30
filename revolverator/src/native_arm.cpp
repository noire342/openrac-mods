// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_arm.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace revolverator {
namespace {
constexpr Quaternion identity{0,0,0,1};
Vec3 position(const std::array<float,16>& matrix) { return {matrix[12],matrix[13],matrix[14]}; }
Vec3 axis(const std::array<float,16>& matrix,unsigned row) {
    return unit({matrix[row*4],matrix[row*4+1],matrix[row*4+2]});
}
Vec3 local_vector(const std::array<float,16>& frame,Vec3 world) {
    return {dot(world,axis(frame,0)),dot(world,axis(frame,1)),dot(world,axis(frame,2))};
}
Vec3 world_vector(const std::array<float,16>& frame,Vec3 local) {
    return unit(axis(frame,0)*local.x+axis(frame,1)*local.y+axis(frame,2)*local.z);
}
Vec3 cross(Vec3 a,Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Quaternion product(Quaternion a,Quaternion b) {
    Quaternion q{a[3]*b[0]+b[3]*a[0]+a[1]*b[2]-a[2]*b[1],
        a[3]*b[1]+b[3]*a[1]+a[2]*b[0]-a[0]*b[2],
        a[3]*b[2]+b[3]*a[2]+a[0]*b[1]-a[1]*b[0],
        a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
    float norm=0;for(float v:q)norm+=v*v;
    if(!(norm>.000001f)||!std::isfinite(norm))return identity;
    for(auto& v:q)v/=std::sqrt(norm);
    return q;
}
Quaternion delta_rotation(Vec3 v) {
    const float angle=std::sqrt(length_squared(v));
    if(angle<.000001f)return identity;
    const float s=std::sin(angle*.5f)/angle;
    return {v.x*s,v.y*s,v.z*s,std::cos(angle*.5f)};
}
struct Stack {
    const openrac_api* api; unsigned saved{},address{};
    explicit Stack(const openrac_api* a):api(a),address(a->stack_push(64,&saved)){}
    ~Stack(){if(address)api->stack_pop(saved);}
};
}
NativeArm::NativeArm(const openrac_api* api,GuestArena& arena):Memory(api),guest_(arena),
    bone_(function<Bone>("FUN_0020cca8")),basis_(function<Basis>("FUN_001fa030")) {}
NativeArm::~NativeArm(){reset();}
bool NativeArm::attached() const {
    return owner_&&api_->loaded_level()==level_&&valid(owner_,256)&&
        read<unsigned>(owner_+0x24)==klass_&&read<unsigned>(owner_+0x38)==stamp_;
}
bool NativeArm::attach(unsigned owner) {
    if(owner_==owner&&attached())return true;
    reset();
    const auto klass=read<unsigned>(owner+0x24);
    if(!valid(owner,256)||!valid(klass,0x30)||read<unsigned char>(klass+8)!=111)return false;
    if(arena_level_!=api_->loaded_level()){arena_=0;arena_level_=api_->loaded_level();}
    if(!arena_)arena_=guest_.allocate(512);
    if(!valid(arena_,512)){arena_=0;return false;}
    std::memset(api_->memory+arena_,0,512);
    owner_=owner;klass_=klass;stamp_=read<unsigned>(owner+0x38);level_=api_->loaded_level();
    // Joint 52 is the clavicle. Leave it untouched; rotate the actual upper arm
    // (53) and forearm (54) independently around a deliberately outward elbow.
    for(unsigned i=0;i<3;++i){
        const unsigned node=arena_+i*64;
        write<unsigned char>(node+1,1);write(node+4,0x70000000u+(i==2?56:53+i)*64);
        write(node+8,read<unsigned>(owner+0x64));write(node+0x10,identity);
        write(node+0x20,Vec3{1,1,1});write(owner+0x64,node);
    }
    return true;
}
std::optional<NativeArm::Matrix> NativeArm::sample(unsigned joint) {
    if(!attached()||joint>56||joint<52)return {};
    const unsigned original=read<unsigned>(klass_+0x1c),table=arena_+192,chain=arena_+272;
    if(!valid(original,68))return {};
    std::memcpy(api_->memory+table,api_->memory+original,68);
    const unsigned count=8+(joint-52);
    write<unsigned char>(chain,static_cast<unsigned char>(count));
    write<unsigned short>(chain+2,1);
    for(unsigned i=0;i<7;++i)write<unsigned char>(chain+4+i,static_cast<unsigned char>(i));
    for(unsigned i=52;i<=joint;++i)write<unsigned char>(chain+11+i-52,static_cast<unsigned char>(i));
    write<unsigned char>(chain+4+count,static_cast<unsigned char>(joint));
    write(table+4,chain);
    Stack stack(api_);if(!stack.address)return {};
    // BuildBoneTransform accepts a chain index, not a joint. Supply a private,
    // temporary chain for this synchronous call; restore the class before any
    // update/render callback can observe it. All other chain entries are copied.
    struct Restore {
        NativeArm& self;unsigned at,old,replacement;
        ~Restore(){if(self.read<unsigned>(at)==replacement)self.write(at,old);}
    } restore{*this,klass_+0x1c,original,table};
    write(klass_+0x1c,table);
    bone_(owner_,0,stack.address);
    Matrix matrix{};std::memcpy(matrix.data(),api_->memory+stack.address,64);
    for(float value:matrix)if(!std::isfinite(value))return {};
    return matrix;
}
std::optional<NativeArm::Matrix> NativeArm::parent_frame(unsigned joint) {
    const auto rest=read<unsigned>(klass_+0x18);
    if(!valid(rest,joint*16+16))return {};
    const auto parent=read<unsigned>(rest+joint*16+12);
    if(parent<0x70000000u||(parent&63u))return {};
    return sample((parent-0x70000000u)/64);
}
bool NativeArm::aim(unsigned node,unsigned joint,unsigned endpoint,Vec3 target,float weight) {
    write(node+0x10,identity);
    const auto base=sample(joint),end=sample(endpoint),parent=parent_frame(joint);
    if(!base||!end||!parent)return false;
    const auto origin=position(*base);
    // Native mode-0 modifiers compose q_authored * q_modifier. Its row-vector
    // matrices therefore apply the correction in the PARENT frame, not in the
    // posed joint frame. Quaternion rows also turn opposite to shortest_arc's
    // active-vector convention, hence the deliberately reversed arguments.
    const auto rotation=shortest_arc(local_vector(*parent,target-origin),
        local_vector(*parent,position(*end)-origin));
    Quaternion best=identity;float score=length_squared(position(*end)-target);
    write(node+0x10,rotation);
    if(const auto result=sample(endpoint))if(length_squared(position(*result)-target)<score)best=rotation;
    rotations_[(node-arena_)/64]=best;write(node+0x10,blend_rotation(best,weight));
    return true;
}
bool NativeArm::turn_wrist(float weight) {
    const unsigned node=arena_+128;
    write(node+0x10,identity);const auto hand=sample(56),parent=parent_frame(56);
    if(!hand||!parent||!grip_ready_)return false;
    const auto barrel=grip_axes_[1]*-1;
    const auto wanted=local_vector(*parent,{0,0,1});
    Quaternion best=identity;
    Matrix measured=*hand;
    float score=1-world_vector(measured,barrel).z;
    const auto candidate=shortest_arc(wanted,local_vector(*parent,world_vector(measured,barrel)));
    write(node+0x10,candidate);
    if(const auto posed=sample(56))if(1-world_vector(*posed,barrel).z<score){
        score=1-world_vector(*posed,barrel).z;best=candidate;measured=*posed;
    }
    write(node+0x10,rotations_[2]);
    if(const auto posed=sample(56))if(1-world_vector(*posed,barrel).z<score){
        score=1-world_vector(*posed,barrel).z;best=rotations_[2];measured=*posed;
    }
    // Native keys are not always unit quaternions after their authored blend.
    // Consequently their composed rows can be sheared: a purely algebraic arc
    // can leave a visible residual. Refine against measured native matrices,
    // using a damped 3-axis Jacobian, with a strict bounded work budget.
    for(unsigned iteration=0;iteration<5&&score>.000001f;++iteration){
        const auto current=world_vector(measured,barrel);std::array<Vec3,3> jacobian{};
        bool complete=true;
        for(unsigned i=0;i<3;++i){
            Vec3 step{};if(i==0)step.x=.002f;else if(i==1)step.y=.002f;else step.z=.002f;
            write(node+0x10,product(best,delta_rotation(step)));
            const auto trial=sample(56);if(!trial){complete=false;break;}
            jacobian[i]=(world_vector(*trial,barrel)-current)*500.f;
        }
        if(!complete)break;
        float system[3][4]{};
        for(unsigned r=0;r<3;++r){
            for(unsigned c=0;c<3;++c)system[r][c]=dot(jacobian[r],jacobian[c])+(r==c?.0001f:0);
            system[r][3]=dot(jacobian[r],Vec3{0,0,1}-current);
        }
        for(unsigned col=0;col<3;++col){
            unsigned pivot=col;for(unsigned r=col+1;r<3;++r)if(std::abs(system[r][col])>std::abs(system[pivot][col]))pivot=r;
            for(unsigned c=col;c<4;++c)std::swap(system[col][c],system[pivot][c]);
            const float scale=system[col][col];
            if(std::abs(scale)<.0000001f){complete=false;break;}
            for(unsigned c=col;c<4;++c)system[col][c]/=scale;
            for(unsigned r=0;r<3;++r)if(r!=col){
                const float factor=system[r][col];
                for(unsigned c=col;c<4;++c)system[r][c]-=factor*system[col][c];
            }
        }
        if(!complete)break;
        Vec3 step{system[0][3],system[1][3],system[2][3]};
        if(!finite(step))break;
        const float magnitude=std::sqrt(length_squared(step));if(magnitude>.6f)step=step*(.6f/magnitude);
        bool improved=false;
        for(unsigned attempt=0;attempt<4;++attempt){
            const auto next=product(best,delta_rotation(step));
            write(node+0x10,next);const auto posed=sample(56);if(!posed)break;
            const float error=1-world_vector(*posed,barrel).z;
            if(error<score){score=error;best=next;measured=*posed;improved=true;break;}
            step=step*.5f;
        }
        if(!improved)break;
    }
    rotations_[2]=best;write(node+0x10,blend_rotation(best,weight));return true;
}
bool NativeArm::track_weapon(unsigned weapon) {
    if(weapon_==weapon&&read<unsigned>(weapon+0x24)==weapon_class_&&read<unsigned>(weapon+0x38)==weapon_stamp_)return true;
    if(weapon_)return false;
    const auto klass=read<unsigned>(weapon+0x24);
    if(!valid(weapon,256)||!valid(klass,48)||read<unsigned char>(klass+8)!=5)return false;
    weapon_=weapon;weapon_class_=klass;weapon_stamp_=read<unsigned>(weapon+0x38);
    original_radius_=read<float>(weapon+12);owned_radius_=std::max(original_radius_,1280.f);
    // NativeRotor exclusively owns the weapon's modifier list and neutral root.
    // The arm owns only Ratchet's three arm joints and the held bounds radius.
    return true;
}
std::optional<Vec3> NativeArm::hold(unsigned owner,unsigned weapon,float yaw,float seconds) {
    if(!std::isfinite(seconds)||seconds<0||seconds>.1f||!std::isfinite(yaw)||!attach(owner)||!valid(weapon,256))return {};
    if(!track_weapon(weapon))return {};
    release_time_=0;raised_time_=std::min(.32f,raised_time_+seconds);yaw_=yaw;
    return align_weapon(weapon);
}
bool NativeArm::pose() {
    for(unsigned i=0;i<3;++i)write(arena_+i*64+0x10,identity);
    const auto shoulder=sample(53),elbow=sample(54),hand=sample(56);
    if(!shoulder||!elbow||!hand)return false;
    if(!grip_ready_){
        for(unsigned i=0;i<3;++i){
            const auto direction=unit(read<Vec3>(weapon_+0xc0+i*16));
            grip_axes_[i]=local_vector(*hand,direction);
        }
        grip_ready_=true;
    }
    const auto forward_axis=unit(read<Vec3>(owner_+0xc0));
    const float yaw=length_squared(forward_axis)>.5f?std::atan2(forward_axis.y,forward_axis.x):yaw_;
    const Vec3 outward{std::sin(yaw),-std::cos(yaw),0},forward{std::cos(yaw),std::sin(yaw),0};
    const auto goal=arm_goal(position(*shoulder),std::sqrt(length_squared(position(*elbow)-position(*shoulder))),
        std::sqrt(length_squared(position(*hand)-position(*elbow))),outward,forward,1);
    if(!goal)return false;
    const float t=std::clamp(raised_time_/.2f,0.f,1.f),weight=t*t*(3-2*t);
    if(!aim(arena_,53,54,goal->elbow,weight)||!aim(arena_+64,54,56,goal->hand,weight))return false;
    const float wrist=std::clamp((raised_time_-.08f)/.18f,0.f,1.f);
    return turn_wrist(wrist*wrist*(3-2*wrist));
}
std::optional<Vec3> NativeArm::align_weapon(unsigned weapon) {
    if(!attached()||weapon!=weapon_||!valid(weapon,256)||read<unsigned>(weapon+0x24)!=weapon_class_||
        read<unsigned>(weapon+0x38)!=weapon_stamp_)return {};
    // The authored animation advances after mods::frame. Re-solve against its
    // current keys here, inside Moby180.update before the render snapshot, so
    // a modifier computed for the previous frame cannot tilt the held shaft.
    if(!pose())return {};
    const auto wrist=sample(56);if(!wrist)return {};
    const Vec3 at=position(*wrist);
    write(weapon+0x10,at);
    // MobyProc stores bounds in 1/1024 units, while the position is in metres.
    // Writing the position directly to the sphere put it near world origin and
    // culled the held drill when the camera/enemy turned.
    write(weapon,at*1024.f);
    write(weapon+12,owned_radius_);
    const Vec3 x=unit({(*wrist)[0],(*wrist)[1],(*wrist)[2]}),
        y=unit({(*wrist)[4],(*wrist)[5],(*wrist)[6]}),z=unit({(*wrist)[8],(*wrist)[9],(*wrist)[10]});
    const auto axis=[&](Vec3 local){return unit(x*local.x+y*local.y+z*local.z);};
    // Authored quaternion interpolation may leave a slightly sheared joint
    // basis. A held rigid weapon needs an orthonormal basis before Euler
    // extraction; keep the solved shaft (Y) exact and remove only its shear.
    const auto iy=axis(grip_axes_[1]);
    auto ix=unit(axis(grip_axes_[0])-iy*dot(axis(grip_axes_[0]),iy));
    if(length_squared(ix)<.5f)ix=unit(cross(iy,axis(grip_axes_[2])));
    const auto iz=unit(cross(ix,iy));ix=unit(cross(iy,iz));
    const float pitch=std::asin(std::clamp(-ix.z,-1.f,1.f));
    const Vec3 angles=std::abs(ix.z)>.99999f?Vec3{0,pitch,std::atan2(-iy.x,iy.y)}:
        Vec3{std::atan2(iy.z,iz.z),pitch,std::atan2(ix.y,ix.x)};
    write(weapon+0x40,angles);
    basis_(weapon+0xc0,weapon+0x40);
    // Actual Hoven-model shaft axis and tip, including its offset from the grip.
    return at+unit(read<Vec3>(weapon+0xc0))*.013592358f
        -unit(read<Vec3>(weapon+0xd0))*.903450852f+unit(read<Vec3>(weapon+0xe0))*.203153592f;
}
void NativeArm::release(){if(attached())release_time_=.18f;}
void NativeArm::settle(float seconds) {
    if(release_time_<=0){reset();return;}
    if(!attached()){reset();return;}
    release_time_=std::max(0.f,release_time_-std::clamp(seconds,0.f,.1f));
    for(unsigned i=0;i<3;++i)write(arena_+i*64+0x10,blend_rotation(rotations_[i],release_time_/.18f));
    if(release_time_<=0)reset();
}
void NativeArm::unlink(unsigned owner,unsigned node) {
    unsigned link=owner+0x64;
    for(unsigned i=0;i<32;++i){
        const auto at=read<unsigned>(link);
        if(at==node){write(link,read<unsigned>(node+8));return;}
        if(!valid(at,64))return;link=at+8;
    }
}
void NativeArm::reset(bool discard) {
    if(attached()){
        for(unsigned i=0;i<3;++i)unlink(owner_,arena_+i*64);
    }
        if(api_->loaded_level()==level_&&weapon_&&valid(weapon_,256)&&read<unsigned>(weapon_+0x24)==weapon_class_&&read<unsigned>(weapon_+0x38)==weapon_stamp_){
            if(read<float>(weapon_+12)==owned_radius_)write(weapon_+12,original_radius_);
        }
    if(discard||api_->loaded_level()!=arena_level_){arena_=0;arena_level_=-1;}
    owner_=klass_=stamp_=weapon_=weapon_class_=weapon_stamp_=0;level_=-1;raised_time_=release_time_=yaw_=0;
    rotations_={identity,identity,identity};grip_ready_=false;grip_axes_={};original_radius_=owned_radius_=0;
}
}
