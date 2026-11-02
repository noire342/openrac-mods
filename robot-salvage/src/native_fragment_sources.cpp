// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_fragment_sources.hpp"
#include "builtin_catalog.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace salvage {
namespace {
constexpr FragmentFactoryBinding Bindings[]{
 {FragmentFactory::FlyingDebris81,"rac1.moby.spawn_flying_debris","func_L14_002BC560","u32(u32,f32,u32)",81},
 {FragmentFactory::PlasmabotDebris325,"rac1.moby.spawn_plasmabot_death_debris","func_L14_002DFE98","u32(u32,u32)",325},
 {FragmentFactory::PathDebris403,"rac1.moby.spawn_path_or_ballistic_debris","func_L14_002E1570","u32(u32,u32,i32)",403},
 {FragmentFactory::BomberJetDebris1510,"rac1.moby.spawn_bomber_jet_debris","func_L01_0030D248","u32(u32,u32,f32)",1510},
};
const FragmentFactoryBinding* binding(FragmentFactory factory) noexcept {
 for(const auto& item:Bindings)if(item.factory==factory)return &item;
 return nullptr;
}
unsigned owner_offset(FragmentFactory factory) noexcept {
 switch(factory){case FragmentFactory::FlyingDebris81:return 0x20;
  case FragmentFactory::PathDebris403:return 0;
  default:return 0x24;}
}
bool source_pair(FragmentFactory factory,unsigned source,int mode) noexcept {
 switch(factory){
  case FragmentFactory::FlyingDebris81:return source==31;
  case FragmentFactory::PlasmabotDebris325:return source==28||source==211;
  case FragmentFactory::PathDebris403:return (source==28&&mode==1)||(source==211&&mode==0);
  case FragmentFactory::BomberJetDebris1510:return source==688;
 }
 return false;
}
}
const FragmentFactoryBinding* fragment_factory_bindings(std::size_t& count) noexcept {
 count=sizeof Bindings/sizeof Bindings[0];return Bindings;
}
NativeFragmentSources::NativeFragmentSources(const openrac_api* api,Adopter adopter)
 :NativeMemory(api),adopter_(std::move(adopter)){}

void NativeFragmentSources::reset() noexcept {
 pending_.clear();level_=-1;
 if(++visit_==0)visit_=1;
}
bool NativeFragmentSources::level_ready(){
 if(!api_||!api_->memory||!api_->loaded_level||!api_->relocate_data||!api_->relocate_level_data||!adopter_)return false;
 const int level=api_->loaded_level();
 if(level<0||level>18){if(level_!=-1||!pending_.empty())reset();return false;}
 if(level!=level_){reset();level_=level;}
 return true;
}
FragmentObservation NativeFragmentSources::observe(FragmentFactory factory,unsigned owner,unsigned actor,int mode){
 // Every actual factory result starts a new observed allocation generation,
 // even when an engine slot has the same class/pvars as its previous occupant.
 pending_.erase(actor);
 if(!level_ready()||!valid(owner,256)||!valid(actor,256))return FragmentObservation::Rejected;
 const auto descriptor=binding(factory);if(!descriptor)return FragmentObservation::Rejected;
 const auto source=read<unsigned short>(owner+0xa6),klass=read<unsigned short>(actor+0xa6);
 if(klass!=descriptor->fragment_class||!source_pair(factory,source,mode)||!builtin_part(source,klass))return FragmentObservation::Rejected;
 const auto pc=read<unsigned>(actor+0x24),pv=read<unsigned>(actor+0x78),update=read<unsigned>(actor+0x74);
 if(read<unsigned char>(actor+0x20)>=0x80||!valid(pc,0x48)||!valid(pv,0x40)||!update||
    read<unsigned>(pv+owner_offset(factory))!=owner||!finite(read<Vec3>(actor+0x10))||
    generation_==std::numeric_limits<std::uint64_t>::max())return FragmentObservation::Rejected;
 if(factory==FragmentFactory::PathDebris403&&read<unsigned char>(actor+0x20)!=unsigned(mode))return FragmentObservation::Rejected;
 Pending pending;
 pending.factory=factory;pending.origin={source,klass};
 pending.token={actor,pc,pv,update,owner,++generation_,visit_,read<unsigned short>(actor+0xa8)};
 pending.last_position=read<Vec3>(actor+0x10);
 pending_.emplace(actor,pending);
 if(read<int>(this->mode())!=0)return FragmentObservation::Pending;
 return try_adopt(actor)?FragmentObservation::Adopted:FragmentObservation::Pending;
}
bool NativeFragmentSources::valid_pending(Pending& p)const{
 const auto& token=p.token;
 if(token.visit!=visit_||!valid(token.address,256)||read<unsigned char>(token.address+0x20)>=0x80||
    read<unsigned>(token.address+0x24)!=token.pclass||read<unsigned>(token.address+0x78)!=token.pvars||
    read<unsigned>(token.address+0x74)!=token.update||read<unsigned short>(token.address+0xa6)!=p.origin.fragment_class||
    read<unsigned>(token.pvars+owner_offset(p.factory))!=token.owner||!finite(read<Vec3>(token.address+0x10)))return false;
 if(p.factory==FragmentFactory::PathDebris403&&read<unsigned char>(token.address+0x20)>1)return false;
 // Matrix rebuilds increment this u16. A reset backwards (except a genuine
 // wrap) indicates an unobserved reinitialization, not the original fragment.
 const auto counter=read<unsigned short>(token.address+0xa8);
 if(static_cast<unsigned short>(counter-token.matrix_counter)>0x7fff)return false;
 p.token.matrix_counter=counter;
 return true;
}
bool NativeFragmentSources::due(const Pending& p)const{
 if(p.factory!=FragmentFactory::PathDebris403)return true;
 const auto state=read<unsigned char>(p.token.address+0x20);
 if(state==1)return true;
 if(state!=0)return false;
 // The original mode0 NEVER becomes ballistic: it follows its spline, then
 // explodes/deletes directly. Take ownership just before that second burst.
 const auto remaining=read<int>(p.token.pvars+4);
 return remaining>=0&&remaining<=2;
}
bool NativeFragmentSources::motion(const Pending& p,FragmentMotion& result)const{
 const float dt=read<float>(g(Delta));
 if(!std::isfinite(dt)||dt<.00001f||dt>.1f)return false;
 Vec3 velocity,angular;
 const auto vars=p.token.pvars;
 if(p.factory==FragmentFactory::PathDebris403){
  const auto state=read<unsigned char>(p.token.address+0x20);
  if(state==0){velocity=(read<Vec3>(p.token.address+0x10)-p.last_position)*(1.f/dt);angular={};}
  else if(state==1){velocity=read<Vec3>(vars+0x30)*(1.f/dt);angular=read<Vec3>(vars+0x20)*(1.f/dt);}
  else return false;
 }else{
  velocity=read<Vec3>(vars)*(1.f/dt);
  angular=read<Vec3>(vars+(p.factory==FragmentFactory::FlyingDebris81?0x14:0x10))*(1.f/dt);
 }
 if(!finite(velocity)||!finite(angular))return false;
 result={velocity,angular};return true;
}
bool NativeFragmentSources::try_adopt(unsigned actor){
 const auto found=pending_.find(actor);if(found==pending_.end())return false;
 auto& pending=found->second;
 if(!valid_pending(pending)){pending_.erase(found);return false;}
 FragmentMotion decoded;
 if(!due(pending)||!motion(pending,decoded)){
  pending.last_position=read<Vec3>(actor+0x10);return false;
 }
 const auto generation=pending.token.generation;
 const auto origin=pending.origin;
 // Callback may reset tracking or observe another factory result. Only erase
 // the completed allocation, never a replacement occupying the same address.
 const bool accepted=adopter_(origin,actor,decoded);
 const auto current=pending_.find(actor);
 if(current!=pending_.end()&&current->second.token.generation==generation){
  if(accepted)pending_.erase(current);
  else current->second.last_position=read<Vec3>(actor+0x10);
 }
 return accepted;
}
void NativeFragmentSources::frame(){
 if(!level_ready()||read<int>(mode())!=0)return;
 std::vector<unsigned> actors;actors.reserve(pending_.size());
 for(const auto& [actor,record]:pending_){(void)record;actors.push_back(actor);}
 for(const auto actor:actors)try_adopt(actor);
}

} // namespace salvage
