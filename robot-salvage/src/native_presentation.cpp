// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_presentation.hpp"
#include "text_layout.hpp"
#include "../../arsenal-compat/src/native_text_state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>

namespace salvage {
namespace {
// OpenRAC PAL config/overlays/functions.tsv, func_L00_002422D8 places.
constexpr std::array<unsigned,19> Relays{{0x2422d8,0x256268,0x243dc8,0x230518,0x234c48,0x26b298,0x250f68,0x2698b0,0x24be40,0x2612c8,0x234cd0,0x2675f8,0x25a448,0x24d4d8,0x249e68,0x231868,0x23d918,0x23ca48,0x244550}};
constexpr unsigned PageBytes=0x88,ScreenBytes=0xa4;
std::string bounded(const std::string& value,std::size_t n=110){if(value.size()<=n)return value;return value.substr(0,n-3)+"...";}
constexpr std::uint64_t ComponentsId=0x92524f424f545301ull;
std::map<std::uint64_t,NativePresentation*> PauseContexts;
std::uint64_t NextPauseContext=1;
}
NativePresentation::NativePresentation(const openrac_api* api,openrac_mod* mod,GuestArena& arena,const Catalog& catalog,const Inventory& inventory)
 :NativeMemory(api),mod_(mod),arena_(arena),catalog_(catalog),inventory_(inventory),pause_menu_(api,[&arena](std::size_t bytes){return arena.allocate(bytes);}){}
NativePresentation::~NativePresentation(){reset();release_pause_service();}
std::int32_t NativePresentation::open_components(std::uint64_t context,std::uint64_t id){
 const auto found=PauseContexts.find(context);if(id!=ComponentsId||found==PauseContexts.end())return ARSENAL_PAUSE_NOT_FOUND;auto& ui=*found->second;
 if(!ui.page_||ui.level_!=ui.api_->loaded_level()||ui.level_transitioning()||ui.read<int>(ui.mode())!=3)return ARSENAL_PAUSE_NOT_READY;ui.open_from_menu();return ARSENAL_PAUSE_OK;
}
void NativePresentation::open_from_menu(){if(level_transitioning()||read<int>(mode())!=3)return;const auto system=g(MenuSystem),current=read<unsigned>(system+4);write(page_+0x38,current==pause_menu_.directory_address()?current:root_);write(system+8,page_);}
void NativePresentation::release_pause_service(){
 if(pause_service_&&pause_owner_){if(pause_presenter_)pause_service_->release_presenter(pause_owner_,pause_presenter_);if(pause_token_)pause_service_->unregister_entry(pause_owner_,pause_token_);pause_service_->release(pause_owner_);}
 if(pause_context_)PauseContexts.erase(pause_context_);pause_service_=nullptr;pause_owner_=pause_token_=pause_presenter_=pause_context_=0;pause_revision_=~std::uint64_t(0);pause_menu_.set_composer({});
}
void NativePresentation::set_pause_service(const arsenal_pause_api_v1* service){
 reset();release_pause_service();if(!service)return;
 if(service->size<sizeof(*service)||service->version!=1||!service->acquire||!service->release||!service->register_entry||!service->unregister_entry||!service->entry_count||!service->entry_at||!service->claim_presenter||!service->release_presenter||!service->compose||!service->set_focus||!service->snapshot||!service->activate)return;
 pause_service_=service;if(service->acquire("robot-salvage.components",&pause_owner_)!=ARSENAL_PAUSE_OK)return;
 if(!NextPauseContext)return;pause_context_=NextPauseContext++;PauseContexts.emplace(pause_context_,this);
 arsenal_pause_entry_v1 entry{};entry.size=sizeof entry;entry.id=ComponentsId;entry.order=1000;entry.flags=ARSENAL_PAUSE_VISIBLE;std::strcpy(entry.label_it,"Componenti");std::strcpy(entry.label_en,"Components");
 if(service->register_entry(pause_owner_,&entry,open_components,pause_context_,&pause_token_)!=ARSENAL_PAUSE_OK)return;
 arsenal_pause_capacity_v1 capacity{sizeof capacity,39,32,0};if(service->claim_presenter(pause_owner_,&capacity,&pause_presenter_)!=ARSENAL_PAUSE_OK)return;
 pause_menu_.set_composer([this](const std::vector<arsenal::pause::Entry>& stock,arsenal::pause::EntryId focus,arsenal::pause::Snapshot& output){
  std::vector<arsenal_pause_stock_v1> native;for(const auto& x:stock)native.push_back({sizeof(arsenal_pause_stock_v1),x.visible?ARSENAL_PAUSE_VISIBLE:0u,x.id,x.order});
  if(pause_service_->compose(pause_presenter_,native.data(),unsigned(native.size()))!=ARSENAL_PAUSE_OK)return arsenal::pause::Status::Invalid;
  if(focus)pause_service_->set_focus(pause_presenter_,focus);
  arsenal_pause_state_v1 state{};state.size=sizeof state;std::uint64_t ids[39]{};std::uint32_t count=0;
  if(pause_service_->snapshot(pause_presenter_,&state,ids,39,&count)!=ARSENAL_PAUSE_OK||count>39)return arsenal::pause::Status::Invalid;
  output.revision=state.revision;output.focus=state.focus;output.visible.assign(ids,ids+count);return arsenal::pause::Status::Ok;
 });
}
void NativePresentation::sync_pause_entries(){
 using Native=arsenal::pause::NativePauseMenu;
 if(!pause_service_){if(pause_revision_!=~std::uint64_t(0))return;Native::Item entry{{ComponentsId,1000,true},"Componenti","Components",[this](std::uint64_t){if(page_)open_from_menu();}};if(pause_menu_.set_entries({std::move(entry)})==arsenal::pause::Status::Ok)pause_revision_=0;return;}
 if(!pause_presenter_)return;std::uint32_t count=0;std::uint64_t revision=0;if(pause_service_->entry_count(&count,&revision)!=ARSENAL_PAUSE_OK||count>32||revision==pause_revision_)return;
 std::vector<Native::Item> entries;for(std::uint32_t i=0;i<count;++i){arsenal_pause_entry_copy_v1 copy{};copy.size=sizeof copy;if(pause_service_->entry_at(i,&copy)!=ARSENAL_PAUSE_OK)return;
  if(!std::memchr(copy.entry.label_it,0,sizeof copy.entry.label_it)||!std::memchr(copy.entry.label_en,0,sizeof copy.entry.label_en))return;
  entries.push_back({{copy.entry.id,copy.entry.order,(copy.entry.flags&ARSENAL_PAUSE_VISIBLE)!=0},copy.entry.label_it,copy.entry.label_en,[this](std::uint64_t id){pause_service_->activate(pause_presenter_,id);}});
 }
 if(pause_menu_.set_entries(std::move(entries))==arsenal::pause::Status::Ok)pause_revision_=revision;
}
bool NativePresentation::italian()const{return read<int>(g(Language))==5;}
void NativePresentation::reset(){
 const bool restore=!level_transitioning();if(restore&&level_==api_->loaded_level()){
  remove_world_prompt();

  const auto system=g(MenuSystem);
  if(page_&&(read<unsigned>(system+4)==page_||read<unsigned>(system+8)==page_)){write(system+4,root_);write(system+8,root_);write<int>(system+0xc,1);}

 }
 pause_menu_.reset(restore);
 screens_.clear();level_=-1;root_=page_=entry_=relay_=message_=world_prompt_=0;candidate_=0;candidate_position_.reset();selected_=0;tick_=notified_tick_=confirmation_tick_=~0u;triangle_down_=triangle_claimed_=false;last_message_.clear();
 pause_render_calls_=pause_render_logged_=world_prompt_calls_=world_prompt_logged_=world_prompt_rejects_=0;world_registered_tick_=~0u;
}
void NativePresentation::before_pause_render(){
 if(level_transitioning())return;
 ++pause_render_calls_;pause_menu_.before_render();const auto current=read<unsigned>(g(MenuSystem)+4);const unsigned flag=current==pause_menu_.root_address()?1u:current==pause_menu_.directory_address()?2u:0u;
 if(flag&&!(pause_render_logged_&flag)&&api_->log&&pause_menu_.borrowed_render_rows()){char trace[192];std::snprintf(trace,sizeof trace,"salvage native pause render callback=%u page=%08x rows=%llu",pause_render_calls_,current,(unsigned long long)pause_menu_.borrowed_render_rows());api_->log(mod_,OPENRAC_LOG_INFO,trace);pause_render_logged_|=flag;}
}
void NativePresentation::after_pause_render(){if(!level_transitioning())pause_menu_.after_render();}
void NativePresentation::draw_hud_prompt(){if(native_hud_boundary_&&!level_transitioning()&&read<int>(mode())==0)draw_world_prompt();}
bool NativePresentation::bind(){
 if(level_transitioning())return false;
 const int current=api_->loaded_level();if(current<0||current>18||!arena_.begin())return false;
 if(pause_service_&&!pause_presenter_)return false;
 const auto callback=Relays[unsigned(current)];if(api_->function_at(callback)!=api_->function("func_L00_002422D8"))return false;
 sync_pause_entries();if(!pause_menu_.bind(callback))return false;
 const auto root=pause_menu_.root_address(),prototype=pause_menu_.weapon_page_address();if(!valid(prototype,PageBytes))return false;
 const auto storage=arena_.allocate(PageBytes+14*ScreenBytes+512+32);if(!storage)return false;
 root_=root;page_=storage;entry_=pause_menu_.entry_address();relay_=callback;message_=storage+PageBytes+14*ScreenBytes;world_prompt_=message_+512;level_=current;
 std::memcpy(api_->memory+page_,api_->memory+prototype,PageBytes);write(page_+0x38,root_);write<unsigned>(page_+0x80,0);
 const auto original_focus=read<unsigned>(prototype+0x40);unsigned focus=0;
 for(unsigned i=0;i<14;++i){
  const auto source=read<unsigned>(prototype+0x44+i*4);if(!source){write<unsigned>(page_+0x44+i*4,0);continue;}
  if(!valid(source,ScreenBytes)){reset();return false;}
  const auto screen=page_+PageBytes+i*ScreenBytes;std::memcpy(api_->memory+screen,api_->memory+source,ScreenBytes);write<unsigned>(screen,0);write(screen+4,relay_);write<unsigned>(screen+8,0);write<unsigned>(screen+12,0);
  Role role=Role::Blank;if(source==original_focus){role=Role::List;focus=screen;}else switch(i){case 0:role=Role::Heading;break;case 1:role=Role::Details;break;case 3:role=Role::Thumbnail;break;case 4:role=Role::Name;break;case 5:role=Role::Quantity;break;case 7:role=Role::Total;break;case 8:role=Role::Controls;break;default:break;}
  screens_.push_back({screen,role});write(page_+0x44+i*4,screen);
 }
 if(!focus){reset();return false;}write(page_+0x40,focus);
 if(api_->log){char info[192];std::snprintf(info,sizeof info,"salvage native pause ready root=%08x components=%08x entry=%08x directory=%08x shared=%d",root_,page_,entry_,pause_menu_.directory_address(),int(pause_presenter_!=0));api_->log(mod_,OPENRAC_LOG_INFO,info);}return true;

}
bool NativePresentation::inventory_open()const{return !level_transitioning()&&page_&&level_==api_->loaded_level()&&read<int>(mode())==3&&read<unsigned>(g(MenuSystem)+4)==page_;}
bool NativePresentation::route_triangle(bool down){
 if(!down){triangle_down_=triangle_claimed_=false;return false;}
 const bool edge=!triangle_down_;triangle_down_=true;
 if(triangle_claimed_)return true;
 if(!edge||!inventory_open())return false;
 const auto system=g(MenuSystem);
 // Same request used by retail Pause_PageInput: the engine owns the twelve
 // frame leave/enter transition and the physical panel animations.
 if(read<unsigned>(system+8)==0||read<unsigned>(system+8)==page_){const auto back=read<unsigned>(page_+0x38);write(system+8,back==pause_menu_.directory_address()?back:root_);triangle_claimed_=true;return true;}
 return false;
}
bool NativePresentation::native_interaction_active()const{
 if(level_transitioning()||read<int>(mode())!=0||read<int>(g(Hero)+0x22a8)<=0||read<unsigned char>(g(Hero)+0x20a4)!=0)return true;
 // Retail's two-frame help selector is also the gate used by Hero's own
 // Triangle handling. Vendors, ships, teleporters and vehicle entry refresh it
 // only while their native eligibility checks pass (not a guessed radius).
 if(read<int>(l(0,0x15f674))!=0)return true;
 // Dialog NPCs and the Gold vendor use HUD channel12 directly. A guest callback
 // is resolved through the SDK; guest32 addresses are never host pointers.
 const auto channel=l(0,0x17dd50)+12u*0x90u;
 const auto draw=api_->function("func_L00_00237B90");
 if(valid(channel,0x90)&&draw&&api_->function_at){
  const bool visible=read<int>(channel+0x7c)>0||read<int>(channel+0x6c)>-6;
  const bool pending=read<int>(channel+0x68)!=0&&read<unsigned>(channel+0x20)!=0xffffu;
  if((visible&&api_->function_at(read<unsigned>(channel+0x18))==draw)||
     (pending&&api_->function_at(read<unsigned>(channel+0x38))==draw))return true;
 }
 const auto timer=l(0,0x15f720),native_text=l(0,0x179218);
 if(read<int>(timer)>0&&valid(native_text,256)){
  const auto* current=reinterpret_cast<const char*>(api_->memory+native_text);const auto* end=static_cast<const char*>(std::memchr(current,0,256));
  if(!end||std::string(current,end)!=last_message_)return true;
 }
 return false;
}
bool NativePresentation::owns_callback(unsigned argument)const{return pause_menu_.owns_callback(argument)||(level_==api_->loaded_level()&&((world_prompt_&&argument==world_prompt_)||std::any_of(screens_.begin(),screens_.end(),[&](const Screen& s){return s.address==argument;})));}
std::vector<std::pair<PartId,Quantity>> NativePresentation::rows()const{
 std::vector<std::pair<PartId,Quantity>> result(inventory_.snapshot().begin(),inventory_.snapshot().end());
 std::stable_sort(result.begin(),result.end(),[&](const auto& a,const auto& b){const auto* x=catalog_.find(a.first);const auto* y=catalog_.find(b.first);if(bool(x)!=bool(y))return bool(x);if(!x)return a.first<b.first;const auto& xn=italian()?x->name_it:x->name_en;const auto& yn=italian()?y->name_it:y->name_en;return xn==yn?a.first<b.first:xn<yn;});return result;
}
void NativePresentation::text(unsigned screen,const std::string& value,bool small,bool center,int top,int bottom,unsigned color,int inset){
 const int width=read<int>(screen+0x20),height=read<int>(screen+0x24);if(width<16||width>2048||height<10||height>2048||value.size()>1500)return;
 openrac_gaddr saved=0;const auto at=api_->stack_push(unsigned(value.size()+33),&saved);if(!at)return;
 struct Pop{const openrac_api* api;unsigned saved;~Pop(){api->stack_pop(saved);}} pop{api_,saved};
 const std::int16_t area[12]={std::int16_t(top),std::int16_t(bottom<0?height-4:bottom),4,std::int16_t(width-4),std::int16_t(center?width/2:inset),std::int16_t(top+2),0,0,std::int16_t(small?12:14),std::int16_t(center?1:0),0,0};
 std::memcpy(api_->memory+at,area,sizeof area);std::memcpy(api_->memory+at+32,value.c_str(),value.size()+1);
 using Begin=void(*)(int);using End=void(*)();using Font=void(*)(unsigned,unsigned,unsigned,int);
 const auto begin=reinterpret_cast<Begin>(api_->function("func_001F4630"));
 const auto end=reinterpret_cast<End>(api_->function("func_001F4748"));
 const auto font=reinterpret_cast<Font>(api_->function(small?"func_001F75D0":"func_001F7560"));if(begin&&end&&font&&arsenal::native::prepare_capture_text(api_)){begin(0);font(at,color,at+32,-1);end();}
}
void NativePresentation::draw_list(unsigned screen){
 const auto list=rows();if(list.empty()){text(screen,italian()?"Nessun componente raccolto.":"No components collected.",true,true,18);return;}
 selected_=std::min(selected_,list.size()-1);const auto system=g(MenuSystem);const auto tick=read<unsigned>(system+0x110),buttons=read<unsigned>(g(Pad)+0x1c4);
 if(tick_!=tick){tick_=tick;const auto previous=selected_;if(buttons&0x1000u)selected_=(selected_+list.size()-1)%list.size();else if(buttons&0x4000u)selected_=(selected_+1)%list.size();
  if(previous!=selected_){using Sound=int(*)(int,int,unsigned);if(auto sound=reinterpret_cast<Sound>(api_->function("func_0022ED80")))sound(1,0x11,read<unsigned>(screen+0x14));}}
 const int height=read<int>(screen+0x24),count=std::max(1,std::min(15,(height-8)/22));const std::size_t first=selected_>=std::size_t(count)?selected_-std::size_t(count)+1:0;
 for(int row=0;row<count&&first+std::size_t(row)<list.size();++row){const auto index=first+std::size_t(row);const auto* d=catalog_.find(list[index].first);
  const auto name=d?(italian()?d->name_it:d->name_en):(italian()?"Componente non disponibile":"Unavailable component");
  const auto caption=presentation::list_caption(name,list[index].second,std::max(0,read<int>(screen+0x20)-20));text(screen,caption.text,caption.font==presentation::Font::Small,false,4+row*22,26+row*22,index==selected_?0x8020ffffu:0x80ffa888u,12);
 }
}
unsigned NativePresentation::invoke_callback(unsigned screen){
 if(level_transitioning())return 0;
 if(pause_menu_.owns_callback(screen)){
  // This executes inside the real native draw, after DrawMobyList and native
  // projection, rather than observing restored fields after the RAII lease.
  if(pause_menu_.directory_open()&&pause_menu_.borrowed_render_rows()&&!(pause_render_logged_&4u)&&api_->log){const auto actor=read<unsigned>(g(0x1d6120)),vars=read<unsigned>(actor+0x78);char trace[700];std::snprintf(trace,sizeof trace,"salvage native directory inside-draw actor=%08x vars=%08x frame=%u prev=%u seq=%u blend=%.4f speed=%.4f current_data=%08x previous_data=%08x corner=[%.4f,%.4f,%.4f] rect=[%d,%d,%d,%d]",actor,vars,unsigned(read<unsigned char>(actor+0x50)),unsigned(read<unsigned char>(actor+0x51)),unsigned(read<unsigned char>(actor+0x52)),double(read<float>(actor+0x54)),double(read<float>(actor+0x58)),read<unsigned>(actor+0x68),read<unsigned>(actor+0x6c),double(read<float>(vars)),double(read<float>(vars+4)),double(read<float>(vars+8)),read<int>(screen+0x18),read<int>(screen+0x1c),read<int>(screen+0x20),read<int>(screen+0x24));api_->log(mod_,OPENRAC_LOG_INFO,trace);pause_render_logged_|=4u;}
  return pause_menu_.invoke_callback(screen);
 }
 if(world_prompt_&&screen==world_prompt_){draw_world_prompt();return 0;}
 const auto found=std::find_if(screens_.begin(),screens_.end(),[&](const Screen& s){return s.address==screen;});if(found==screens_.end())return 0;
 if(!inventory_open())return 2;if(found->role==Role::List){draw_list(screen);return 2;}
 const auto list=rows();if(!list.empty())selected_=std::min(selected_,list.size()-1);const auto* d=list.empty()?nullptr:catalog_.find(list[selected_].first);
 const auto name=d?(italian()?d->name_it:d->name_en):(italian()?"Nessun componente":"No component");
 switch(found->role){
 case Role::Heading:text(screen,italian()?"Componenti":"Components",false,true);break;
 case Role::Name:{const auto caption=presentation::name_caption(name,std::max(0,read<int>(screen+0x20)-12),read<int>(screen+0x24));text(screen,caption.text,caption.font==presentation::Font::Small,true,caption.font==presentation::Font::Small?4:10);break;}
 case Role::Quantity:if(!list.empty()){const auto caption=presentation::fit_line("x"+std::to_string(list[selected_].second),std::max(0,read<int>(screen+0x20)-12));text(screen,caption.text,caption.font==presentation::Font::Small,true,std::max(4,read<int>(screen+0x24)/3));}break;
 case Role::Details:if(d)text(screen,std::string(name)+"\1\1"+(italian()?"Origine: ":"Source: ")+(italian()?d->source_it:d->source_en)+"\1\1"+(italian()?"Quantita: ":"Quantity: ")+presentation::exact_quantity(list[selected_].second),true,false,12);else text(screen,italian()?"Recupera i pezzi dei robot con \x12.":"Recover robot parts with \x12.",true,false,16);break;
 case Role::Thumbnail:if(d&&thumbnail_)thumbnail_(screen,*d);else if(d)text(screen,italian()?"Anteprima non disponibile":"Preview unavailable",true,true,16);break;
 case Role::Total:{Quantity total=0;for(const auto& row:list)total=row.second>std::numeric_limits<Quantity>::max()-total?std::numeric_limits<Quantity>::max():total+row.second;text(screen,(italian()?"Raccolti: ":"Collected: ")+std::to_string(total),false,true);break;}
 case Role::Controls:text(screen,italian()?"Su/Giu: seleziona\1\x12 Indietro":"Up/Down: select\1\x12 Back",true,false,8);break;
 default:break;
 }return 2;
}
bool NativePresentation::message(const std::string& text_value,int frames){
 if(!message_||!arena_.owns(message_,512)||text_value.empty()||text_value.size()>250)return false;
 if(native_interaction_active())return false;
 const auto timer=l(0,0x15f720),native_text=l(0,0x179218);
 if(read<int>(timer)>0&&valid(native_text,256)){
  const auto* current=reinterpret_cast<const char*>(api_->memory+native_text);const auto* end=static_cast<const char*>(std::memchr(current,0,256));
  if(!end||std::string(current,end)!=last_message_)return false;
 }
 const auto bank=read<unsigned>(g(0x15f780));const int count=read<int>(g(0x1997fc));if(count<1||count>20000||!valid(bank,std::size_t(count)*16))return false;
 using Notify=void(*)(int,int);const auto notify=reinterpret_cast<Notify>(api_->function("func_L00_00264DB8"));if(!notify)return false;
 for(int i=0;i<count;++i){const auto entry=bank+unsigned(i)*16;if(read<int>(entry+4)!=0x4ee2)continue;const auto original=read<unsigned>(entry);
  std::memcpy(api_->memory+message_,text_value.c_str(),text_value.size()+1);write(entry,message_);
  struct Restore{NativePresentation* ui;unsigned at,previous,replacement;~Restore(){if(ui->read<unsigned>(at)==replacement)ui->write(at,previous);}} restore{this,entry,original,message_};
  notify(0x4ee2,frames);last_message_=text_value;return true;
 }return false;
}
void NativePresentation::notify_collected(PartId id,Quantity amount){
 candidate_=0;candidate_position_.reset();
 if(level_transitioning())return;
 const auto* d=catalog_.find(id);if(!d)return;if(message("+"+std::to_string(amount)+" "+bounded(italian()?d->name_it:d->name_en,100),90))confirmation_tick_=read<unsigned>(l(0,0x15f6b0));
 using Sound=int(*)(int,int,unsigned);if(auto sound=reinterpret_cast<Sound>(api_->function("func_0022ED80")))sound(1,0x11,read<unsigned>(g(Hero)+0x2080));
}
void NativePresentation::frame(){
 if(level_transitioning())return;
 const int current=api_->loaded_level();if(current!=level_)reset();if(current<0||current>18)return;
 if(!page_&&!bind())return;sync_pause_entries();pause_menu_.frame();
 // The copied weapon-grid gameplay handler is disabled. Preserve its generic
 // Start/Select close action without invoking weapon selection side effects.
 if(inventory_open()&&(read<unsigned>(g(Pad)+0x1c4)&0xd00u)&&read<int>(g(MenuSystem)+0x124)==0)write<int>(g(MenuSystem)+0xc,1);
 if(read<int>(mode())!=0||!candidate_)return;
 const auto* d=catalog_.find(candidate_);if(!d)return;
 // Use the level's native frame counter; g_current_level_index is a constant
 // while visiting a planet and must never be mistaken for elapsed frames.
 const auto frame=read<unsigned>(l(0,0x15f6b0));
 if(confirmation_tick_!=~0u&&frame-confirmation_tick_<45)return;
 if(notified_tick_!=~0u&&frame-notified_tick_<8)return;notified_tick_=frame;
 if(world_prompt_available()&&world_registered_tick_!=~0u&&frame-world_registered_tick_<4)return;
 message(std::string(1,char(0x12))+(italian()?" Raccogli ":" Collect ")+bounded(italian()?d->name_it:d->name_en,80),12);
}
bool NativePresentation::world_prompt_available()const{
 return candidate_position_&&finite(*candidate_position_)&&world_prompt_&&arena_.owns(world_prompt_,32)&&
  (native_hud_boundary_?api_->function("func_00234F40"):api_->function("func_001F49B0"))&&api_->function("func_001F2418")&&api_->function("func_001F75D0");
}
bool NativePresentation::queue_world_prompt(){
 ++world_prompt_calls_;
 if(native_hud_boundary_||level_transitioning()||read<int>(mode())!=0)return false;
 if(!world_prompt_available()||native_interaction_active()||!candidate_||!catalog_.find(candidate_))return false;
 const auto frame=read<unsigned>(l(0,0x15f6b0));if(confirmation_tick_!=~0u&&frame-confirmation_tick_<45)return false;
 const int count=read<int>(g(0x15f564));const auto functions=g(0x18dc40),arguments=g(0x18dd40);
 if(count<0||count>64||!valid(functions,64*4)||!valid(arguments,64*4))return false;
 bool queued=false;for(int i=0;i<count;++i)if(read<unsigned>(functions+unsigned(i)*4)==relay_&&read<unsigned>(arguments+unsigned(i)*4)==world_prompt_)queued=true;
 if(!queued){if(count==64)return false;using Add=void(*)(unsigned,unsigned);reinterpret_cast<Add>(api_->function("func_001F49B0"))(relay_,world_prompt_);const auto appended=read<int>(g(0x15f564));if(appended>=0&&appended<=64)for(int i=0;i<appended;++i)if(read<unsigned>(functions+unsigned(i)*4)==relay_&&read<unsigned>(arguments+unsigned(i)*4)==world_prompt_)queued=true;
  if(!queued){if(!(world_prompt_rejects_&64u)&&api_->log){char trace[230];std::snprintf(trace,sizeof trace,"salvage native world prompt append-mismatch count=%d after=%d functions=%08x arguments=%08x",count,appended,functions,arguments);api_->log(mod_,OPENRAC_LOG_WARN,trace);world_prompt_rejects_|=64u;}return false;}}
 if(!world_prompt_logged_&&api_->log){char trace[192];std::snprintf(trace,sizeof trace,"salvage native world prompt queued dispatch=%u argument=%08x frame=%u",world_prompt_calls_,world_prompt_,frame);api_->log(mod_,OPENRAC_LOG_INFO,trace);world_prompt_logged_=1;}
 return true;
}
void NativePresentation::remove_world_prompt(){
 if(!world_prompt_||!relay_)return;const auto count_at=g(0x15f564),functions=g(0x18dc40),arguments=g(0x18dd40);int count=read<int>(count_at);
 if(count<0||count>64||!valid(functions,64*4)||!valid(arguments,64*4))return;
 for(int i=0;i<count;){if(read<unsigned>(functions+unsigned(i)*4)!=relay_||read<unsigned>(arguments+unsigned(i)*4)!=world_prompt_){++i;continue;}
  for(int j=i+1;j<count;++j){write(functions+unsigned(j-1)*4,read<unsigned>(functions+unsigned(j)*4));write(arguments+unsigned(j-1)*4,read<unsigned>(arguments+unsigned(j)*4));}
  --count;write(count_at,count);
 }
}
void NativePresentation::draw_world_prompt(){
 if(level_transitioning()||read<int>(mode())!=0)return;
 auto diagnostic=[&](unsigned bit,const char* reason,float a=0,float b=0,float c=0){if(!(world_prompt_rejects_&bit)&&api_->log){char trace[320];std::snprintf(trace,sizeof trace,"salvage native world prompt callback reason=%s frame=%u candidate=%llu values=[%.4f,%.4f,%.4f]",reason,read<unsigned>(l(0,0x15f6b0)),(unsigned long long)candidate_,double(a),double(b),double(c));api_->log(mod_,OPENRAC_LOG_INFO,trace);world_prompt_rejects_|=bit;}};
 diagnostic(1,"entered");
 if(!world_prompt_available()||native_interaction_active()||!candidate_){diagnostic(2,"unavailable-or-native-interaction");return;}
 const auto frame=read<unsigned>(l(0,0x15f6b0));if(confirmation_tick_!=~0u&&frame-confirmation_tick_<45)return;
 const auto* d=catalog_.find(candidate_);if(!d)return;
 const auto extent=g(0x13e600);const int half_width=read<int>(extent+8),half_height=read<int>(extent+12);
 if(half_width<64||half_width>2048||half_height<64||half_height>2048){diagnostic(4,"invalid-extent",float(half_width),float(half_height));return;}
 const int width=half_width*2,height=half_height*2;
 openrac_gaddr saved=0;const auto storage=api_->stack_push(512,&saved);if(!storage)return;
 struct Pop{const openrac_api* api;unsigned saved;~Pop(){api->stack_pop(saved);}}pop{api_,saved};
 const std::array<float,4> point{{candidate_position_->x,candidate_position_->y,candidate_position_->z+std::max(.25f,d->radius+.12f),1.f}};
 const auto camera=g(0x187040);const auto basis=read<std::array<float,12>>(camera);const auto camera_position=read<std::array<float,3>>(camera+0x140);
 const float depth=basis[2]*(point[0]-camera_position[0])+basis[6]*(point[1]-camera_position[1])+basis[10]*(point[2]-camera_position[2]);
 if(!std::isfinite(depth)||depth<=.01f){diagnostic(8,"behind-camera",depth,camera_position[0],camera_position[2]);return;}
 std::memcpy(api_->memory+storage,point.data(),sizeof point);
 using Project=void(*)(unsigned,unsigned);reinterpret_cast<Project>(api_->function("func_001F2418"))(storage+16,storage);
 // SDK stack frames may live in the E0000000 guest window, outside actor RAM.
 // This range belongs to our scoped 512-byte allocation; don't route it through
 // the deliberately strict 32-MiB validation used for native world objects.
 std::array<float,4> projected{};std::memcpy(projected.data(),api_->memory+storage+16,sizeof projected);
 // Native projectedZ is before perspective division and can be negative for
 // visible points. Front/behind comes from the raw camera basis above.
 if(!std::isfinite(projected[0])||!std::isfinite(projected[1])){diagnostic(16,"invalid-projection",projected[0],projected[1],depth);return;}
 const float x=(projected[0]-float(read<int>(extent+16)))*.0625f,y=(projected[1]-float(read<int>(extent+20)))*.0625f;
 if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>width||y<0||y>height){diagnostic(32,"offscreen",x,y,depth);return;}
 const int left=std::clamp(int(x),20,width-20),top=std::clamp(int(y)-30,12,height-34);
 if(world_prompt_logged_<2&&api_->log){char trace[192];std::snprintf(trace,sizeof trace,"salvage native world prompt draw x=%.2f y=%.2f camera_depth=%.3f",double(x),double(y),double(depth));api_->log(mod_,OPENRAC_LOG_INFO,trace);world_prompt_logged_=2;}
 const auto label=std::string(1,char(0x12))+(italian()?" Raccogli":" Collect")+"\1"+bounded(italian()?d->name_it:d->name_en,40);
 std::memcpy(api_->memory+storage+80,label.c_str(),label.size()+1);
 std::int16_t area[12]={4,std::int16_t(height-4),4,std::int16_t(width-4),std::int16_t(left+1),std::int16_t(top+1),0,0,12,1,0,0};
 using Font=void(*)(unsigned,unsigned,unsigned,int);const auto font=reinterpret_cast<Font>(api_->function("func_001F75D0"));
 if(native_hud_boundary_&&!arsenal::native::prepare_hud_text(api_))return;
 std::memcpy(api_->memory+storage+48,area,sizeof area);font(storage+48,0x80000000u,storage+80,-1);
 area[4]=std::int16_t(left);area[5]=std::int16_t(top);std::memcpy(api_->memory+storage+48,area,sizeof area);font(storage+48,0x8020ffffu,storage+80,-1);
 // Queued work can be culled or skipped. Only a real draw may retire the
 // existing native subtitle, so a missing anchor never removes all guidance.
 world_registered_tick_=read<unsigned>(l(0,0x15f6b0));
 if(!last_message_.empty()&&last_message_[0]==char(0x12)&&read<int>(l(0,0x15f720))>0){const auto native=l(0,0x179218);const auto* text=reinterpret_cast<const char*>(api_->memory+native);const auto* end=static_cast<const char*>(std::memchr(text,0,256));if(end&&std::string(text,end)==last_message_)write<int>(l(0,0x15f720),0);}
}
}
