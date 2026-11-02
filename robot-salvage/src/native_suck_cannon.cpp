// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_suck_cannon.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace salvage {
namespace {
// Owned PAL config/overlays/functions.tsv: exact guest entry points for each
// overlay. Native function lookup verifies each one before installing a table.
constexpr std::array<unsigned,19> Grab{{0x2dcda8,0x305540,0x2e79a0,0x2d7ff8,0x2df068,0x312898,0x2ed990,0x308840,0x2feb38,0x2fe158,0x2d1958,0x305f60,0x2fd138,0x2ffdd0,0x2f92e0,0x2e1c90,0x2db6c0,0x2df618,0x2e7078}};
constexpr std::array<unsigned,19> Record{{0x2dcd40,0x3054d8,0x2e7938,0x2d7f90,0x2df000,0x312830,0x2ed928,0x3087d8,0x2fead0,0x2fe0f0,0x2d18f0,0x305ef8,0x2fd0d0,0x2ffd68,0x2f9278,0x2e1c28,0x2db658,0x2df5b0,0x2e7010}};
constexpr std::array<unsigned,19> Store{{0x2dcfd0,0x305768,0x2e7bc8,0x2d8220,0x2df290,0x312ac0,0x2edbb8,0x308a68,0x2fed60,0x2fe380,0x2d1b80,0x306188,0x2fd360,0x2ffff8,0x2f9508,0x2e1eb8,0x2db8e8,0x2df840,0x2e72a0}};
constexpr std::array<unsigned,19> Fire{{0x2dd0e0,0x305878,0x2e7cd8,0x2d8330,0x2df3a0,0x312bd0,0x2edcc8,0x308b78,0x2fee70,0x2fe490,0x2d1c90,0x306298,0x2fd470,0x300108,0x2f9618,0x2e1fc8,0x2db9f8,0x2df950,0x2e73b0}};
constexpr std::array<unsigned,19> Cancel{{0x2dd2d0,0x305a68,0x2e7ec8,0x2d8520,0x2df590,0x312dc0,0x2edeb8,0x308d68,0x2ff060,0x2fe680,0x2d1e80,0x306488,0x2fd660,0x3002f8,0x2f9808,0x2e21b8,0x2dbbe8,0x2dfb40,0x2e75a0}};
constexpr unsigned Slots=0x2050,Stored=0x2078,Incoming=0x207c;
bool argument(unsigned at,std::size_t size){return NativeMemory::valid(at,size)||(size<=0x100000&&at>=0xe0000000u&&at<=0xe0100000u-size);}
template<class T>std::optional<T> argument(const openrac_api* api,unsigned at){if(!argument(at,sizeof(T)))return {};T result{};std::memcpy(&result,api->memory+at,sizeof result);return result;}
float distance_xy(Vec3 a,Vec3 b){const float x=a.x-b.x,y=a.y-b.y;return std::sqrt(x*x+y*y);}
}
NativeSuckCannon::NativeSuckCannon(const openrac_api* api,GuestArena& arena,CannonWorldPort& world):NativeMemory(api),arena_(arena),world_(world){
 carried_=fn<int(*)(unsigned,unsigned)>("func_L00_002DDEA0");matrix_=fn<void(*)(unsigned)>("func_0020EEE8");remove_=fn<void(*)(unsigned)>("func_0020D678");ticks_=fn<int(*)(int)>("func_001F9850");
}
NativeSuckCannon::~NativeSuckCannon(){reset();}
unsigned NativeSuckCannon::capacity()const{return read<unsigned char>(g(0x13e620)+9)?10u:5u;}
bool NativeSuckCannon::same_actor(const Actor& a)const{
 if(!(level_==api_->loaded_level()&&valid(a.body.actor,256)&&arena_.owns(a.storage,a.bytes)&&a.bytes>=0x120&&
  read<unsigned>(a.body.actor+0x24)==a.clone&&
  read<unsigned>(a.body.actor+0x78)==a.body.pvars&&read<unsigned>(a.body.actor+0x74)==a.body.update))return false;
 // A native pool may recycle identical address/class/pvars values. A newly
 // materialized logical component is nevertheless a different generation.
 // Detached incoming/held/projectile actors intentionally have no descriptor.
 const auto current=world_.cannon_describe(a.body.actor);
 return !current||(current->fragment.id==a.body.fragment.id&&current->fragment.visit==a.body.fragment.visit);
}
bool NativeSuckCannon::valid_actor(const Actor& a)const{return !a.retired&&same_actor(a)&&read<unsigned char>(a.body.actor+0x20)<0x80;}
bool NativeSuckCannon::storage_referenced(const Actor& a)const{return level_==api_->loaded_level()&&valid(a.body.actor,256)&&read<unsigned>(a.body.actor+0x24)==a.clone;}
std::size_t NativeSuckCannon::tracked()const noexcept{return std::size_t(std::count_if(actors_.begin(),actors_.end(),[](const auto& item){return !item.second.retired;}));}
unsigned NativeSuckCannon::allocate(unsigned bytes,unsigned& allocated){
 auto found=free_.end();for(auto it=free_.begin();it!=free_.end();++it)if(it->bytes>=bytes&&(found==free_.end()||it->bytes<found->bytes))found=it;
 if(found!=free_.end()){const auto value=*found;free_.erase(found);allocated=value.bytes;return value.address;}allocated=bytes;return arena_.allocate(bytes);
}
bool NativeSuckCannon::offer(unsigned actor){
 const int current=api_->loaded_level();if(current<0||current>18||!arena_.begin())return false;
 if(level_!=current){reset();level_=current;}
 if(const auto found=actors_.find(actor);found!=actors_.end()){
  if(same_actor(found->second))return valid_actor(found->second);
  if(!found->second.retired)return false;
  // A recycled managed actor can keep its previous class pointer when its
  // class ID is unchanged. Restore the shared class through the new owner's
  // compare/replace port before recycling storage still referenced by it.
  if(storage_referenced(found->second)&&(read<unsigned short>(actor+0xa6)!=found->second.native_class||!world_.cannon_replace_class(actor,found->second.clone,found->second.original_class)))return false;
  if(arena_.owns(found->second.storage,found->second.bytes))free_.push_back({found->second.storage,found->second.bytes});actors_.erase(found);
 }
 const auto body=world_.cannon_describe(actor);if(!body||!valid(actor,256)||!valid(body->pclass,0x48)||!body->pvars||!std::isfinite(body->scale)||body->scale<=0)return false;
 const auto index=unsigned(current);
 if(!api_->function_at||api_->function_at(Record[index])!=api_->function("func_L00_002DCD40")||api_->function_at(Grab[index])!=api_->function("func_L00_002DCDA8")||api_->function_at(Store[index])!=api_->function("func_L00_002DCFD0")||api_->function_at(Fire[index])!=api_->function("func_L00_002DD0E0")||api_->function_at(Cancel[index])!=api_->function("func_L00_002DD2D0"))return false;
 const unsigned class_bytes=0x48+4*std::max(1u,unsigned(read<unsigned char>(body->pclass+12)));
 if(!valid(body->pclass,class_bytes))return false;const unsigned class_storage=(class_bytes+15u)&~15u;
 unsigned allocated{};const auto storage=allocate(0xa0+class_storage+0x30,allocated);if(!storage)return false;
 Actor value{};value.body=*body;value.original_class=body->pclass;value.storage=value.record=storage;value.bytes=allocated;value.clone=storage+0xa0;value.table=value.clone+class_storage;value.anims=value.table+0x20;value.original_flags=read<unsigned short>(actor+0x34);value.native_class=read<unsigned short>(actor+0xa6);
 std::memset(api_->memory+storage,0,allocated);std::memcpy(api_->memory+value.clone,api_->memory+body->pclass,class_bytes);
 write(value.clone+0x10,body->collision);write(value.clone+0x24,body->scale);write(value.clone+0x2c,value.table);
 write(value.table,Grab[index]);write(value.table+4,Store[index]);write(value.table+8,Fire[index]);write(value.table+12,Cancel[index]);
 // Same-unit native callers can inline the ordinary getter. Its class callback
 // is indirect and therefore reaches the typed getter detour in every build.
 write(value.table+16,Record[index]);
 // A NULL terminal handler makes the stock finalizer call DeleteMoby directly.
 // Its boot guest address is not a valid callback in every loaded overlay.
 write<unsigned>(value.table+20,0);
 const auto sequence=read<unsigned char>(actor+0x53);std::memset(api_->memory+value.anims,sequence,8);write(value.record+0x70,value.anims);
 write<float>(value.record+0x78,read<float>(actor+0x58));write<float>(value.record+0x7c,.1f);
 const auto radius=read<float>(actor+12)/1024.f;const float fitted=std::isfinite(radius)?std::clamp(radius,.06f,20.f):.2f;
 write(value.record+0x80,fitted);write(value.record+0x84,read<unsigned>(actor+0x98));write(value.record+0x88,read<unsigned>(actor+0x98));write(value.record+0x8c,fitted);
 write<short>(value.record+0x6a,-1);write<short>(value.record+0x9c,-1);write<short>(value.record+0x9e,-1);
 try{
  const auto inserted=actors_.emplace(actor,value);if(!inserted.second)return false;
  if(!world_.cannon_replace_class(actor,value.original_class,value.clone)){actors_.erase(inserted.first);free_.push_back({storage,allocated});return false;}
  inserted.first->second.body.pclass=value.clone;write<unsigned short>(actor+0x34,read<unsigned short>(actor+0x34)|0x1000u);return true;
 }catch(...){const auto found=actors_.find(actor);if(found!=actors_.end()&&read<unsigned>(actor+0x24)==value.original_class)actors_.erase(found);return false;}
}
bool NativeSuckCannon::owns(unsigned actor)const{const auto found=actors_.find(actor);return found!=actors_.end()&&same_actor(found->second);}
bool NativeSuckCannon::handles_record(unsigned actor)const{
 if(level_!=api_->loaded_level()||!valid(actor,256))return false;const auto pc=read<unsigned>(actor+0x24);
 for(const auto& [key,a]:actors_){(void)key;if(pc==a.clone&&arena_.owns(a.storage,a.bytes))return true;}return false;
}
unsigned NativeSuckCannon::record(unsigned actor)const{const auto found=actors_.find(actor);return found!=actors_.end()&&valid_actor(found->second)?found->second.record:0;}
int NativeSuckCannon::grab(unsigned actor,unsigned muzzle,unsigned cannon){return candidate(cannon,0,actor,record(actor),muzzle);}
int NativeSuckCannon::slot(unsigned actor)const{const auto hero=g(Hero);for(unsigned i=0;i<10;++i)if(read<unsigned>(hero+Slots+i*4)==actor)return int(i);return -1;}
int NativeSuckCannon::candidate(unsigned cannon,unsigned,unsigned actor,unsigned record_address,unsigned muzzle){
 auto found=actors_.find(actor);if(found==actors_.end()||!valid_actor(found->second)||found->second.record!=record_address)return 0;
 auto& a=found->second;if(a.captured)return 0;
 const auto position=argument<std::array<float,4>>(api_,muzzle);if(!position||!finite({(*position)[0],(*position)[1],(*position)[2]})||!valid(cannon,256))return 0;
 const auto hero=g(Hero);const auto state=read<short>(a.record+0x68);
 if(a.claimed&&state==3){if(slot(actor)<0){rollback(a,false);return 0;}write(a.record,*position);write<float>(a.record+12,1);write(a.record+0x60,cannon);return 2;}
 if(a.claimed||state!=0||read<int>(hero+Stored)<0||read<int>(hero+Incoming)<0||read<unsigned>(hero+Stored)+read<unsigned>(hero+Incoming)>=capacity())return 0;
 int vacant=-1;for(unsigned i=0;i<capacity();++i)if(!read<unsigned>(hero+Slots+i*4)){vacant=int(i);break;}if(vacant<0)return 0;
 const auto claimed=world_.cannon_claim(actor);if(!claimed)return 0;
 if(claimed->actor!=actor||claimed->pclass!=a.clone||claimed->pvars!=a.body.pvars||!claimed->lease.generation){world_.cannon_cancel(*claimed,valid_actor(a));return 0;}
 a.body=*claimed;a.claimed=true;write(a.record,*position);write<float>(a.record+12,1);write(a.record+0x60,cannon);
 write<float>(a.record+0x64,std::max(.001f,distance_xy(read<Vec3>(actor+0x10),{(*position)[0],(*position)[1],(*position)[2]})));
 write<short>(a.record+0x68,3);write<short>(a.record+0x6a,short(vacant));write<float>(a.record+0x6c,1);write<float>(a.record+0x98,0);
 write<unsigned>(hero+Slots+unsigned(vacant)*4,actor);write<int>(hero+Incoming,read<int>(hero+Incoming)+1);return 2;
}
int NativeSuckCannon::store(unsigned actor,unsigned muzzle){
 auto found=actors_.find(actor);if(found==actors_.end()||!valid_actor(found->second))return 0;auto& a=found->second;
 if(a.captured)return read<short>(a.record+0x68)==4?1:0;
 const auto position=argument<std::array<float,4>>(api_,muzzle);const auto hero=g(Hero);const auto index=read<short>(a.record+0x6a);
 if(!a.claimed||!position||index<0||unsigned(index)>=capacity()||slot(actor)!=index||read<int>(hero+Stored)<=0||read<short>(a.record+0x68)!=3){a.abort_pending=a.claimed;return 0;}
 if(!world_.cannon_capture(a.body)){a.abort_pending=true;return 0;}
 a.captured=true;a.abort_pending=false;write(actor+0x10,*position);write<short>(a.record+0x68,4);write<float>(actor+0x2c,0);write<unsigned>(actor+0x94,0);write<unsigned char>(actor+0x31,0);write<unsigned short>(actor+0x34,(read<unsigned short>(actor+0x34)|3u)&~0x1000u);return 1;
}
void NativeSuckCannon::clear_slot(Actor& a,bool stored){
 const auto hero=g(Hero);const int index=slot(a.body.actor);if(index>=0){write<unsigned>(hero+Slots+unsigned(index)*4,0);const auto field=hero+(stored?Stored:Incoming);if(read<int>(field)>0)write<int>(field,read<int>(field)-1);}
 write<short>(a.record+0x6a,-1);
}
void NativeSuckCannon::restore(Actor& a){
 if(!valid_actor(a))return;write(a.body.actor+0x2c,a.body.scale);write<unsigned char>(a.body.actor+0x31,1);write(a.body.actor+0x94,a.body.collision);write(a.body.actor+0x98,read<unsigned>(a.record+0x84));
 write<unsigned short>(a.body.actor+0x34,(read<unsigned short>(a.body.actor+0x34)&~3u)|0x1000u);matrix_(a.body.actor);
}
bool NativeSuckCannon::rollback(Actor& a,bool stored){
 const bool valid=valid_actor(a);if(valid){clear_slot(a,stored);restore(a);write<short>(a.record+0x68,0);write<float>(a.record+0x0c,0);}
 if(!world_.cannon_cancel(a.body,valid))return false;a.claimed=false;a.abort_pending=false;a.body.lease={};return true;
}
int NativeSuckCannon::cancel(unsigned actor){
 auto found=actors_.find(actor);if(found==actors_.end()||!valid_actor(found->second))return 0;auto& a=found->second;
 if(a.captured)return read<short>(a.record+0x68)==4?1:0;
 if(!a.claimed)return 0;rollback(a,false);return 0;
}
int NativeSuckCannon::launch(unsigned actor,unsigned velocity,unsigned target,float height,Launch original){
 auto found=actors_.find(actor);if(found==actors_.end()||!valid_actor(found->second)||!found->second.captured||read<short>(found->second.record+0x68)!=4||!original)return 0;
 const auto vector=argument<std::array<float,4>>(api_,velocity);if(!vector||!finite({(*vector)[0],(*vector)[1],(*vector)[2]})||!std::isfinite(height))return 0;
 auto& a=found->second;const auto sequence=read<unsigned char>(actor+0x53);std::memset(api_->memory+a.anims,sequence,8);
 return original(actor,velocity,target,height);
}
void NativeSuckCannon::finalize(unsigned actor,unsigned record_address,int explode,Finalize original){
 auto found=actors_.find(actor);if(found==actors_.end()||!valid_actor(found->second)||found->second.record!=record_address||!found->second.captured||!original)return;
 const auto body=found->second.body;original(actor,record_address,explode);world_.cannon_retire(body);
 // DeleteMoby defers native cleanup by two ticks (state FD/FE). Its private
 // class must remain readable until the actor generation actually changes.
 found=actors_.find(actor);if(found!=actors_.end())found->second.retired=true;
}
bool NativeSuckCannon::update(unsigned actor){
 auto found=actors_.find(actor);if(found==actors_.end()||!valid_actor(found->second))return false;const auto state=read<short>(found->second.record+0x68);
 if(found->second.claimed&&!found->second.captured&&(found->second.abort_pending||state==0)){rollback(found->second,found->second.abort_pending);return true;}
 if(!found->second.claimed&&!found->second.captured)return false;if(state<3||state>6)return false;
 if(state==3&&(slot(actor)!=read<short>(found->second.record+0x6a)||read<int>(g(Hero)+Incoming)<=0)){rollback(found->second,false);return true;}
 const auto record_address=found->second.record;
 // Native MobyAnimAdvance runs before this relay and may have changed the
 // previous sequence since admission. Do not retain a stale animation index.
 std::memset(api_->memory+found->second.anims,read<unsigned char>(actor+0x53),8);
 struct Dispatch{unsigned& depth;explicit Dispatch(unsigned& n):depth(n){++depth;}~Dispatch(){--depth;}} dispatch(dispatch_depth_);
 carried_(actor,record_address);
 found=actors_.find(actor);if(found!=actors_.end()&&found->second.abort_pending)rollback(found->second,true);return true;
}
void NativeSuckCannon::frame(){
 if(level_!=api_->loaded_level()){reset();return;}
 for(auto it=actors_.begin();it!=actors_.end();){auto& a=it->second;if(valid_actor(a)){if(a.claimed&&!a.captured&&(a.abort_pending||read<short>(a.record+0x68)==0))rollback(a,a.abort_pending);++it;continue;}
  if(!a.retired){if(a.captured)world_.cannon_retire(a.body);else if(a.claimed&&!world_.cannon_cancel(a.body,false)){++it;continue;}a.retired=true;}
  if(storage_referenced(a)){++it;continue;}
  if(arena_.owns(a.storage,a.bytes))free_.push_back({a.storage,a.bytes});it=actors_.erase(it);
 }
}
void NativeSuckCannon::reset(){
 if(dispatch_depth_)return;
 if(level_transitioning()){
  // The loader owns all previous actor/class/record storage now. Resolve only
  // domain custody: restoring a class, slot or getter would corrupt its input.
  for(auto& [actor,a]:actors_){(void)actor;
   if(a.captured)world_.cannon_retire(a.body);
   else if(a.claimed)world_.cannon_cancel(a.body,false);
  }
  actors_.clear();free_.clear();level_=-1;return;
 }
 for(auto& [actor,a]:actors_){
  // A native dead/recycled actor can still reference this class when the mod
  // leaves. Disable its self getter before dropping the hook ownership.
  if(level_==api_->loaded_level()&&arena_.owns(a.storage,a.bytes)&&valid(a.table+16,4))write<unsigned>(a.table+16,0);
  if(a.retired)continue;const bool valid=valid_actor(a);
  if(a.captured){if(valid){clear_slot(a,read<short>(a.record+0x68)==4);remove_(actor);}world_.cannon_retire(a.body);}
  else {if(a.claimed)rollback(a,false);if(valid){world_.cannon_replace_class(actor,a.clone,a.original_class);write<unsigned short>(actor+0x34,(read<unsigned short>(actor+0x34)&~0x1000u)|(a.original_flags&0x1000u));}}
 }
 actors_.clear();free_.clear();level_=-1;
}
}
