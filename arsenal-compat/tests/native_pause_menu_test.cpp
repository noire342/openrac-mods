// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_pause_menu.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace arsenal::pause;
namespace {unsigned checks;void require(bool v,const char* message){++checks;if(!v){std::fprintf(stderr,"native pause: %s\n",message);std::exit(1);}}
struct Fixture;Fixture* live;
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);openrac_api api{};unsigned arena=0x1200000,stack=0x1e00000,tick=1,animations{};std::vector<std::string> drawn;std::vector<EntryId> activated;std::uint64_t alpha{},test{};std::vector<unsigned> draw_order;unsigned windows{},last_metrics{},last_texture{};int last_x{},last_y{};
 static constexpr unsigned Root=0x1d2678,Prototype=0x300000,Buttons=0x310000,System=0x1d5f70;
 template<class T>void put(unsigned at,const T& v){std::memcpy(ram.data()+at,&v,sizeof v);}template<class T>T get(unsigned at){T v{};std::memcpy(&v,ram.data()+at,sizeof v);return v;}
 static unsigned relocate(unsigned v){return v==Root?0x380000:v;}static unsigned level_data(int,unsigned v){return v;}static int level(){return 1;}static void input(){}static void loader(){}
 static void* at(unsigned v){return v==0x21bb90||v==0x21c1b0?reinterpret_cast<void*>(&input):v==0x256268?reinterpret_cast<void*>(&loader):nullptr;}
 static unsigned push(unsigned n,unsigned* saved){*saved=live->stack;const auto p=live->stack;live->stack+=(n+15)&~15u;return p;}static void pop(unsigned v){live->stack=v;}
 static void gs(int id,std::uint64_t value){live->draw_order.push_back(unsigned(id));if(id==0x42)live->alpha=value;if(id==0x47)live->test=value;}
 static void begin(int){require(live->alpha==0x44&&live->test==0x2004b,"custom callback owns ALPHA and TEST before its capture paging");live->draw_order.push_back(1);}static void end(){live->draw_order.push_back(3);}static void window(unsigned,unsigned,unsigned,int){++live->windows;}
 static int measure(unsigned t,int,unsigned metrics){return int(std::strlen(reinterpret_cast<char*>(live->ram.data()+t)))*(metrics==0x1dfb10?10:metrics==0x1df770?6:8);}
 static std::uint64_t texture(int i){live->last_texture=unsigned(i);return 0x1234567800000000ull+unsigned(i);}
 static void font(int x,int y,std::uint64_t,unsigned t,int,std::uint64_t texture,unsigned metrics){require(texture==0x1234567800000000ull+live->last_texture,"FontPrint keeps all64bits of its native texture descriptor");live->draw_order.push_back(2);live->last_x=x;live->last_y=y;live->last_metrics=metrics;live->drawn.emplace_back(reinterpret_cast<char*>(live->ram.data()+t));}
 static void animate(unsigned actor,int sequence,int frame){++live->animations;live->put<unsigned char>(actor+0x52,static_cast<unsigned char>(sequence));live->put<unsigned char>(actor+0x50,static_cast<unsigned char>(frame));}
 static void attach(unsigned actor){const auto vars=live->get<unsigned>(actor+0x78);const auto sequence=live->get<unsigned char>(actor+0x52);const auto basis=live->get<std::array<float,12>>(actor+0xc0);const auto position=live->get<std::array<float,4>>(actor+0x10);std::array<float,16> corners{};for(unsigned i=0;i<4;++i){const float local[3]={i%2?10.f:-10.f,float(int(sequence)-10)*20.f+(i/2?4.f:-4.f),0.f};for(unsigned k=0;k<3;++k){corners[i*4+k]=position[k];for(unsigned j=0;j<3;++j)corners[i*4+k]+=basis[j*4+k]*local[j];}corners[i*4+3]=1;}live->put(vars,corners);}
 static void* function(const char* n){if(!std::strcmp(n,"func_00234C98"))return reinterpret_cast<void*>(&gs);if(!std::strcmp(n,"func_001F4630"))return reinterpret_cast<void*>(&begin);if(!std::strcmp(n,"func_001F4748"))return reinterpret_cast<void*>(&end);if(!std::strcmp(n,"func_001F7560"))return reinterpret_cast<void*>(&window);if(!std::strcmp(n,"func_001F65B0"))return reinterpret_cast<void*>(&measure);if(!std::strcmp(n,"func_001F4868"))return reinterpret_cast<void*>(&texture);if(!std::strcmp(n,"func_001F6668"))return reinterpret_cast<void*>(&font);if(!std::strcmp(n,"func_00213D28"))return reinterpret_cast<void*>(&animate);if(!std::strcmp(n,"func_0023B5D0"))return reinterpret_cast<void*>(&attach);return nullptr;}
 Fixture(){live=this;api.memory=ram.data();api.relocate_data=relocate;api.relocate_level_data=level_data;api.loaded_level=level;api.function_at=at;api.function=function;api.stack_push=push;api.stack_pop=pop;
  put<int>(Root+0x3c,2);put<unsigned>(Root+0x40,Buttons);put<unsigned>(Prototype+0x38,Root);put<unsigned>(System+4,Root);put<int>(0x15ee88,5);
  for(unsigned i=0;i<7;++i){const unsigned s=Buttons+i*0x100,list=s+0xa8;put(Root+0x44+i*4,s);put(Root+i*4,10u+i);put(s,0x21bb90u);put(s+4,0x21c1b0u);put(s+0x34,list);put(list,(3u<<16)|(i==6?0x4fc2u:0x4ee2u+i));put(list+4,Prototype);put(s+0x38,Buttons+((i+6)%7)*0x100);put(s+0x3c,Buttons+((i+1)%7)*0x100);put<int>(s+0x20,256);put<int>(s+0x24,48);}
  for(unsigned row=0;row<14;++row){const unsigned actor=0x340000+row*0x100,vars=0x350000+row*0x100;put<unsigned>(0x1d6120+row*4,actor);put<unsigned>(actor+0x78,vars);put<unsigned>(actor+0x24,0x360000);put<unsigned short>(actor+0xa6,0x472);std::array<float,12> basis{};basis[0]=basis[5]=basis[10]=1;put(actor+0xc0,basis);animate(actor,10+int(std::min(row,7u)),1);put<float>(actor+0x54,1.f);put<unsigned>(actor+0x64,0x370000+row*0x100);put<unsigned>(actor+0x68,0x390000+row*0x100);put<unsigned>(actor+0x6c,0x3a0000+row*0x100);attach(actor);put<int>(0x1b2740+row*4,1);}
  // Native slot7 reuses the last stock root animation (row6), whereas slot8
  // has its own per-instance manipulators and is only used by the directory.
  animate(0x340700,16,1);attach(0x340700);
 }
 void next(NativePauseMenu& menu){put(System+0x110,++tick);require(menu.frame(),"native frame composes a valid ring");menu.before_render();menu.after_render();menu.after_capture();}
 NativePauseMenu::Item entry(unsigned i){return {{0x8000000000000000ull+i,1000+i,true},"Menu "+std::to_string(i),"Menu "+std::to_string(i),[this](EntryId id){activated.push_back(id);}};}
 void cross(NativePauseMenu& menu,unsigned screen){put<unsigned>(System+0x110,++tick);put<unsigned>(0x13ca40+0x1c4,0x40);menu.invoke_callback(screen);menu.invoke_callback(screen);put<unsigned>(0x13ca40+0x1c4,0);}
};}
void verify_transition_leases(){
 for(const bool backward:{false,true}){
  Fixture f;NativePauseMenu menu(&f.api,[&](std::size_t n){const auto p=f.arena;f.arena+=unsigned(n+15)&~15u;return p;});
  std::vector<NativePauseMenu::Item> entries;for(unsigned i=1;i<=32;++i)entries.push_back(f.entry(i));
  require(menu.set_entries(entries)==Status::Ok&&menu.bind(0x256268),"transition fixture binds32 registered entries");
  f.put<int>(Fixture::System+0xd8,1);for(unsigned i=0;i<20;++i)f.next(menu);
  const auto directory=menu.directory_address();
  // Native update_pause_menu uses destination animations forward and old-page
  // animations in reverse. Both directions therefore animate directory slots.
  for(unsigned slot=0;slot<14;++slot){const auto actor=0x340000+slot*0x100;Fixture::animate(actor,int(f.get<unsigned>(directory+slot*4)),3);f.put<unsigned char>(actor+0x51,4);f.put<float>(actor+0x54,.375f);f.put<float>(actor+0x58,backward?-1.f:1.f);f.put<unsigned short>(actor+0x34,0);Fixture::attach(actor);f.put<int>(0x1b2740+slot*4,1);}
  f.put<unsigned>(Fixture::System+4,0);f.put<int>(Fixture::System,1);
  f.put<unsigned>(Fixture::System+0xd0,backward?directory:Fixture::Root);f.put<unsigned>(Fixture::System+8,backward?Fixture::Root:directory);
  const auto before=f.ram;const auto calls=f.animations;
  const auto anim=f.get<std::array<unsigned char,36>>(0x340700+0x50);
  menu.before_render();
  require(menu.borrowed_render_rows()==8,"current0 native forward/back transition retains eight physical row leases");
  require(f.get<int>(0x1b2740+6*4)==0&&(f.get<unsigned short>(0x340600+0x34)&1u),"current0 transition suppresses unused Goodies actor6");
  require(f.get<int>(0x1b2740+7*4)==1&&f.get<int>(0x1b2740+8*4)==1&&!(f.get<unsigned short>(0x340800+0x34)&1u),"current0 transition keeps directory row7 and footer8 active");
  require(f.animations==calls&&f.get<std::array<unsigned char,36>>(0x340700+0x50)==anim,"transition fitting preserves live native closing/opening animation bytes and pointers");
  const auto basis=f.get<std::array<float,12>>(0x340700+0xc0);require(std::abs(basis[5]-.875f)<.0001f,"transition physical mesh keeps the eight-row vertical fit");
  const auto a=f.get<std::array<float,16>>(0x350700),b=f.get<std::array<float,16>>(0x350800);float ca=0,cb=0;for(unsigned i=0;i<4;++i){ca+=a[i*4+1]*.25f;cb+=b[i*4+1]*.25f;}
  require(std::abs(cb-ca-17.5f)<.001f,"duplicate native row6 sequences receive distinct directory rows without cancelling animation motion");
  menu.after_render();require(f.get<unsigned short>(0x340600+0x34)&1u,"transition unused actors remain hidden until frontend capture");
  menu.after_capture();require(f.ram==before,"transition geometry, visibility and animation restore byte-exactly after capture");
  for(const auto owner:{Fixture::Root,directory}){
   f.put<unsigned>(Fixture::System+4,0);f.put<unsigned>(Fixture::System+8,0);f.put<unsigned>(Fixture::System+0xd0,owner);f.put<int>(Fixture::System,0x14);
   const auto closed=f.ram;menu.before_render();menu.after_render();menu.after_capture();require(!menu.borrowed_render_rows()&&f.ram==closed,"native Root/Directory close state14 never revives deleted preview actors");
  }
 }
}
int main(){Fixture f;f.put<unsigned>(Fixture::Buttons+0x38,0);f.put<unsigned>(Fixture::Buttons+0x3c,0);NativePauseMenu menu(&f.api,[&](std::size_t n){const auto p=f.arena;f.arena+=unsigned(n+15)&~15u;return p;});
 require(menu.set_entries({f.entry(1)})==Status::Ok&&menu.bind(0x256268),"one producer attaches via native root discovery");const auto root_entry=menu.entry_address();
 require(f.get<unsigned>(root_entry+0x38)!=0&&f.get<unsigned>(root_entry+0x3c)!=0,"pre-pause zero neighbours are initialized for owned clones");
 require(f.get<unsigned>(Fixture::Root+0x44+7*4)==root_entry,"adapter owns one additional root slot");
 for(unsigned repeat=0;repeat<20;++repeat){const unsigned last=Fixture::Buttons+(repeat%2?6:5)*0x100;f.put<int>(Fixture::System+0xd8,int(repeat%2));f.put(Fixture::Buttons+0x38,last);f.put(last+0x3c,Fixture::Buttons);f.next(menu);
  require(f.get<unsigned>(Fixture::Buttons+0x38)==root_entry&&f.get<unsigned>(root_entry+0x3c)==Fixture::Buttons&&f.get<unsigned>(last+0x3c)==root_entry&&f.get<unsigned>(root_entry+0x38)==last,"both wrap directions survive native Goodies visibility and reopen rewrites");}
 f.drawn.clear();f.alpha=0xdeadbeef;f.test=~0ull;f.draw_order.clear();menu.invoke_callback(root_entry);require(!f.drawn.empty()&&f.drawn.back()=="Menu 1","single custom entry keeps its direct label");require(f.draw_order==std::vector<unsigned>{0x42,0x47,1,2,3},"capture callback resets inherited producer state in exact native register/paging/font order");
 require(!f.windows&&f.last_x==104&&f.last_y==17&&f.last_metrics==0x1df3d0&&f.last_texture==1,"normal list label uses direct FontPrint with native measured centering and row baseline, never FontPrintWindow");
 const auto original_flags=f.get<unsigned>(root_entry+0x10);for(const auto bank:{4u,8u,12u,0x10u}){f.put<unsigned>(root_entry+0x30,bank);menu.invoke_callback(root_entry);const bool small=(bank&8u)!=0,large=!small&&(bank&4u);const auto metrics=small?0x1df770u:large?0x1dfb10u:0x1df3d0u;const int height=small?10:large?14:12,font_width=small?6:large?10:8;const int row=(bank&0x10u)?height+3:24;require(f.last_metrics==metrics&&f.last_texture==unsigned(small?2:large?3:1)&&f.last_x==128-6*font_width/2&&f.last_y==row-height/2-1,"copied descriptor flags choose exactly native normal/large/small font metrics, texture and baseline");require(f.get<unsigned>(root_entry+0x10)==original_flags&&!f.windows,"font bank selection keeps native capture flags and callback return path unchanged");}
 f.put<unsigned>(root_entry+0x30,0);f.put<int>(root_entry+0x20,40);menu.invoke_callback(root_entry);require(f.drawn.back()=="M..."&&f.last_x>=4,"long labels fit by measured glyph pixels in the same font bank");f.put<int>(root_entry+0x20,256);
 f.put<unsigned>(Fixture::Root+0x40,root_entry);f.cross(menu,root_entry);const auto old_activation_tick=f.tick;f.put<unsigned>(Fixture::System+0x110,0);require(menu.frame(),"native reopen resets its update counter");f.tick=old_activation_tick-1;f.cross(menu,root_entry);
 require(f.activated.size()==2,"equally timed first Cross on a new pause visit is not suppressed by the old dedup cache");f.activated.clear();
 require(menu.set_entries({f.entry(1),f.entry(2)})==Status::Ok,"second producer accepted without increasing root screen count");f.next(menu);f.drawn.clear();menu.invoke_callback(root_entry);require(f.drawn.back()=="Altri menu","multiple producers share a native overflow entry");
 f.put<unsigned>(Fixture::Root+0x40,root_entry);f.cross(menu,root_entry);require(f.get<unsigned>(Fixture::System+8)==menu.directory_address(),"Cross opens directory through native next-page transition");
 f.put<unsigned>(Fixture::System+4,menu.directory_address());f.put<unsigned>(Fixture::System+8,0);f.next(menu);const auto first=f.get<unsigned>(menu.directory_address()+0x44),second=f.get<unsigned>(menu.directory_address()+0x48);
 require(first&&second&&f.get<unsigned>(first)==0x21bb90&&f.get<unsigned>(menu.directory_address()+0x38)==Fixture::Root,"directory preserves native input/back handlers and root back link");
 f.put<unsigned>(menu.directory_address()+0x40,first);f.cross(menu,first);f.put<unsigned>(menu.directory_address()+0x40,second);f.cross(menu,second);
 require(f.activated.size()==2&&f.activated[0]==f.entry(1).entry.id&&f.activated[1]==f.entry(2).entry.id,"both copied producer callbacks activate once despite duplicate draw passes");
 std::vector<NativePauseMenu::Item> entries;for(unsigned i=1;i<=32;++i)entries.push_back(f.entry(i));require(menu.set_entries(entries)==Status::Ok,"thirty-two producers supported logically");f.activated.clear();f.next(menu);
 for(unsigned page=0;page<5;++page){unsigned pager=0;for(unsigned row=0;row<8;++row){const unsigned slot=row<6?row:row+1,s=f.get<unsigned>(menu.directory_address()+0x44+slot*4);if(!s)continue;f.drawn.clear();menu.invoke_callback(s);f.put<unsigned>(menu.directory_address()+0x40,s);if(!f.drawn.empty()&&f.drawn.back().find("Pagina ")==0)pager=s;else f.cross(menu,s);}
  require(f.get<unsigned>(menu.directory_address()+0x44+6*4)==0,"directory never uses globally Goodies-gated physical slot6");if(page<4){require(pager!=0,"overflow page exposes native pager");f.put<unsigned>(menu.directory_address()+0x40,pager);f.cross(menu,pager);f.next(menu);}}
 require(std::set<EntryId>(f.activated.begin(),f.activated.end()).size()==32,"all thirty-two producers are reachable through native directory pagination");
 f.put<unsigned char>(0x360000+0xc,32);for(unsigned sequence=0;sequence<32;++sequence){const auto seq=0x380000+sequence*0x20;f.put(0x360000+0x48+sequence*4,seq);f.put<unsigned char>(seq+0x10,2);}
 f.put<unsigned>(Fixture::System+4,Fixture::Root);f.next(menu);f.put<unsigned>(Fixture::Root+0x40,root_entry);f.cross(menu,root_entry);f.put<unsigned>(Fixture::System+4,menu.directory_address());f.put<unsigned>(Fixture::System+8,0);f.next(menu);f.put<float>(0x340000+0x54,0.f);
 const auto old_basis=f.get<std::array<float,12>>(0x340000+0xc0);const auto old_position=f.get<std::array<float,4>>(0x340800+0x10);const auto old_corners=f.get<std::array<float,16>>(0x350800);
 menu.before_render();const auto fitted=f.get<std::array<float,12>>(0x340000+0xc0);const auto pager_corners=f.get<std::array<float,16>>(0x350800);
 require((f.get<unsigned short>(0x340600+0x34)&1u)&&!(f.get<unsigned short>(0x340800+0x34)&1u),"native-unused Goodies mesh is hidden while the active directory pager remains visible");
 require(f.get<float>(0x340000+0x54)==1.f&&f.get<float>(0x340000+0x58)==0.f,"directory borrows the proven open Root blend and freezes only its own preview pose");
 require(f.get<unsigned>(0x340800+0x64)==0x370800&&f.get<unsigned>(0x340800+0x68)==0x390600&&f.get<unsigned>(0x340800+0x6c)==0x3a0600,"portable pager pose borrows frame data and preserves its own manipulator pointer");
 require(std::abs(fitted[5]-.875f)<.0001f,"eight-row render fitting transforms the actual physical panel basis");
 require(std::abs((pager_corners[1]+pager_corners[5]+pager_corners[9]+pager_corners[13])*.25f-122.5f)<.001f,"native pose and extra row translation fit the pager into the eighth row");
 menu.after_render();require(f.get<std::array<float,12>>(0x340000+0xc0)==fitted&&(f.get<unsigned short>(0x340600+0x34)&1u),"physical mesh and unused-preview hiding remain leased until the official frontend has captured them");menu.after_capture();require(f.get<std::array<float,12>>(0x340000+0xc0)==old_basis&&f.get<std::array<float,4>>(0x340800+0x10)==old_position&&f.get<std::array<float,16>>(0x350800)==old_corners&&f.get<unsigned short>(0x340600+0x34)==0,"after-capture restores borrowed physical transforms, corners and visibility exactly");
 require(f.get<float>(0x340000+0x54)==0.f,"directory's original interpolation blend is restored after the borrowed render");
 menu.before_render();menu.after_render();menu.after_capture();require(f.get<std::array<float,12>>(0x340000+0xc0)==old_basis,"repeated rendering never compounds nonuniform panel fitting");
 menu.before_render();menu.after_render();auto foreign_basis=f.get<std::array<float,12>>(0x340000+0xc0);foreign_basis[0]=123.f;f.put(0x340000+0xc0,foreign_basis);menu.after_capture();require(f.get<std::array<float,12>>(0x340000+0xc0)==foreign_basis,"deferred restoration preserves a later foreign transform write");f.put(0x340000+0xc0,old_basis);
 const auto original_vars=f.get<unsigned>(0x340000+0x78);f.put<unsigned>(0x340000+0x78,0x351000);f.put<unsigned>(Fixture::System+8,0);menu.before_render();require(menu.borrowed_render_rows()==0&&f.get<unsigned>(Fixture::System+8)==Fixture::Root,"recycled native actor variables invalidate cached Root poses before any borrowed write");menu.after_render();f.put<unsigned>(0x340000+0x78,original_vars);
 f.put<unsigned>(Fixture::System+4,Fixture::Root);const auto old_animations=f.animations;menu.before_render();menu.after_render();require(f.animations==old_animations,"root rendering preserves every native stock opening and closing animation");
 menu.after_capture();f.put<unsigned>(0x340700+0x6c,0xdeadbeef);menu.before_render();require(f.get<unsigned>(0x340700+0x6c)==f.get<unsigned>(0x340600+0x6c)&&f.get<unsigned>(0x340700+0x64)==0x370700,"Root8 extension borrows the opened final stock rig while preserving per-instance manipulators");menu.after_render();menu.after_capture();require(f.get<unsigned>(0x340700+0x6c)==0xdeadbeef,"Root8 stock-rig borrowing restores the original extension data after frontend capture");
 f.put<unsigned>(Fixture::System+0x110,0);require(menu.frame(),"new native pause epoch resets the pose cache");f.tick=0;f.put<unsigned>(Fixture::System+8,0);f.put<unsigned>(Fixture::Root+0x40,root_entry);f.cross(menu,root_entry);require(f.get<unsigned>(Fixture::System+8)==0,"fast Cross waits for a proven fully open Root instead of borrowing a closing pose");menu.before_render();menu.after_render();require(f.get<unsigned>(Fixture::System+8)==0,"an opening Root below twelve native updates cannot qualify the cache");f.put<unsigned>(Fixture::System+0x110,12);menu.before_render();menu.after_render();require(f.get<unsigned>(Fixture::System+8)==menu.directory_address(),"deferred directory opens after the actual Root has rendered a valid open pose");f.tick=12;
 const auto arena_before_visibility=f.arena;for(auto& entry:entries)entry.entry.visible=false;require(menu.set_entries(entries)==Status::Ok,"all producers may become hidden transactionally");f.put<unsigned>(Fixture::Root+0x40,root_entry);f.next(menu);
 require(f.get<unsigned>(Fixture::Root+0x44+7*4)==0&&f.get<unsigned>(Fixture::Root+7*4)==0,"hiding every custom restores unused root screen and original animation");
 require(f.get<unsigned>(Fixture::Buttons+0x38)==Fixture::Buttons+6*0x100&&f.get<unsigned>(Fixture::Buttons+6*0x100+0x3c)==Fixture::Buttons&&f.get<unsigned>(Fixture::Root+0x40)!=root_entry,"all-hidden composition restores the native ring and visible focus");
 for(unsigned slot=0;slot<14;++slot)f.put<int>(0x1b2740+slot*4,1);menu.before_render();require(f.get<int>(0x1b2740+7*4)==0,"live all-hidden root suppresses its retired physical extension panel");menu.after_render();require(f.get<int>(0x1b2740+7*4)==1,"scoped extension visibility restores native renderer flags");
 entries[0].entry.visible=true;require(menu.set_entries(entries)==Status::Ok,"a hidden producer may reappear");f.next(menu);const auto before_reappear=f.animations;menu.before_render();menu.after_render();
 require(f.get<unsigned>(Fixture::Root+0x44+7*4)==root_entry&&f.get<unsigned>(Fixture::Root+7*4)==16&&f.animations==before_reappear&&f.get<unsigned char>(0x340700+0x52)==16,"live reappear borrows a proven pose without resetting any native animation");
 const auto after_reappear=f.animations;menu.before_render();menu.after_render();require(f.animations==after_reappear,"visible steady-state never resets stock or extension animation");
 for(unsigned repeat=0;repeat<20;++repeat){entries[0].entry.visible=false;require(menu.set_entries(entries)==Status::Ok,"visibility hide update");f.next(menu);entries[0].entry.visible=true;require(menu.set_entries(entries)==Status::Ok,"visibility reveal update");f.next(menu);}require(f.arena==arena_before_visibility,"repeated visibility changes allocate no additional guest storage");
 for(auto& entry:entries)entry.entry.visible=false;f.put<unsigned>(Fixture::System+4,menu.directory_address());require(menu.set_entries(entries)==Status::Ok,"directory's last visible producer can disappear");f.next(menu);require(f.get<unsigned>(Fixture::System+8)==Fixture::Root,"empty active directory requests native back to root");
 entries.push_back(f.entry(33));require(menu.set_entries(entries)==Status::Exhausted,"capacity failure preserves the existing directory transactionally");
 f.put<unsigned>(Fixture::Root+0x44+7*4,0xabcdef);menu.reset();require(f.get<unsigned>(Fixture::Root+0x44+7*4)==0xabcdef&&!menu.owns_callback(first)&&f.stack==0x1e00000,"teardown preserves later foreign ownership and balances guest stack");
 {Fixture loading;NativePauseMenu old_ui(&loading.api,[&](std::size_t n){const auto p=loading.arena;loading.arena+=unsigned(n+15)&~15u;return p;});require(old_ui.set_entries({loading.entry(1)})==Status::Ok&&old_ui.bind(0x256268),"old scene presenter initially binds");const auto owned=old_ui.entry_address();old_ui.before_render();old_ui.after_render();const auto incoming=loading.ram;old_ui.reset(false);old_ui.after_capture();require(loading.ram==incoming&&!old_ui.root_address()&&!old_ui.owns_callback(owned),"retiring during WAD reuse drops host leases without restoring or detaching old native storage");}
 verify_transition_leases();
 std::printf("native pause adapter: %u checks passed\n",checks);
}
