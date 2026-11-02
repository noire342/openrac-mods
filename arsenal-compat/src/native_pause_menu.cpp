// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_pause_menu.hpp"
#include "native_text_state.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>

namespace arsenal::pause {
namespace {constexpr unsigned PageBytes=0x88,ScreenBytes=0xa4,Menu=0x1d5f70,Panels=0x1d6120;
bool same(const std::array<float,16>& a,const std::array<float,16>& b){return std::memcmp(a.data(),b.data(),sizeof a)==0;}}
NativePauseMenu::NativePauseMenu(const openrac_api* a,Allocate allocate,Compose compose):api_(a),allocate_(std::move(allocate)),compose_(std::move(compose)){}
bool NativePauseMenu::valid(unsigned p,std::size_t n){return n<=0x2000000&&p>=0x10000&&p<=0x2000000-n;}
unsigned NativePauseMenu::g(unsigned p)const{return api_->relocate_data(p);}
bool NativePauseMenu::italian()const{return read<int>(g(0x15ee88))==5;}
const NativePauseMenu::Item* NativePauseMenu::item(EntryId id)const{const auto i=std::find_if(items_.begin(),items_.end(),[&](const Item& x){return x.entry.id==id;});return i==items_.end()?nullptr:&*i;}
Status NativePauseMenu::set_entries(std::vector<Item> entries){
 if(entries.size()>MaxCustom)return Status::Exhausted;Model next=model_;
 for(const auto& old:items_)next.remove_entry(old.entry.id);
 for(const auto& x:entries){if(x.entry.id<0x8000000000000000ull||x.label_it.empty()||x.label_en.empty()||x.label_it.size()>95||x.label_en.size()>95||!x.activate)return Status::Invalid;const auto s=next.register_entry(x.entry);if(s!=Status::Ok)return s;}
 model_=std::move(next);items_=std::move(entries);return Status::Ok;
}
bool NativePauseMenu::discover(){
 auto matches=[&](unsigned p){if(!valid(p,PageBytes)||read<int>(p+0x3c)!=2||read<unsigned>(p+0x44+7*4))return false;std::array<unsigned,7> seen{};
  for(unsigned i=0;i<7;++i){const auto s=read<unsigned>(p+0x44+i*4),list=read<unsigned>(s+0x34);if(!valid(s,ScreenBytes)||!valid(list,12))return false;
   const auto label=read<unsigned short>(list);if(label!=(i==6?0x4fc2:0x4ee2+i)||read<unsigned short>(list+2)!=3)return false;
   if(!api_->function_at(read<unsigned>(s))||!api_->function_at(read<unsigned>(s+4))||std::find(seen.begin(),seen.begin()+i,s)!=seen.begin()+i)return false;seen[i]=s;}
  const auto first=seen[0],child=read<unsigned>(read<unsigned>(first+0x34)+4),focus=read<unsigned>(p+0x40);
  return valid(child,PageBytes)&&read<unsigned>(child+0x38)==p&&std::find(seen.begin(),seen.end(),focus)!=seen.end();};
 const auto system=g(Menu);unsigned best=0;
 for(const auto p:{read<unsigned>(system+4),read<unsigned>(system+8),g(0x1d2678)})if(matches(p)){best=p;break;}
 if(!best){unsigned distance=~0u;for(unsigned p=0x160000;p<0x400000;p+=4)if(matches(p)){const auto d=p>system?p-system:system-p;if(d<distance){best=p;distance=d;}}}
 if(!best)return false;root_=best;for(unsigned i=0;i<7;++i)stock_[i]=read<unsigned>(best+0x44+i*4);prototype_=read<unsigned>(read<unsigned>(stock_[0]+0x34)+4);return true;
}
bool NativePauseMenu::bind(unsigned callback){
 if(root_)return level_==api_->loaded_level()&&frame();if(!callback||!allocate_||!discover())return false;
 const auto bytes=PageBytes+9*(ScreenBytes+32),storage=allocate_(bytes);if(!valid(storage,bytes)){root_=prototype_=0;return false;}
 level_=api_->loaded_level();relay_=callback;directory_=storage;std::memcpy(api_->memory+directory_,api_->memory+root_,PageBytes);
 write(directory_+0x38,root_);write<unsigned>(directory_+0x80,0);for(unsigned i=0;i<14;++i)write<unsigned>(directory_+0x44+i*4,0);
 for(unsigned i=0;i<widgets_.size();++i){auto& w=widgets_[i];w.screen=storage+PageBytes+i*(ScreenBytes+32);w.items=w.screen+ScreenBytes;std::memcpy(api_->memory+w.screen,api_->memory+stock_[0],ScreenBytes);std::memset(api_->memory+w.items,0,32);write(w.screen+4,relay_);write(w.screen+0x34,w.items);write<int>(w.screen+0x40,0);write<unsigned short>(w.items,0x4ee2);}
 original_extension_animation_=read<unsigned>(root_+7*4);has_visible_custom_=std::any_of(items_.begin(),items_.end(),[](const Item& x){return x.entry.visible;});
 if(!lease(root_+0x44+7*4,widgets_[0].screen)||!lease(root_+7*4,read<unsigned>(root_+6*4))){reset();return false;}
 // Returning from this directory uses its animation table in reverse. Preserve
 // the new root row's native animation even when that row is unused here.
 write(directory_+7*4,read<unsigned>(root_+6*4));
 return frame();
}
bool NativePauseMenu::known_screen(unsigned screen)const{return std::find(stock_.begin(),stock_.end(),screen)!=stock_.end()||std::any_of(widgets_.begin(),widgets_.end(),[&](const Widget& x){return x.screen==screen;});}
bool NativePauseMenu::lease(unsigned at,unsigned value){
 auto p=std::find_if(patches_.begin(),patches_.end(),[&](const Patch& x){return x.at==at;});const auto live=read<unsigned>(at);
 if(p==patches_.end()){patches_.push_back({at,live,value});write(at,value);return true;}
 if(live&&live!=p->applied&&live!=p->original&&!known_screen(live))return false;write(at,value);p->applied=value;return true;
}
std::vector<EntryId> NativePauseMenu::custom_visible()const{std::vector<EntryId> out;for(const auto id:snapshot_.visible)if(item(id))out.push_back(id);return out;}
void NativePauseMenu::directory_rows(const std::vector<EntryId>& ids){
 const auto pages=std::max(std::size_t(1),(ids.size()+DirectoryPageSize-1)/DirectoryPageSize);directory_page_%=pages;
 const auto first=directory_page_*DirectoryPageSize,count=std::min(DirectoryPageSize,ids.size()-first);const bool footer=pages>1;std::vector<unsigned> shown;
 for(unsigned row=0;row<8;++row){const unsigned slot=row<6?row:row+1;auto& w=widgets_[row+1];w.id=row<count?ids[first+row]:0;w.footer=footer&&row==count;
  if(w.id||w.footer){write(directory_+0x44+slot*4,w.screen);write(directory_+slot*4,read<unsigned>(root_+std::min(row,6u)*4));shown.push_back(w.screen);}
  else write<unsigned>(directory_+0x44+slot*4,0);
 }
 if(shown.empty())return;
 for(unsigned i=0;i<shown.size();++i){write(shown[i]+0x38,shown[(i+shown.size()-1)%shown.size()]);write(shown[i]+0x3c,shown[(i+1)%shown.size()]);}
 const auto focus=read<unsigned>(directory_+0x40);if(std::find(shown.begin(),shown.end(),focus)==shown.end())write(directory_+0x40,shown[0]);
}
bool NativePauseMenu::frame(){
 after_capture();
 if(!root_||level_!=api_->loaded_level())return false;const auto system=g(Menu),current=read<unsigned>(system+4);const bool goodies=read<int>(system+0xd8)!=0;
 const auto menu_tick=read<unsigned>(system+0x110);if(last_menu_tick_!=~0u&&menu_tick<last_menu_tick_){activation_tick_=layout_tick_=~0u;root_pose_cache_={};root_render_candidates_={};extension_pose_.reset();directory_open_pending_=false;}last_menu_tick_=menu_tick;
 std::vector<Entry> stock;for(unsigned i=0;i<7;++i)stock.push_back({stock_id(i),std::int64_t(i)*100,i!=6||goodies});
 EntryId focus=0;const auto screen=current==directory_?read<unsigned>(directory_+0x40):read<unsigned>(root_+0x40);
 for(unsigned i=0;i<7;++i)if(screen==stock_[i])focus=stock_id(i);for(const auto& w:widgets_)if(w.screen==screen&&w.id)focus=w.id;
 if(compose_){Snapshot next;if(compose_(stock,focus,next)!=Status::Ok)return false;snapshot_=std::move(next);}
 else {if(model_.compose(stock)!=Status::Ok)return false;if(focus)model_.set_focus(focus);snapshot_=model_.snapshot();}
 const auto customs=custom_visible();widgets_[0].id=customs.size()==1?customs[0]:0;
 const bool visible=!customs.empty();if(visible&&!has_visible_custom_)extension_reappear_pending_=true;has_visible_custom_=visible;
 if(!lease(root_+0x44+7*4,visible?widgets_[0].screen:0)||!lease(root_+7*4,visible?read<unsigned>(root_+6*4):original_extension_animation_))return false;
 if(current==directory_&&!visible)write(system+8,root_);
 std::vector<unsigned> ring;for(const auto id:snapshot_.visible){if(id<=7&&id>0)ring.push_back(stock_[unsigned(id)-1]);}
 if(!customs.empty())ring.push_back(widgets_[0].screen);
 if(ring.empty())return false;
 // A native initializer may rewrite ANY known stock neighbour. An unknown
 // replacement belongs to another presenter; never partially overwrite it.
 // Before PauseAllSounds, native end-links and our copied widget links may be
 // null. They are uninitialized links, not foreign ownership. Every nonzero
 // foreign target still aborts the complete ring transaction.
 for(const auto s:ring)for(const auto at:{s+0x38,s+0x3c}){const auto target=read<unsigned>(at);if(target&&!known_screen(target))return false;}
 for(unsigned i=0;i<ring.size();++i){if(!lease(ring[i]+0x38,ring[(i+ring.size()-1)%ring.size()])||!lease(ring[i]+0x3c,ring[(i+1)%ring.size()]))return false;}
 if(current==root_&&known_screen(screen)&&std::find(ring.begin(),ring.end(),screen)==ring.end()){
  const auto selected=snapshot_.focus>0&&snapshot_.focus<=7?stock_[unsigned(snapshot_.focus)-1]:widgets_[0].screen;write(root_+0x40,std::find(ring.begin(),ring.end(),selected)!=ring.end()?selected:ring.front());
 }
 if(current==directory_&&focus){const auto i=std::find(customs.begin(),customs.end(),snapshot_.focus);if(i!=customs.end())directory_page_=std::size_t(i-customs.begin())/DirectoryPageSize;}
 directory_rows(customs);
 if(current==directory_&&focus)for(const auto& w:widgets_)if(w.id==snapshot_.focus)write(directory_+0x40,w.screen);
 // Physical geometry is borrowed only around the actual native renderer. A
 // frame-time edit would survive into the pose cache and compound its fit.
 restore_layout();return true;
}
void NativePauseMenu::restore_layout(){for(auto& q:quads_){if(q.active&&valid(q.vars,64)&&read<unsigned>(q.actor+0x78)==q.vars&&same(read<std::array<float,16>>(q.vars),q.applied))write(q.vars,q.original);q={};}}
void NativePauseMenu::layout(const std::vector<unsigned>& slots){
 if(slots.empty()||slots.size()>8)return;const auto tick=read<unsigned>(g(Menu)+0x110);if(layout_tick_==tick)return;layout_tick_=tick;
 for(const auto slot:slots){const auto actor=read<unsigned>(g(Panels)+slot*4),vars=read<unsigned>(actor+0x78);if(!valid(actor,256)||!valid(vars,64))return;auto raw=read<std::array<float,16>>(vars);auto& q=quads_[slot];if(q.active&&q.actor==actor&&q.vars==vars&&same(raw,q.applied))raw=q.original;q.actor=actor;q.vars=vars;q.original=raw;}
 auto center=[](const auto& a){std::array<float,3> c{};for(unsigned j=0;j<4;++j)for(unsigned k=0;k<3;++k)c[k]+=a[j*4+k]*.25f;return c;};
 const auto origin=center(quads_[slots[0]].original),second=center(quads_[slots[std::min(std::size_t(1),slots.size()-1)]].original);std::array<float,3> step{};float length=0;
 for(unsigned k=0;k<3;++k){step[k]=second[k]-origin[k];length+=step[k]*step[k];}length=std::sqrt(length);if(!std::isfinite(length)||length<.0001f)return;
 const float ratio=slots.size()==8?7.f/8.f:1.f;
 for(unsigned row=0;row<slots.size();++row){auto& q=quads_[slots[row]];const auto c=center(q.original);q.applied=q.original;
  for(unsigned j=0;j<4;++j){float dot=0;for(unsigned k=0;k<3;++k)dot+=(q.original[j*4+k]-c[k])*step[k]/length;
   for(unsigned k=0;k<3;++k)q.applied[j*4+k]=origin[k]+step[k]*(float(row)*ratio)+q.original[j*4+k]-c[k]+step[k]/length*dot*(ratio-1);}
  write(q.vars,q.applied);q.active=true;
 }
}
bool NativePauseMenu::directory_open()const{return directory_&&level_==api_->loaded_level()&&read<unsigned>(g(Menu)+4)==directory_;}
NativePauseMenu::RenderLease NativePauseMenu::pose(unsigned actor)const{
 RenderLease p;p.actor=actor;p.vars=read<unsigned>(actor+0x78);p.klass=read<unsigned>(actor+0x24);p.position=read<std::array<float,4>>(actor+0x10);p.sphere=read<std::array<float,4>>(actor);p.basis=read<std::array<float,12>>(actor+0xc0);p.corners=read<std::array<float,16>>(p.vars);p.animation=read<std::array<unsigned char,36>>(actor+0x50);p.variables=read<std::array<unsigned char,96>>(p.vars);p.scale=read<float>(actor+0x2c);return p;
}
bool NativePauseMenu::same_instance(const RenderLease& p)const{
 return level_==api_->loaded_level()&&valid(p.actor,256)&&valid(p.vars,96)&&valid(p.klass,0x48)&&read<unsigned>(p.actor+0x24)==p.klass&&read<unsigned>(p.actor+0x78)==p.vars&&read<unsigned short>(p.actor+0xa6)==0x472;
}
bool NativePauseMenu::root_cache_ready()const{
 for(const auto slot:{0u,1u,2u,3u,4u,5u,7u})if(!root_pose_cache_[slot]||!same_instance(*root_pose_cache_[slot]))return false;return true;
}
void NativePauseMenu::apply_portable_pose(const RenderLease& p,unsigned actor,unsigned vars){
 // Per-instance manipulator pointer+64 must remain owned by the destination.
 std::memcpy(api_->memory+actor+0x50,p.animation.data(),20);std::memcpy(api_->memory+actor+0x68,p.animation.data()+24,12);
 write(actor+0x10,p.position);write(actor,p.sphere);write(actor+0xc0,p.basis);write(actor+0x2c,p.scale);write(vars,p.variables);
}
void NativePauseMenu::before_render(){
 after_render();after_capture();root_render_candidates_={};if(!root_||level_!=api_->loaded_level())return;const auto system=g(Menu);auto current=read<unsigned>(system+4);bool transition=false;
 if(current!=root_&&current!=directory_){
  if(current||read<int>(system)!=1)return;const auto previous=read<unsigned>(system+0xd0),next=read<unsigned>(system+8);
  if(!((previous==root_&&next==directory_)||(previous==directory_&&next==root_)))return;
  // Native forward changes animate the destination; native back changes play
  // the old page's animations backwards, with current==0 for twelve ticks.
  current=next==read<unsigned>(previous+0x38)?previous:next;transition=true;
 }
 std::vector<unsigned> slots;if(current==root_){const bool goodies=read<int>(system+0xd8)!=0;for(unsigned i=0;i<7;++i)if(i!=6||goodies)slots.push_back(i);if(!custom_visible().empty())slots.push_back(7);}else for(unsigned row=0;row<8;++row){const unsigned slot=row<6?row:row+1;if(read<unsigned>(directory_+0x44+slot*4))slots.push_back(slot);}
 if(slots.empty()||slots.size()>8)return;
 if(current==directory_&&!transition&&!root_cache_ready()){write(system+8,root_);return;}
 const auto enabled=api_->relocate_level_data(0,0x1b2740);if(valid(enabled,14*4))for(unsigned slot=0;slot<14;++slot){const int original=read<int>(enabled+slot*4);const bool active=std::find(slots.begin(),slots.end(),slot)!=slots.end();const int applied=active?(current==directory_?1:original):0;render_flags_.push_back({enabled+slot*4,original,applied});write(enabled+slot*4,applied);}
 // panel_enabled controls native packets, while the frontend separately reads
 // live preview mobys. Hide unused preview actors for that same capture only.
 for(unsigned slot=0;slot<14;++slot)if(std::find(slots.begin(),slots.end(),slot)==slots.end()){const auto p=pose(read<unsigned>(g(Panels)+slot*4));if(!same_instance(p))continue;const auto flags=read<unsigned short>(p.actor+0x34);const auto hidden=static_cast<unsigned short>(flags|1u);visibility_leases_.push_back({p,flags,hidden});write(p.actor+0x34,hidden);}
 auto centre=[](const RenderLease& p){std::array<float,3> c{};for(unsigned i=0;i<4;++i)for(unsigned k=0;k<3;++k)c[k]+=p.corners[i*4+k]*.25f;return c;};
 for(unsigned row=0;row<slots.size();++row){const auto slot=slots[row],actor=read<unsigned>(g(Panels)+slot*4);auto original=pose(actor);if(!same_instance(original)){after_render();return;}render_leases_.push_back(original);
  if(current==directory_&&!transition){const auto source_slot=slot==8?7:slot;const auto& source=*root_pose_cache_[source_slot];if(source.klass!=original.klass||(slot!=8&&(source.actor!=actor||source.vars!=original.vars))){after_render();write(system+8,root_);return;}apply_portable_pose(source,actor,original.vars);}
  else if(current==root_&&!transition&&slot==7&&extension_reappear_pending_){
   const RenderLease* source=nullptr;if(root_pose_cache_[7]&&same_instance(*root_pose_cache_[7]))source=&*root_pose_cache_[7];else for(unsigned i=0;i<7;++i)if(root_pose_cache_[i]&&same_instance(*root_pose_cache_[i]))source=&*root_pose_cache_[i];
   if(source&&source->klass==original.klass&&root_pose_cache_[0]&&root_pose_cache_[1]){auto borrowed=*source;borrowed.actor=actor;borrowed.vars=original.vars;borrowed.klass=original.klass;const auto origin=centre(*root_pose_cache_[0]),next=centre(*root_pose_cache_[1]),base=centre(borrowed);for(unsigned k=0;k<3;++k){const float shift=origin[k]+(next[k]-origin[k])*6.f-base[k];borrowed.position[k]+=shift;borrowed.sphere[k]+=shift*1024.f;for(unsigned i=0;i<4;++i)borrowed.corners[i*4+k]+=shift;}std::memcpy(borrowed.variables.data(),borrowed.corners.data(),64);extension_pose_=borrowed;extension_reappear_pending_=false;}
  }
  if(current==root_&&!transition&&slot==7&&extension_pose_){if(same_instance(*extension_pose_))apply_portable_pose(*extension_pose_,actor,original.vars);else extension_pose_.reset();}
  // With Goodies present both final rows use the same stock rig sequence. The
  // extension's copied initializer can leave different previous-frame data;
  // borrow the actually opened final stock rig, keeping our own manipulators.
  if(current==root_&&!transition&&slots.size()==8&&slot==7&&root_pose_cache_[6]&&same_instance(*root_pose_cache_[6])&&root_pose_cache_[6]->klass==original.klass)apply_portable_pose(*root_pose_cache_[6],actor,original.vars);
  if(current==root_&&!transition)root_render_candidates_[slot]=pose(actor);
 }
 if(slots.size()<2)return;
 // Fit along the proven vertical Root axis during a transition. The moving
 // native panels can scatter horizontally; their live animation stays intact.
 if(transition&&(!root_pose_cache_[0]||!root_pose_cache_[1]||!same_instance(*root_pose_cache_[0])||!same_instance(*root_pose_cache_[1])))return;
 const auto first=transition?*root_pose_cache_[0]:pose(render_leases_[0].actor),second=transition?*root_pose_cache_[1]:pose(render_leases_[1].actor);const auto origin=centre(first),next=centre(second);std::array<float,3> step{};float magnitude=0;for(unsigned k=0;k<3;++k){step[k]=next[k]-origin[k];magnitude+=step[k]*step[k];}magnitude=std::sqrt(magnitude);if(!std::isfinite(magnitude)||magnitude<.0001f)return;
 const float ratio=slots.size()==8?7.f/8.f:1.f;
 for(unsigned row=0;row<render_leases_.size();++row){if(current==root_&&ratio==1.f)continue;auto p=pose(render_leases_[row].actor);const auto c=centre(p);std::array<float,3> translation{};for(unsigned k=0;k<3;++k)translation[k]=transition?step[k]*(float(row)-float(std::min(slots[row],6u))):origin[k]+step[k]*float(row)-c[k];
  auto point=[&](auto& q){float dot=0;for(unsigned k=0;k<3;++k)dot+=(q[k]-origin[k])*step[k]/magnitude;for(unsigned k=0;k<3;++k)q[k]+=step[k]/magnitude*dot*(ratio-1)+translation[k]*ratio;};point(p.position);for(unsigned k=0;k<3;++k)p.sphere[k]/=1024.f;point(p.sphere);for(unsigned k=0;k<3;++k)p.sphere[k]*=1024.f;
  for(unsigned column=0;column<3;++column){float dot=0;for(unsigned k=0;k<3;++k)dot+=p.basis[column*4+k]*step[k]/magnitude;for(unsigned k=0;k<3;++k)p.basis[column*4+k]+=step[k]/magnitude*dot*(ratio-1);}
  for(unsigned i=0;i<4;++i){std::array<float,3> q{{p.corners[i*4],p.corners[i*4+1],p.corners[i*4+2]}};point(q);for(unsigned k=0;k<3;++k)p.corners[i*4+k]=q[k];}std::memcpy(p.variables.data(),p.corners.data(),64);apply_portable_pose(p,p.actor,p.vars);
 }
}
void NativePauseMenu::after_render(){
 const auto system=g(Menu);if(root_&&read<unsigned>(system+4)==root_&&read<unsigned>(system+0x110)>=12){for(unsigned slot=0;slot<14;++slot)if(root_render_candidates_[slot]&&same_instance(*root_render_candidates_[slot])){const auto screen=read<unsigned>(root_+0x44+slot*4);const auto width=read<int>(screen+0x20),height=read<int>(screen+0x24);if(screen&&width>=16&&width<=2048&&height>=10&&height<=2048)root_pose_cache_[slot]=root_render_candidates_[slot];}}
 // The frontend queues only actor addresses here, then reads transforms at
 // vsync. Keep the borrowed pose alive until its capture has actually finished.
 for(const auto& original:render_leases_)if(same_instance(original))deferred_leases_.push_back({original,pose(original.actor)});render_leases_.clear();root_render_candidates_={};
 deferred_visibility_.insert(deferred_visibility_.end(),visibility_leases_.begin(),visibility_leases_.end());visibility_leases_.clear();
 for(auto p=render_flags_.rbegin();p!=render_flags_.rend();++p)if(read<int>(p->address)==p->applied)write(p->address,p->original);render_flags_.clear();
 if(directory_open_pending_&&root_cache_ready()&&read<unsigned>(system+4)==root_){directory_open_pending_=false;write(system+8,directory_);}
}
void NativePauseMenu::after_capture(){
 for(auto p=deferred_leases_.rbegin();p!=deferred_leases_.rend();++p)if(same_instance(p->original)){
  const auto& old=p->original;const auto& applied=p->applied;
  auto restore=[&](unsigned address,const auto& before,const auto& owned){using T=std::decay_t<decltype(owned)>;const auto live=read<T>(address);if(std::memcmp(&live,&owned,sizeof(T))==0)write(address,before);};
  restore(old.actor+0x10,old.position,applied.position);restore(old.actor,old.sphere,applied.sphere);restore(old.actor+0xc0,old.basis,applied.basis);restore(old.actor+0x50,old.animation,applied.animation);restore(old.actor+0x2c,old.scale,applied.scale);restore(old.vars,old.variables,applied.variables);
 }
 deferred_leases_.clear();
 for(auto p=deferred_visibility_.rbegin();p!=deferred_visibility_.rend();++p)if(same_instance(p->identity)&&read<unsigned short>(p->identity.actor+0x34)==p->applied)write(p->identity.actor+0x34,p->original);deferred_visibility_.clear();
}
bool NativePauseMenu::owns_callback(unsigned argument)const{return level_==api_->loaded_level()&&argument&&std::any_of(widgets_.begin(),widgets_.end(),[&](const Widget& w){return w.screen==argument;});}
void NativePauseMenu::text(unsigned screen,const std::string& label){
 const int width=read<int>(screen+0x20),height=read<int>(screen+0x24);if(width<16||height<10||width>2048||height>2048||label.size()>256)return;
 // Match the native list renderer's font bank, measurement and glyph routine.
 // Rendering a caption does not own page transitions or physical panel poses.
 const auto flags=read<unsigned>(screen+0x30);int font_height=12,texture_index=1;unsigned metrics=g(0x1df3d0);
 if(flags&4u){font_height=14;texture_index=3;metrics=g(0x1dfb10);}if(flags&8u){font_height=10;texture_index=2;metrics=g(0x1df770);}
 using Begin=void(*)(int);using End=void(*)();using Measure=int(*)(unsigned,int,unsigned);using Texture=std::uint64_t(*)(int);using Font=void(*)(int,int,std::uint64_t,unsigned,int,std::uint64_t,unsigned);
 const auto begin=reinterpret_cast<Begin>(api_->function("func_001F4630"));
 const auto end=reinterpret_cast<End>(api_->function("func_001F4748"));const auto measure=reinterpret_cast<Measure>(api_->function("func_001F65B0"));const auto texture=reinterpret_cast<Texture>(api_->function("func_001F4868"));const auto font=reinterpret_cast<Font>(api_->function("func_001F6668"));
 if(!begin||!end||!measure||!texture||!font||!valid(metrics,232*4))return;
 openrac_gaddr saved=0;const auto storage=api_->stack_push(unsigned(label.size()+4),&saved);if(!storage)return;struct Pop{const openrac_api* a;unsigned saved;~Pop(){a->stack_pop(saved);}}pop{api_,saved};
 auto caption=label;auto measured=[&](const std::string& value){std::memcpy(api_->memory+storage,value.c_str(),value.size()+1);return measure(storage,-1,metrics);};int text_width=measured(caption);
 if(text_width>width-8){while(!caption.empty()){auto last=caption.size()-1;while(last&&(static_cast<unsigned char>(caption[last])&0xc0u)==0x80u)--last;caption.resize(last);text_width=measured(caption+"...");if(text_width<=width-8)break;}if(text_width>width-8)return;}
 const auto current=read<unsigned>(g(Menu)+4);const std::uint64_t color=read<unsigned>(current+0x40)==screen?0x8020ffffu:0x80ffa888u;
 const int row_height=(flags&0x10u)?font_height+3:height/2;const int x=width/2-text_width/2,y=row_height-font_height/2-1;
 if(arsenal::native::prepare_capture_text(api_)){begin(0);font(x,y,color,storage,-1,texture(texture_index),metrics);end();}
}unsigned NativePauseMenu::invoke_callback(unsigned screen){
 auto found=std::find_if(widgets_.begin(),widgets_.end(),[&](const Widget& w){return w.screen==screen;});if(found==widgets_.end())return 0;const auto customs=custom_visible();const bool root=screen==widgets_[0].screen;
 std::string label;if(root&&customs.size()!=1)label=italian()?"Altri menu":"Other menus";else if(found->footer){const auto pages=(customs.size()+DirectoryPageSize-1)/DirectoryPageSize;label=(italian()?"Pagina ":"Page ")+std::to_string(directory_page_+1)+"/"+std::to_string(pages)+"  >";}else if(const auto* value=item(found->id))label=italian()?value->label_it:value->label_en;
 text(screen,label);const auto system=g(Menu),current=read<unsigned>(system+4),tick=read<unsigned>(system+0x110);
 if((current==root_||current==directory_)&&read<unsigned>(current+0x40)==screen&&(read<unsigned>(g(0x13ca40)+0x1c4)&0x40u)&&activation_tick_!=tick){activation_tick_=tick;
  if(root&&customs.size()>1){directory_page_=0;directory_rows(customs);if(root_cache_ready())write(system+8,directory_);else directory_open_pending_=true;}
  else if(found->footer){++directory_page_;directory_rows(customs);}
  else if(const auto* value=item(found->id))try{value->activate(value->entry.id);}catch(...){}
 }
 return 2;
}
void NativePauseMenu::reset(bool restore){
 if(restore){after_render();after_capture();}
 if(restore&&level_==api_->loaded_level()){restore_layout();const auto system=g(Menu);if(directory_&&(read<unsigned>(system+4)==directory_||read<unsigned>(system+8)==directory_)){write(system+4,root_);write(system+8,root_);write<int>(system+0xc,1);}
  for(auto p=patches_.rbegin();p!=patches_.rend();++p)if(read<unsigned>(p->at)==p->applied)write(p->at,p->original);}
 root_=prototype_=directory_=relay_=0;level_=-1;stock_={};widgets_={};patches_.clear();quads_={};render_leases_.clear();deferred_leases_.clear();visibility_leases_.clear();deferred_visibility_.clear();render_flags_.clear();root_pose_cache_={};root_render_candidates_={};extension_pose_.reset();directory_open_pending_=false;activation_tick_=layout_tick_=last_menu_tick_=~0u;directory_page_=0;original_extension_animation_=0;has_visible_custom_=extension_reappear_pending_=false;
}
}
