// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_fragment_sources.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <vector>
using namespace salvage;
namespace {
unsigned checks;
void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"fragment sources: %s\n",message);std::exit(1);}}
bool near(Vec3 a,Vec3 b){return length(a-b)<.0001f;}
struct Accepted {FragmentOrigin origin;unsigned actor;FragmentMotion motion;};
struct Fixture;Fixture* live;
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
 openrac_api api{};int level=14;bool accepting=true,reset_during_adoption=false;unsigned attempts{};
 std::vector<Accepted> accepted;NativeFragmentSources sources;
 static constexpr unsigned Owner=0x40000,Actor=0x41000,Vars=0x92000;
 template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
 static unsigned relocate(unsigned x){return x;}static unsigned relocate_level(int,unsigned x){return x;}static int loaded(){return live->level;}
 static const openrac_api* prepare(Fixture* f){live=f;f->api.memory=f->ram.data();f->api.relocate_data=relocate;f->api.relocate_level_data=relocate_level;f->api.loaded_level=loaded;return &f->api;}
 Fixture():sources(prepare(this),[this](FragmentOrigin origin,unsigned actor,const FragmentMotion& motion){
  ++attempts;if(!accepting)return false;accepted.push_back({origin,actor,motion});if(reset_during_adoption)sources.reset();return true;
 }){put(NativeMemory::Delta,.02f);}
 void actor(unsigned at,unsigned klass,unsigned vars,unsigned update){
  std::memset(ram.data()+at,0,256);put<unsigned char>(at+0x20,0);put<unsigned short>(at+0xa6,static_cast<unsigned short>(klass));
  put(at+0x24,0x90000u);put(at+0x78,vars);put(at+0x74,update);put<unsigned short>(at+0xa8,1);put(at+0x10,Vec3{12,10,1});
 }
 void factory(FragmentFactory factory,unsigned source,int mode=1){
  unsigned klass=0,owner_offset=0x24,spin_offset=0x10;
  switch(factory){case FragmentFactory::FlyingDebris81:klass=81;owner_offset=0x20;spin_offset=0x14;break;
   case FragmentFactory::PlasmabotDebris325:klass=325;break;
   case FragmentFactory::PathDebris403:klass=403;owner_offset=0;spin_offset=0x20;break;
   case FragmentFactory::BomberJetDebris1510:klass=1510;level=1;break;}
  actor(Owner,source,0x91000,0x250000);actor(Actor,klass,Vars,0x280000);
  std::memset(ram.data()+Vars,0,128);put(Vars,Vec3{.1f,.2f,.3f});put(Vars+spin_offset,Vec3{.04f,.06f,.08f});
  put(Vars+owner_offset,Owner);
  if(factory==FragmentFactory::FlyingDebris81)put<unsigned>(Vars+0x10,0xffff00c8); // Not angular velocity: timer and light slot.
  if(factory==FragmentFactory::PathDebris403){put<unsigned char>(Actor+0x20,static_cast<unsigned char>(mode));put(Vars+4,mode==0?90:200);put(Vars+0x30,Vec3{.1f,.2f,.3f});}
 }
};
}
int main(){
 {
  std::size_t count{};const auto* bindings=fragment_factory_bindings(count);std::set<std::string> hooks,symbols;
  check(bindings&&count==4,"only four source-backed native factory bindings are exported");
  for(std::size_t i=0;i<count;++i)check(hooks.insert(bindings[i].hook_id).second&&symbols.insert(bindings[i].symbol).second,"factory hook identities and symbols are unique");
  check(!std::strcmp(bindings[0].signature,"u32(u32,f32,u32)")&&!std::strcmp(bindings[3].signature,"u32(u32,u32,f32)"),"mixed-float ABI positions remain distinct for minibomber and bomber jet");
 }
 for(const auto factory:{FragmentFactory::FlyingDebris81,FragmentFactory::PlasmabotDebris325,FragmentFactory::PathDebris403,FragmentFactory::BomberJetDebris1510}){
  Fixture f;const unsigned owner=factory==FragmentFactory::FlyingDebris81?31:factory==FragmentFactory::BomberJetDebris1510?688:28;
  f.factory(factory,owner);
  check(f.sources.observe(factory,Fixture::Owner,Fixture::Actor)==FragmentObservation::Adopted,"actual source-backed factory result is adopted immediately when ballistic");
  check(f.accepted.size()==1&&f.accepted[0].origin.source_class==owner&&f.accepted[0].actor==Fixture::Actor,"adoption copies actual owner class and native fragment identity");
  check(near(f.accepted[0].motion.velocity,{5,10,15})&&near(f.accepted[0].motion.angular_velocity,{2,3,4}),"factory-specific motion fields normalize native per-frame displacement to seconds");
  f.sources.frame();check(f.sources.pending()==0&&f.attempts==1,"accepted factory result is not adopted a second time next frame");
 }
 {
  Fixture f;f.factory(FragmentFactory::PathDebris403,211,0);const float nan=std::numeric_limits<float>::quiet_NaN();
  f.put(Fixture::Vars+0x30,Vec3{nan,nan,nan});f.put(Fixture::Vars+0x20,Vec3{nan,nan,nan});
  check(f.sources.observe(FragmentFactory::PathDebris403,Fixture::Owner,Fixture::Actor,0)==FragmentObservation::Pending,"mode0 detached racer body retains its native spline phase");
  f.put(Fixture::Vars+4,89);f.put(Fixture::Actor+0x10,Vec3{12.2f,10.1f,1.2f});f.put<unsigned short>(Fixture::Actor+0xa8,2);f.sources.frame();
  f.put(Fixture::Vars+4,3);f.put(Fixture::Actor+0x10,Vec3{12.4f,10.2f,1.4f});f.put<unsigned short>(Fixture::Actor+0xa8,3);f.sources.frame();
  check(f.attempts==0&&f.sources.pending()==1,"deferred spline is never decoded from uninitialized ballistic fields");
  // Native owner is already gone/recycled. Only the immutable provenance copy is relevant.
  f.actor(Fixture::Owner,623,0x95000,0x260000);
  f.put(Fixture::Vars+4,2);f.put(Fixture::Actor+0x10,Vec3{12.6f,10.3f,1.6f});f.put<unsigned short>(Fixture::Actor+0xa8,4);f.sources.frame();
  check(f.accepted.size()==1&&f.accepted[0].origin.source_class==211&&f.accepted[0].origin.fragment_class==403,"deferred part keeps its original robot identity after owner reuse");
  check(near(f.accepted[0].motion.velocity,{10,5,10})&&near(f.accepted[0].motion.angular_velocity,{}),"spline expiry takeover derives last native displacement including vertical movement and zero spin");
  check(f.sources.pending()==0,"mode0 takeover finishes before native secondary explosion without awaiting nonexistent state1 transition");
 }
 {
  Fixture f;f.factory(FragmentFactory::PathDebris403,211,0);f.sources.observe(FragmentFactory::PathDebris403,Fixture::Owner,Fixture::Actor,0);
  f.sources.reset();f.put(Fixture::Vars+4,2);f.sources.frame();
  check(f.attempts==0&&f.sources.pending()==0,"same-planet reload reset invalidates every previous native allocation generation");
 }
 {
  Fixture f;f.factory(FragmentFactory::PathDebris403,211,0);f.sources.observe(FragmentFactory::PathDebris403,Fixture::Owner,Fixture::Actor,0);
  f.level=1;f.put(Fixture::Vars+4,2);f.sources.frame();check(f.attempts==0&&f.sources.pending()==0,"planet change drops pending native handles before they can be reused");
 }
 for(unsigned changed=0;changed<6;++changed){
  Fixture f;f.factory(FragmentFactory::PathDebris403,211,0);f.sources.observe(FragmentFactory::PathDebris403,Fixture::Owner,Fixture::Actor,0);
  f.put(Fixture::Vars+4,2);
  switch(changed){case 0:f.put(Fixture::Actor+0x24,0x96000u);break;case 1:f.put(Fixture::Actor+0x78,0x97000u);break;
   case 2:f.put(Fixture::Actor+0x74,0x290000u);break;case 3:f.put<unsigned char>(Fixture::Actor+0x20,0xfe);break;
   case 4:f.put(Fixture::Vars,0x42000u);break;case 5:f.put<unsigned short>(Fixture::Actor+0xa8,0);break;}
  f.sources.frame();check(f.attempts==0&&f.sources.pending()==0,"class/pvars/update/state/owner-reference/counter changes reject stale deferred fragment handles");
 }
 {
  Fixture f;f.factory(FragmentFactory::PathDebris403,211,0);f.put<unsigned short>(Fixture::Actor+0xa8,0xffff);f.sources.observe(FragmentFactory::PathDebris403,Fixture::Owner,Fixture::Actor,0);
  f.put<unsigned short>(Fixture::Actor+0xa8,0);f.put(Fixture::Vars+4,2);f.sources.frame();
  check(f.accepted.size()==1,"legitimate matrix-counter wrap is not confused with recycled-object reset");
 }
 {
  Fixture f;f.factory(FragmentFactory::PathDebris403,211,0);f.sources.observe(FragmentFactory::PathDebris403,Fixture::Owner,Fixture::Actor,0);
  f.factory(FragmentFactory::PathDebris403,28,1);check(f.sources.observe(FragmentFactory::PathDebris403,Fixture::Owner,Fixture::Actor,1)==FragmentObservation::Adopted,"a new observed factory allocation replaces pending same-address generation");
  f.sources.frame();check(f.accepted.size()==1&&f.accepted[0].origin.source_class==28,"recycled same-class/pvars slot cannot adopt the previous robot origin");
 }
 {
  Fixture f;f.factory(FragmentFactory::FlyingDebris81,31);f.accepting=false;
  check(f.sources.observe(FragmentFactory::FlyingDebris81,Fixture::Owner,Fixture::Actor)==FragmentObservation::Pending&&f.sources.pending()==1,"temporary native adoption failure retains the observation for retry");
  f.accepting=true;f.sources.frame();check(f.accepted.size()==1&&f.attempts==2&&f.sources.pending()==0,"recovery accepts one part exactly once after the failed attempt");
 }
 {
  Fixture f;f.factory(FragmentFactory::PlasmabotDebris325,28);f.put(0x15f6a8,1);
  check(f.sources.observe(FragmentFactory::PlasmabotDebris325,Fixture::Owner,Fixture::Actor)==FragmentObservation::Pending,"menu context defers native adoption");
  f.sources.frame();check(f.attempts==0,"paused frame cannot adopt native actors into the menu pool");
  f.put(0x15f6a8,0);f.sources.frame();check(f.accepted.size()==1,"gameplay resumes deferred adoption safely");
 }
 {
  Fixture f;f.factory(FragmentFactory::PlasmabotDebris325,1446);const auto previous=f.ram;
  check(f.sources.observe(FragmentFactory::PlasmabotDebris325,Fixture::Owner,Fixture::Actor)==FragmentObservation::Rejected&&f.ram==previous,"NPC or decorative source is neither captured nor modified");
  f.factory(FragmentFactory::PlasmabotDebris325,28);f.put<unsigned short>(Fixture::Actor+0xa6,81);
  check(f.sources.observe(FragmentFactory::PlasmabotDebris325,Fixture::Owner,Fixture::Actor)==FragmentObservation::Rejected,"unexpected native result geometry is not inferred from source eligibility");
  check(f.sources.observe(FragmentFactory::FlyingDebris81,0,Fixture::Actor)==FragmentObservation::Rejected,"missing native source cannot manufacture provenance");
 }
 {
  Fixture f;f.factory(FragmentFactory::PathDebris403,28,0);
  check(f.sources.observe(FragmentFactory::PathDebris403,Fixture::Owner,Fixture::Actor,0)==FragmentObservation::Rejected,"unsupported source/mode combination requires source evidence rather than a broad debris hook");
 }
 {
  Fixture f;f.factory(FragmentFactory::PlasmabotDebris325,28);f.put(NativeMemory::Delta,0.f);
  check(f.sources.observe(FragmentFactory::PlasmabotDebris325,Fixture::Owner,Fixture::Actor)==FragmentObservation::Pending&&f.attempts==0,"zero frame duration cannot generate invalid velocity");
  f.put(NativeMemory::Delta,.02f);f.sources.frame();check(f.accepted.size()==1,"valid frame timing recovers pending ballistic observation");
 }
 {
  Fixture f;f.factory(FragmentFactory::PlasmabotDebris325,28);f.reset_during_adoption=true;
  check(f.sources.observe(FragmentFactory::PlasmabotDebris325,Fixture::Owner,Fixture::Actor)==FragmentObservation::Adopted&&f.sources.pending()==0,"adopter-triggered reset does not invalidate a live iteration or duplicate acceptance");
 }
 std::printf("fragment sources: %u checks passed\n",checks);
}
