// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_presentation.hpp"
#include "text_layout.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
using namespace salvage;
namespace {
unsigned checks;
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"salvage presentation: %s\n",message);std::exit(1);}}
void check_pixel_layout(){using namespace salvage::presentation;struct Label{unsigned source,fragment;const char* it;const char* en;};constexpr Label labels[]{
#include "../src/builtin_part_labels.inc"
 };require(text_width("WW",Font::Normal)>text_width("ii",Font::Normal)&&text_width(std::string(1,char(0x12)),Font::Normal)==18,"PAL pixel policy uses actual proportional glyph advances and native controller glyph");
 for(const auto& part:labels)for(const auto* name:{part.it,part.en})for(const int width:{64,96,128,192,224,256})for(const std::uint64_t count:{std::uint64_t(1),std::numeric_limits<std::uint64_t>::max()}){const auto row=list_caption(name,count,width);require(text_width(row.text,row.font)<=width&&row.text.find('\1')==std::string::npos,"every reviewed PAL label and max-u64 count fits one inventory row by pixels");const auto title=name_caption(name,width,36);require(text_width(title.text,title.font)<=width&&std::count(title.text.begin(),title.text.end(),'\1')<=1,"long native name captions fit at most two correctly measured lines");}
 const auto wide=ellipsize("WWWWWW",text_width("iii...",Font::Normal),Font::Normal),narrow=ellipsize("iiiiii",text_width("iii...",Font::Normal),Font::Normal);require(wide.size()<narrow.size(),"pixel ellipsis does not treat narrow and wide characters as equal widths");
 require(exact_quantity(std::numeric_limits<std::uint64_t>::max())=="18 446 744 073 709 551 615","detail quantities retain every uint64 digit with native-readable grouping");require(list_caption("Testa",1,224).text=="Testa  x1","ordinary short names and quantities preserve the native one-line presentation");
}
struct Fixture;Fixture* live;
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);openrac_api api{};GuestArena arena;Catalog catalog;Inventory inventory;
 std::vector<std::string> drawn,notifications;unsigned stack=0x1e00000,sounds{},paging_depth{},paging_calls{},hud_state_calls{};std::uint64_t alpha{},test{};bool skip_draw_add=false;int overlay=1;
 std::array<float,4> projection{{4096.f,2400.f,3.f,0.f}},last_projected_world{};
 static constexpr unsigned Root=0x1d2678,Prototype=0x300000,Buttons=0x310000,Panels=0x320000,Bank=0x340000,OriginalText=0x350000;
 template<class T>void put(unsigned at,const T& value){std::memcpy(api.memory+at,&value,sizeof value);}
 template<class T>T get(unsigned at)const{T result{};std::memcpy(&result,api.memory+at,sizeof result);return result;}
 static unsigned relocate(unsigned value){return value==Root?0x380000:value;}
 static unsigned relocate_level(int,unsigned value){return value;}
 static int level(){return live?live->overlay:1;}
 static void root_update(){}
 static void native_panel(){}
 static unsigned loader(int){return 0;}
 static unsigned push(unsigned size,unsigned* saved){*saved=live->stack;const auto result=live->stack;live->stack+=(size+15)&~15u;return result;}
 static void pop(unsigned saved){live->stack=saved;}
 static void gs(int id,std::uint64_t value){if(id==0x42)live->alpha=value;if(id==0x47)live->test=value;}
 static void hud_state(){++live->hud_state_calls;live->alpha=0;live->test=0;}
 static void begin(int){require(live->alpha==0x44&&live->test==0x2004b,"inventory capture text explicitly resets state before its native paging");++live->paging_depth;++live->paging_calls;}
 static void end(){require(live->paging_depth>0,"font texture paging has matching native begin/end");--live->paging_depth;}
 static void font(unsigned,unsigned,unsigned text,int){live->drawn.emplace_back(reinterpret_cast<char*>(live->api.memory+text));}
 static int measure(unsigned text,int,unsigned){return int(std::strlen(reinterpret_cast<char*>(live->api.memory+text)))*8;}
 static std::uint64_t texture(int index){return unsigned(index);}
 static void direct_font(int,int,std::uint64_t,unsigned text,int,std::uint64_t,unsigned){live->drawn.emplace_back(reinterpret_cast<char*>(live->api.memory+text));}
 static int sound(int,int,unsigned){++live->sounds;return 0;}
 static void add_draw(unsigned callback,unsigned argument){const auto count=live->get<unsigned>(0x15f564);if(count>=64||live->skip_draw_add)return;live->put(0x18dc40+count*4,callback);live->put(0x18dd40+count*4,argument);live->put(0x15f564,count+1);}
 static void project(unsigned out,unsigned in){live->last_projected_world=live->get<std::array<float,4>>(in);live->put(out,live->projection);}
 static void notify(int id,int frames){require(id==0x4ee2,"notification borrows only the scoped native text identity");const auto source=live->get<unsigned>(Bank);const auto* text=reinterpret_cast<char*>(live->ram.data()+source);live->notifications.emplace_back(text);std::strncpy(reinterpret_cast<char*>(live->ram.data()+0x179218),text,255);live->put<int>(0x15f720,frames);}
 static void* function(const char* name){
  if(std::strcmp(name,"func_0021BB90")==0)return reinterpret_cast<void*>(&root_update);
  if(std::strcmp(name,"func_L00_002422D8")==0)return reinterpret_cast<void*>(&loader);
  if(std::strcmp(name,"func_001F4630")==0)return reinterpret_cast<void*>(&begin);
  if(std::strcmp(name,"func_001F4748")==0)return reinterpret_cast<void*>(&end);
  if(std::strcmp(name,"func_001F75D0")==0||std::strcmp(name,"func_001F7560")==0)return reinterpret_cast<void*>(&font);
  if(std::strcmp(name,"func_L00_00264DB8")==0)return reinterpret_cast<void*>(&notify);
  if(std::strcmp(name,"func_001F65B0")==0)return reinterpret_cast<void*>(&measure);if(std::strcmp(name,"func_001F4868")==0)return reinterpret_cast<void*>(&texture);if(std::strcmp(name,"func_001F6668")==0)return reinterpret_cast<void*>(&direct_font);
  if(std::strcmp(name,"func_0022ED80")==0)return reinterpret_cast<void*>(&sound);
  if(std::strcmp(name,"func_00234C98")==0)return reinterpret_cast<void*>(&gs);
  if(std::strcmp(name,"func_00234F40")==0)return reinterpret_cast<void*>(&hud_state);
  if(std::strcmp(name,"func_L00_00237B90")==0)return reinterpret_cast<void*>(&native_panel);
  if(std::strcmp(name,"func_001F49B0")==0)return reinterpret_cast<void*>(&add_draw);
  if(std::strcmp(name,"func_001F2418")==0)return reinterpret_cast<void*>(&project);
  return nullptr;
 }
 static void* function_at(unsigned code){if(code==0x21bb90||code==0x21c1b0)return reinterpret_cast<void*>(&root_update);if(code==0x256268)return reinterpret_cast<void*>(&loader);if(code==0x237b90)return reinterpret_cast<void*>(&native_panel);return nullptr;}
 Fixture(){
  live=this;api.memory=ram.data();api.relocate_data=relocate;api.relocate_level_data=relocate_level;api.loaded_level=level;api.function=function;api.function_at=function_at;api.stack_push=push;api.stack_pop=pop;
  arena.reserve(0x1200000,65536);put<int>(NativeMemory::Language,5);put<int>(0x15f6a8,0);put<int>(NativeMemory::Hero+0x22a8,4);
  put<float>(0x187040,1.f);put<float>(0x187040+20,1.f);put<float>(0x187040+40,1.f);
  put<int>(Root+0x3c,2);put<unsigned>(Prototype+0x3c,3);
  for(unsigned i=0;i<7;++i){const auto b=Buttons+i*0x100,items=b+0x80;put(Root+0x44+i*4,b);put(Root+i*4,10+i);put(b,0x21bb90u);put(b+4,0x21c1b0u);put(b+0x34,items);put(items,(3u<<16)|(i==6?0x4fc2u:0x4ee2u+i));put(items+4,Prototype);put(b+0x38,Buttons+((i+6)%7)*0x100);put(b+0x3c,Buttons+((i+1)%7)*0x100);put<int>(b+0x20,256);put<int>(b+0x24,48);}
  put<unsigned>(Root+0x40,Buttons);put<unsigned>(Prototype+0x38,Root);
  for(unsigned i=0;i<9;++i){const auto screen=Panels+i*0x100;put(Prototype+0x44+i*4,screen);put<int>(screen+0x20,256);put<int>(screen+0x24,i==2?256:96);}
  put<unsigned>(Prototype+0x40,Panels+0x200);
  put<unsigned>(NativeMemory::MenuSystem+0x110,1);put<unsigned>(0x15f780,Bank);put<int>(0x1997fc,1);put<unsigned>(Bank,OriginalText);put<int>(Bank+4,0x4ee2);std::strcpy(reinterpret_cast<char*>(ram.data()+OriginalText),"Armi");
  PartDescriptor p;p.stable_id="robot.test.head";p.name_it="Testa robot";p.name_en="Robot head";p.source_it="Robot esploratore";p.source_en="Scout robot";p.fragment_class=461;p.source_class=459;
  require(catalog.register_part(p)==Status::Ok,"test catalog firstpart");p.stable_id="robot.test.leg";p.name_it="Zampa robot";p.name_en="Robot leg";p.fragment_class=462;require(catalog.register_part(p)==Status::Ok,"test catalog secondpart");
  for(const auto& [id,d]:catalog.entries())require(inventory.apply(catalog,{{id,3,Operation::Add}})==Status::Ok,"test inventory");
 }
 bool has(const std::string& text)const{return std::any_of(drawn.begin(),drawn.end(),[&](const std::string& value){return value.find(text)!=std::string::npos;});}
};
}
int main(){
 check_pixel_layout();
 Fixture f;const auto campaign=std::vector<unsigned char>(f.ram.begin()+0x13d000,f.ram.begin()+0x13f000);
 NativePresentation ui(&f.api,nullptr,f.arena,f.catalog,f.inventory);ui.frame();
 require(ui.ready()&&ui.page_address()&&ui.entry_address(),"native menu template cloned into reserved level arena");
 require(f.arena.owns(ui.page_address(),0x88)&&f.arena.owns(ui.entry_address(),0xa4),"all published custom objects are arena-owned");
 f.put<int>(0x15f6a8,6);const auto loading_ram=f.ram;const auto loading_page=ui.page_address(),loading_used=f.arena.used();ui.frame();ui.before_pause_render();ui.after_pause_render();ui.draw_hud_prompt();ui.invoke_callback(ui.entry_address());require(!ui.queue_world_prompt()&&f.ram==loading_ram&&ui.page_address()==loading_page&&f.arena.used()==loading_used,"loading mode6 never binds, draws, retires pages or writes an old guest arena");f.overlay=-1;ui.frame();ui.before_pause_render();ui.after_pause_render();ui.draw_hud_prompt();require(f.ram==loading_ram&&ui.page_address()==loading_page,"invalid native overlay does not reset an incoming WAD's old page storage");f.overlay=1;f.put<int>(0x15f6a8,0);
 require(f.get<unsigned>(Fixture::Root+0x44+7*4)==ui.entry_address(),"Componenti is an additional pause entry");
 f.put<unsigned>(Fixture::Buttons+0x38,Fixture::Buttons+5*0x100);ui.frame();
 require(f.get<unsigned>(Fixture::Buttons+0x38)==ui.entry_address(),"native hidden-Goodies initializer cannot leave first Up pointing to the old last row");
 for(unsigned i=0;i<7;++i)require(f.get<unsigned>(Fixture::Root+0x44+i*4)==Fixture::Buttons+i*0x100,"original pause entries and Goodies remain present");
 require(std::equal(campaign.begin(),campaign.end(),f.ram.begin()+0x13d000),"menu access never modifies story completion or campaign state");
 require(ui.owns_callback(ui.entry_address())&&!ui.owns_callback(1)&&!ui.owns_callback(0x250000),"only session-owned menu callbacks intercept the level relay");
 f.put<unsigned>(NativeMemory::MenuSystem+4,Fixture::Root);
 ui.invoke_callback(ui.entry_address());require(f.has("Componenti"),"pause entry uses native font with Italian label");
 f.put<unsigned>(NativeMemory::MenuSystem+4,ui.page_address());f.put<int>(0x15f6a8,3);
 require(ui.inventory_open()&&ui.native_interaction_active(),"inventory page retains native pause ownership");
 require(!ui.route_triangle(false)&&ui.route_triangle(true)&&f.get<unsigned>(NativeMemory::MenuSystem+8)==Fixture::Root,"Triangle edge requests the native back page transition");
 f.put<unsigned>(NativeMemory::MenuSystem+4,Fixture::Root);f.put<unsigned>(NativeMemory::MenuSystem+8,0);
 require(ui.route_triangle(true)&&f.get<unsigned>(NativeMemory::MenuSystem+8)==0,"held Triangle remains claimed after leaving Componenti without another back action");
 require(!ui.route_triangle(false)&&!ui.route_triangle(true),"release ends ownership and a fresh root Triangle remains native");
 ui.route_triangle(false);f.put<unsigned>(NativeMemory::MenuSystem+4,ui.page_address());
 f.put<unsigned>(NativeMemory::Pad+0x1c4,0x800);ui.frame();require(f.get<int>(NativeMemory::MenuSystem+0xc)==1,"Start requests native pause close from Componenti");
 f.put<unsigned>(NativeMemory::Pad+0x1c4,0);f.put<int>(NativeMemory::MenuSystem+0xc,0);
 unsigned miniature_calls=0;ui.set_thumbnail_renderer([&](unsigned screen,const PartDescriptor& part){require(f.arena.owns(screen,0xa4)&&part.id!=0,"thumbnail callback receives selected descriptor and own native target");++miniature_calls;});
 for(unsigned i=0;i<14;++i){const auto screen=f.get<unsigned>(ui.page_address()+0x44+i*4);if(screen)ui.invoke_callback(screen);}
 require(f.has("Testa robot")&&f.has("Zampa robot")&&f.has("Robot esploratore")&&f.has("Quantita: 3"),"native panes show piece names provenance and exact quantities");
 require(miniature_calls==1,"actual miniature seam draws once in correct native pane");
 const auto list=f.get<unsigned>(ui.page_address()+0x40);f.put<unsigned>(NativeMemory::Pad+0x1c4,0x4000);f.put<unsigned>(NativeMemory::MenuSystem+0x110,2);
 ui.invoke_callback(list);ui.invoke_callback(list);require(ui.selection()==1&&f.sounds==1,"held/repeated draws consume native repeat input only once per update");
 f.put<unsigned>(NativeMemory::MenuSystem+0x110,3);ui.invoke_callback(list);require(ui.selection()==0&&f.sounds==2,"native navigation wraps across inventory rows");
 f.put<int>(0x15f6a8,0);f.put<unsigned>(NativeMemory::MenuSystem+4,Fixture::Root);f.put<int>(0x15f720,0);
 require(!ui.native_interaction_active(),"no native interaction blocks a normal collection context");
 const auto first=f.catalog.entries().begin()->first;ui.set_candidate(first);f.put<unsigned>(0x15f6b0,20);ui.frame();
 require(!f.notifications.empty()&&f.notifications.back().find(std::string(1,char(0x12))+" Raccogli ")==0,"nearby component prompt uses actual native Triangle glyph");
 require(f.get<unsigned>(Fixture::Bank)==Fixture::OriginalText&&std::strcmp(reinterpret_cast<char*>(f.ram.data()+Fixture::OriginalText),"Armi")==0,"scoped notification restores stock text binding immediately");
 const auto old_count=f.notifications.size();std::strcpy(reinterpret_cast<char*>(f.ram.data()+0x179218),"Parla con il venditore");f.put<int>(0x15f720,70);f.put<unsigned>(0x15f6b0,40);ui.frame();
 require(ui.native_interaction_active()&&f.notifications.size()==old_count,"foreign native message and interaction take priority over collection prompt");
 f.put<int>(0x15f720,0);ui.notify_collected(first);require(f.notifications.back().find("+1 ")==0&&f.get<unsigned>(Fixture::Bank)==Fixture::OriginalText,"collection confirmation uses native channel without persistent name changes");ui.set_candidate(first);
 const auto confirmations=f.notifications.size();f.put<unsigned>(0x15f6b0,50);ui.frame();require(f.notifications.size()==confirmations,"next nearby component cannot immediately overwrite collection confirmation");
 f.put<unsigned>(0x15f6b0,90);ui.frame();require(f.notifications.size()==confirmations+1&&f.notifications.back()[0]==char(0x12),"prompt resumes on real elapsed level frames after confirmation");
 const auto native_count=f.notifications.size();f.put<int>(0x15f674,1);f.put<unsigned>(0x15f6b0,100);ui.frame();
 require(ui.native_interaction_active()&&f.notifications.size()==native_count,"retail help selector gives vendors ships and vehicles priority without replacing their prompt");
 f.put<int>(0x15f674,0);const unsigned channel=0x17dd50+12*0x90;
 f.put<unsigned>(channel+0x18,0x237b90);f.put<int>(channel+0x7c,10);f.put<int>(channel+0x6c,4);f.put<unsigned>(0x15f6b0,110);ui.frame();
 require(ui.native_interaction_active()&&f.notifications.size()==native_count,"visible native NPC and Gold vendor HUD channel wins through resolved guest callback");
 f.put<int>(channel+0x7c,0);f.put<int>(channel+0x6c,-6);
 require(!ui.native_interaction_active(),"expired native HUD callback alone cannot permanently block collection");
 f.put<unsigned>(channel+0x38,0x237b90);f.put<unsigned>(channel+0x20,0);f.put<int>(channel+0x68,1);
 require(ui.native_interaction_active(),"queued NPC prompt owns Triangle before its first visible frame");
 f.put<unsigned>(channel+0x20,0xffff);require(!ui.native_interaction_active(),"queued native HUD removal does not reserve interaction");
 f.put<int>(channel+0x68,0);f.put<unsigned>(channel+0x18,0x256268);f.put<int>(channel+0x7c,10);
 require(!ui.native_interaction_active(),"other resolved callbacks are not mistaken for native interaction panels");
 f.put<int>(channel+0x7c,0);f.put<unsigned>(channel+0x18,0);f.put<unsigned>(channel+0x38,0);
 auto unknown=f.inventory.snapshot();unknown.emplace(1,1);unknown.emplace(std::numeric_limits<PartId>::max(),2);require(f.inventory.restore(unknown)==Status::Ok,"unknown component inventory remains restorable");
 f.drawn.clear();f.put<int>(0x15f6a8,3);f.put<unsigned>(NativeMemory::MenuSystem+4,ui.page_address());f.put<unsigned>(NativeMemory::Pad+0x1c4,0);f.put<unsigned>(NativeMemory::MenuSystem+0x110,4);ui.invoke_callback(list);
 require(f.drawn.size()>=4&&f.drawn[0].find("Testa robot")==0&&f.drawn[1].find("Zampa robot")==0&&f.drawn[2].find("Componente ")==0&&f.drawn[2].substr(f.drawn[2].size()-2)=="x1"&&f.drawn[3].substr(f.drawn[3].size()-2)=="x2","known names precede unknown IDs, whose stable ordering survives pixel ellipsis");
 {const auto saved_inventory=f.inventory.snapshot();PartDescriptor long_part;long_part.stable_id="private.ui.long-label";long_part.name_it="Supporto del cannone della torretta della flotta";long_part.name_en="Fleet turret gun mount";long_part.source_it="Torretta della flotta";long_part.source_en="Fleet turret";require(f.catalog.register_part(long_part)==Status::Ok,"long-label display fixture registers copied metadata");const auto long_id=stable_part_id(long_part.stable_id);require(f.inventory.restore({{long_id,std::numeric_limits<Quantity>::max()}})==Status::Ok,"max-u64 UI fixture restores exact inventory data");f.drawn.clear();ui.invoke_callback(f.get<unsigned>(ui.page_address()+0x44+4));require(f.has(long_part.name_it)&&f.has("18 446 744 073 709 551 615"),"native detail pane receives the entire long name and every quantity digit");f.drawn.clear();ui.invoke_callback(f.get<unsigned>(ui.page_address()+0x44+4*4));auto joined=f.drawn.empty()?std::string{}:f.drawn.back();joined.erase(std::remove(joined.begin(),joined.end(),'\1'),joined.end());joined.erase(std::remove(joined.begin(),joined.end(),' '),joined.end());auto expected=long_part.name_it;expected.erase(std::remove(expected.begin(),expected.end(),' '),expected.end());require(joined==expected,"long name pane retains the complete caption over two measured native lines");require(f.inventory.restore(saved_inventory)==Status::Ok,"UI fixtures restore original inventory after display verification");}
 f.put<int>(0x15f6a8,0);f.put<unsigned>(NativeMemory::MenuSystem+4,Fixture::Root);
 f.put<int>(0x13e608,256);f.put<int>(0x13e60c,224);f.put<unsigned>(0x15f6b0,200);f.put<int>(0x15f720,0);ui.set_candidate(first,Vec3{1,2,3});
 const auto before_anchor=f.notifications.size();ui.frame();require(f.notifications.size()==before_anchor+1,"position-aware candidate retains native fallback until the renderer actually accepts its callback");
 f.skip_draw_add=true;require(!ui.queue_world_prompt()&&f.get<int>(0x15f720)>0,"an append call that does not reach the native list never suppresses the fallback");f.skip_draw_add=false;
 require(ui.queue_world_prompt()&&f.get<unsigned>(0x15f564)==1,"render boundary registers one native AddDrawCallback");
 require(f.get<int>(0x15f720)>0,"registration alone preserves the native fallback until the callback really draws");
 const auto prompt=f.get<unsigned>(0x18dd40);require(ui.owns_callback(prompt)&&f.arena.owns(prompt,32),"world prompt callback routes only its reserved owned argument");
 require(ui.queue_world_prompt()&&f.get<unsigned>(0x15f564)==1,"repeated render dispatcher registration is idempotent");
 f.drawn.clear();const auto paging_before=f.paging_calls;ui.invoke_callback(prompt);require(f.drawn.size()==2&&f.drawn[0].find(std::string(1,char(0x12))+" Raccogli\1")==0&&f.drawn[0]==f.drawn[1]&&f.paging_calls==paging_before&&f.paging_depth==0,"anchored prompt draws native Triangle glyph and shadow without restarting the active frame's texture paging");
 require(f.get<int>(0x15f720)==0,"actual anchored rendering retires only its own duplicate native fallback");
 require(f.last_projected_world[0]==1&&f.last_projected_world[1]==2&&f.last_projected_world[2]>3&&f.stack==0x1e00000,"native projection follows the actual fragment position and stack is balanced");
 f.projection[2]=-1;f.drawn.clear();ui.invoke_callback(prompt);require(!f.drawn.empty(),"negative native pre-divide projectionZ is valid for a visible target");f.drawn.clear();f.put<float>(0x187040+40,-1.f);ui.invoke_callback(prompt);require(f.drawn.empty(),"world prompt is culled behind the raw native camera basis");f.put<float>(0x187040+40,1.f);f.projection[2]=3;
 f.projection[0]=-100;ui.invoke_callback(prompt);require(f.drawn.empty(),"offscreen fragment cannot draw a floating HUD label");f.projection[0]=4096;
#ifdef _WIN32
 // Reserve virtual address space sparsely; commit only normal actor RAM and a
 // separate SDK stack page. The real SDK stack is not inside the 32-MiB game.
 auto* mapped=static_cast<unsigned char*>(VirtualAlloc(nullptr,std::size_t(0xe0100000ull),MEM_RESERVE,PAGE_NOACCESS));
 require(mapped&&VirtualAlloc(mapped,f.ram.size(),MEM_COMMIT,PAGE_READWRITE)&&VirtualAlloc(mapped+0xe0000000u,0x100000,MEM_COMMIT,PAGE_READWRITE),"sparse host fixture maps the high SDK-owned stack independently of actor RAM");
 std::memcpy(mapped,f.ram.data(),f.ram.size());f.api.memory=mapped;const auto low_stack=f.stack;f.stack=0xe0000000u;f.drawn.clear();ui.invoke_callback(prompt);
 require(f.drawn.size()==2&&f.stack==0xe0000000u,"realistic E0000000 SDK stack retains projected coordinates and draws the anchored prompt");
 f.api.memory=f.ram.data();f.stack=low_stack;VirtualFree(mapped,0,MEM_RELEASE);
#endif
 ui.use_native_hud_boundary(true);require(f.get<unsigned>(0x15f564)==0&&!ui.queue_world_prompt(),"native HUD integration removes its previous callback and never queues a world-list draw");f.drawn.clear();const auto hud_paging=f.paging_calls,hud_state=f.hud_state_calls;f.alpha=0xdeadbeef;f.test=~0ull;ui.draw_hud_prompt();require(f.drawn.size()==2&&f.paging_calls==hud_paging&&f.paging_depth==0&&f.hud_state_calls==hud_state+1,"dedicated HUD boundary restores stock HUD register state without resetting active texture paging");ui.use_native_hud_boundary(false);
 f.put<int>(0x15f674,2);require(!ui.queue_world_prompt(),"native ship/vendor priority also suppresses anchored registration");f.put<int>(0x15f674,0);
 f.put<unsigned>(0x15f564,64);f.put<unsigned>(0x18dd40,0);require(!ui.queue_world_prompt()&&f.get<unsigned>(0x15f564)==64,"saturated native callback list is never overwritten");f.put<unsigned>(0x15f564,1);f.put<unsigned>(0x18dd40,prompt);
 const auto foreign_callback=0x200100u,foreign_argument=0x200200u;f.put(0x18dc44,foreign_callback);f.put(0x18dd44,foreign_argument);f.put<unsigned>(0x15f564,2);
 std::array<std::array<float,16>,8> quads{};
 for(unsigned i=0;i<8;++i){
  const unsigned actor=0x360000+i*0x100,vars=0x370000+i*0x100;f.put<unsigned>(0x1d6120+i*4,actor);f.put<unsigned>(actor+0x78,vars);f.put<unsigned>(actor+0x24,0x380000);f.put<unsigned short>(actor+0xa6,0x472);std::array<float,12> basis{};basis[0]=basis[5]=basis[10]=1;f.put(actor+0xc0,basis);
  for(unsigned c=0;c<4;++c){quads[i][c*4]=c%2?10.f:-10.f;quads[i][c*4+1]=float(std::min(i,6u))*20.f+(c/2?4.f:-4.f);quads[i][c*4+3]=1.f;}
  f.put(vars,quads[i]);
 }
 f.put<unsigned>(NativeMemory::MenuSystem+0x110,10);ui.frame();ui.before_pause_render();
 auto center_y=[&](unsigned i){const auto q=f.get<std::array<float,16>>(0x370000+i*0x100);return (q[1]+q[5]+q[9]+q[13])*.25f;};
 require(std::abs(center_y(7)-120.f)<.0001f,"Componenti occupies hidden seventh native row when Goodies is unavailable");
 ui.after_pause_render();ui.frame();f.put<int>(NativeMemory::MenuSystem+0xd8,1);f.put<unsigned>(NativeMemory::MenuSystem+0x110,11);ui.frame();ui.before_pause_render();
 require(std::abs(center_y(7)-122.5f)<.0001f&&std::abs(center_y(6)-105.f)<.0001f,"all eight visible native rows are compacted without replacing Goodies");
 ui.after_pause_render();ui.frame();for(unsigned i=12;i<50;++i){f.put<unsigned>(NativeMemory::MenuSystem+0x110,i);ui.frame();ui.before_pause_render();require(std::abs(center_y(7)-122.5f)<.0001f,"native panel adjustment is idempotent when updates reuse the same quad");ui.after_pause_render();}
 f.put<unsigned>(NativeMemory::MenuSystem+4,ui.page_address());f.put<unsigned>(NativeMemory::MenuSystem+0x110,51);ui.frame();
 require(f.get<std::array<float,16>>(0x370000)==quads[0]&&f.get<std::array<float,16>>(0x370700)==quads[7],"leaving root restores all owned native panel geometry before inventory layout");
 f.put<unsigned>(Fixture::Root+0x44+7*4,0xabcdef);ui.reset();
 require(f.get<unsigned>(Fixture::Root+0x44+7*4)==0xabcdef,"teardown never overwrites a later mod's replacement");
 require(!ui.ready()&&!ui.owns_callback(list)&&f.stack==0x1e00000,"teardown retires routing and temporary guest stack is balanced");
 require(f.get<unsigned>(0x15f564)==1&&f.get<unsigned>(0x18dc40)==foreign_callback&&f.get<unsigned>(0x18dd40)==foreign_argument,"teardown removes only owned queued prompt and preserves other native callbacks");
 std::printf("salvage native presentation: %u checks passed\n",checks);
}
